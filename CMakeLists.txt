cmake_minimum_required(VERSION 3.21)

project(DiscordRPC_NoCRT LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "This no-CRT proof targets Windows with MSVC.")
endif()

add_executable(discord_rpc_nocrt
    src/main.cpp
)

target_compile_features(discord_rpc_nocrt PRIVATE cxx_std_20)
target_compile_definitions(discord_rpc_nocrt PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

# Avoid compiler features that silently introduce CRT helpers.
target_compile_options(discord_rpc_nocrt PRIVATE
    /W4
    /WX
    /GS-
    /GR-
    /EHs-c-
    /Oi
    /Zl
)

target_link_options(discord_rpc_nocrt PRIVATE
    /NODEFAULTLIB
    /ENTRY:mainCRTStartup
    /SUBSYSTEM:CONSOLE
)

target_link_libraries(discord_rpc_nocrt PRIVATE kernel32.lib user32.lib)

