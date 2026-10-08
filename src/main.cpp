#include <windows.h>
#include <stdint.h>

namespace {

constexpr char kApplicationId[] = "1054371405518610502";
constexpr DWORD kOpcodeHandshake = 0;
constexpr DWORD kOpcodeFrame = 1;
constexpr DWORD kOpcodeClose = 2;
constexpr DWORD kOpcodePing = 3;
constexpr DWORD kOpcodePong = 4;
constexpr DWORD kMaxPayload = 16 * 1024;

struct FrameHeader {
    DWORD opcode;
    DWORD length;
};

char g_readBuffer[kMaxPayload + 1];
char g_activityBuffer[kMaxPayload];

unsigned long textLength(const char* text);
char* appendUnsigned(char* out, DWORD value);

void printText(const char* text) {
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, textLength(text), &written, nullptr);
}

void printLine(const char* text) {
    printText(text);
    printText("\r\n");
}

void printFrame(const char* label, DWORD opcode, const char* payload) {
    printText(label);
    printText(" opcode=");
    char number[11];
    char* end = appendUnsigned(number, opcode);
    *end = 0;
    printText(number);
    printText(" payload=");
    printLine(payload);
}

unsigned long textLength(const char* text) {
    unsigned long length = 0;
    while (text[length] != 0) ++length;
    return length;
}

bool writeAll(HANDLE pipe, const void* data, DWORD size) {
    const BYTE* cursor = static_cast<const BYTE*>(data);
    while (size != 0) {
        DWORD written = 0;
        if (!WriteFile(pipe, cursor, size, &written, nullptr) || written == 0) return false;
        cursor += written;
        size -= written;
    }
    return true;
}

bool readAll(HANDLE pipe, void* data, DWORD size) {
    BYTE* cursor = static_cast<BYTE*>(data);
    while (size != 0) {
        DWORD read = 0;
        if (!ReadFile(pipe, cursor, size, &read, nullptr) || read == 0) return false;
        cursor += read;
        size -= read;
    }
    return true;
}

bool sendFrame(HANDLE pipe, DWORD opcode, const char* json) {
    const DWORD length = textLength(json);
    const FrameHeader header{opcode, length};
    return writeAll(pipe, &header, sizeof(header)) && writeAll(pipe, json, length);
}

bool receiveFrame(HANDLE pipe, DWORD* opcode) {
    FrameHeader header{};
    if (!readAll(pipe, &header, sizeof(header)) || header.length > kMaxPayload) return false;
    if (!readAll(pipe, g_readBuffer, header.length)) return false;
    g_readBuffer[header.length] = 0;
    *opcode = header.opcode;
    return true;
}

bool containsText(const char* text, const char* needle) {
    if (*needle == 0) return true;
    for (; *text != 0; ++text) {
        const char* left = text;
        const char* right = needle;
        while (*left != 0 && *right != 0 && *left == *right) {
            ++left;
            ++right;
        }
        if (*right == 0) return true;
    }
    return false;
}

HANDLE connectToDiscord() {
    wchar_t path[] = L"\\\\?\\pipe\\discord-ipc-0";
    for (wchar_t index = L'0'; index <= L'9'; ++index) {
        path[21] = index;
        printText("[ipc] Trying discord-ipc-");
        char suffix[2]{static_cast<char>(index), 0};
        printLine(suffix);
        HANDLE pipe = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            printText("[ipc] Connected to discord-ipc-");
            printLine(suffix);
            return pipe;
        }
    }
    return INVALID_HANDLE_VALUE;
}

char* appendText(char* out, const char* text) {
    while (*text != 0) *out++ = *text++;
    return out;
}

char* appendUnsigned(char* out, DWORD value) {
    char reversed[10];
    DWORD count = 0;
    do {
        reversed[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0);
    while (count != 0) *out++ = reversed[--count];
    return out;
}

void showError(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"Discord RPC — No CRT", MB_OK | MB_ICONERROR);
}

bool sendActivity(HANDLE pipe, bool buttonsProfile) {
    constexpr char activityPrefix[] =
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    constexpr char commonFields[] =
        ",\"activity\":{"
        "\"type\":0,"
        "\"details\":\"Hello World from native C++\","
        "\"timestamps\":{\"start\":1704067200,\"end\":4102444800},"
        "\"assets\":{"
          "\"large_image\":\"example_large\","
          "\"large_text\":\"Large image hover text\","
          "\"small_image\":\"example_small\","
          "\"small_text\":\"Small image hover text\"},"
        "\"party\":{\"id\":\"nocrt-party-001\",\"size\":[1,4]},";
    constexpr char secretsFields[] =
        "\"state\":\"No CRT · secrets profile\","
        "\"secrets\":{"
          "\"match\":\"match-secret-example\","
          "\"join\":\"join-secret-example\","
          "\"spectate\":\"spectate-secret-example\"},";
    constexpr char buttonFields[] =
        "\"state\":\"No CRT · buttons profile\","
        "\"buttons\":["
          "{\"label\":\"Discord Developer Portal\",\"url\":\"https://discord.com/developers/applications/1054371405518610502\"},"
          "{\"label\":\"View GitHub Repository\",\"url\":\"https://github.com/NihonFemStyle/DiscordRPC-NoCRT\"}],";
    constexpr char activityEnd[] =
        "\"instance\":true}},\"nonce\":\"nocrt-activity-update\"}";

    char* cursor = appendText(g_activityBuffer, activityPrefix);
    cursor = appendUnsigned(cursor, GetCurrentProcessId());
    cursor = appendText(cursor, commonFields);
    cursor = appendText(cursor, buttonsProfile ? buttonFields : secretsFields);
    cursor = appendText(cursor, activityEnd);
    *cursor = 0;

    printLine(buttonsProfile
        ? "[rpc] Activating buttons profile (secrets excluded by Discord)"
        : "[rpc] Activating secrets profile (buttons excluded by Discord)");
    if (!sendFrame(pipe, kOpcodeFrame, g_activityBuffer)) {
        printLine("[error] Failed to send SET_ACTIVITY");
        return false;
    }
    DWORD opcode = 0;
    if (!receiveFrame(pipe, &opcode)) {
        printLine("[error] Failed to receive SET_ACTIVITY response");
        return false;
    }
    printFrame("[recv]", opcode, g_readBuffer);

    if (opcode != kOpcodeFrame || containsText(g_readBuffer, "\"evt\":\"ERROR\"")) {
        printLine("[error] Discord rejected SET_ACTIVITY");
        return false;
    }
    printLine(buttonsProfile
        ? "[rpc] Buttons profile is active"
        : "[rpc] Secrets profile is active");
    return true;
}

bool runRpc(HANDLE pipe) {
    constexpr char handshake[] =
        "{\"v\":1,\"client_id\":\"1054371405518610502\"}";

    printLine("[rpc] Sending handshake");
    if (!sendFrame(pipe, kOpcodeHandshake, handshake)) {
        printLine("[error] Failed to send handshake");
        return false;
    }

    DWORD opcode = 0;
    if (!receiveFrame(pipe, &opcode)) {
        printLine("[error] Failed to receive handshake response");
        return false;
    }
    printFrame("[recv]", opcode, g_readBuffer);
    if (opcode != kOpcodeFrame || !containsText(g_readBuffer, "\"evt\":\"READY\"")) {
        printLine("[error] Discord did not return a READY event");
        return false;
    }

    printLine("[rpc] Discord forbids secrets and buttons in one activity; rotating both valid profiles every 15 seconds");
    return sendActivity(pipe, false);
}

} // namespace

extern "C" void WINAPI mainCRTStartup() {
    printLine("Hello RichPresence");
    printLine("[startup] Discord RPC NoCRT proof started");
    HANDLE pipe = connectToDiscord();
    if (pipe == INVALID_HANDLE_VALUE) {
        printLine("[error] No Discord IPC pipe was found");
        showError(L"Discord is not running, or no discord-ipc pipe was found.");
        ExitProcess(1);
    }

    if (!runRpc(pipe)) {
        CloseHandle(pipe);
        showError(L"Discord rejected the Rich Presence update or disconnected.");
        ExitProcess(2);
    }

    bool buttonsProfile = false;
    ULONGLONG nextRotation = GetTickCount64() + 15000;
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
            printLine("[ipc] Discord disconnected");
            break;
        }
        if (available >= sizeof(FrameHeader)) {
            DWORD opcode = 0;
            if (!receiveFrame(pipe, &opcode)) {
                printLine("[ipc] Discord disconnected");
                break;
            }
            printFrame("[recv]", opcode, g_readBuffer);
            if (opcode == kOpcodePing) {
                printLine("[send] PONG");
                if (!sendFrame(pipe, kOpcodePong, g_readBuffer)) {
                    printLine("[error] Failed to send PONG");
                    break;
                }
            }
            if (opcode == kOpcodeClose) {
                printLine("[ipc] Discord requested close");
                break;
            }
        }
        if (GetTickCount64() >= nextRotation) {
            buttonsProfile = !buttonsProfile;
            if (!sendActivity(pipe, buttonsProfile)) break;
            nextRotation = GetTickCount64() + 15000;
        }
        Sleep(100);
    }

    CloseHandle(pipe);
    printLine("[shutdown] Rich Presence stopped");
    ExitProcess(0);
}

