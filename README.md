# Discord RPC — No CRT proof

A tiny Windows C++ proof that publishes a fully populated Discord Rich Presence by speaking Discord's local IPC protocol directly. It uses no C runtime, no C++ runtime, and no third-party library.

Repository: <https://github.com/NihonFemStyle/DiscordRPC-NoCRT>

## What the activity fills

- Details and state
- Start and end timestamps
- Large image plus hover text
- Small image plus hover text
- Party ID, current size, and maximum size
- Match, join, and spectate secrets (secrets profile)
- Two buttons (buttons profile)
- Instance flag and activity type

Discord rejects activities containing secrets and buttons together, so the proof rotates between two otherwise fully populated, valid profiles every 15 seconds. The console shows each accepted response. The application name and default icon are controlled by the Discord Developer Portal and cannot be overridden locally.

## One-time Discord setup

The application ID is already set to `1054371405518610502`.

In the Discord Developer Portal, open that application and upload two Rich Presence art assets with these exact keys:

- `example_large`
- `example_small`

Without those uploads, Discord may omit or reject the image fields. Replace the sample button URLs and secret values in `src/main.cpp` if you want them to perform real actions. Join/spectate secrets prove serialization of those slots; implementing the corresponding invite/join workflow is outside this Hello World proof.

## Build

Use a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Then start the desktop Discord client and run:

```powershell
.\build\Release\discord_rpc_nocrt.exe
```

The program opens a console that prints `Hello RichPresence` and streams its IPC activity in real time: pipe discovery, handshake and READY payload, activity response, ping/pong traffic, errors, and disconnects. It remains active to keep the presence alive. Press Ctrl+C, close the console, or end the process to clear the activity.

## Verify the no-CRT claim

```powershell
dumpbin /dependents .\build\Release\discord_rpc_nocrt.exe
```

The dependency list should contain only Windows system DLLs (normally `KERNEL32.dll` and `USER32.dll`) and no `VCRUNTIME`, `MSVCP`, or Universal CRT DLLs.

