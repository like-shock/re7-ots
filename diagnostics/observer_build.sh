#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
# Build the preserved baseline too; it supplies generated import libraries.
bash build.sh
cat > build/observer_kernel32.def <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
GetModuleHandleW
GetModuleFileNameW
GetModuleHandleExW
GetCurrentProcess
GetCurrentThread
GetCurrentProcessId
SetThreadPriority
ReadProcessMemory
CreateFileW
CreateDirectoryW
ReadFile
WriteFile
CloseHandle
FlushFileBuffers
GetFileSizeEx
GetLocalTime
GetTickCount64
GetLastError
CreateThread
Sleep
DEF
cat > build/observer_user32.def <<'DEF'
LIBRARY USER32.dll
EXPORTS
GetAsyncKeyState
GetForegroundWindow
GetWindowThreadProcessId
DEF
CC="${CC_WINDOWS:-clang}"
LINK="${LINK_WINDOWS:-lld-link}"
"$LINK" /lib /machine:x64 /def:build/observer_kernel32.def /out:build/observer_kernel32.lib
"$LINK" /lib /machine:x64 /def:build/observer_user32.def /out:build/observer_user32.lib
FLAGS=(--target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-ident -funwind-tables -fasynchronous-unwind-tables -Isrc)
"$CC" "${FLAGS[@]}" -c src/observer.c -o build/observer.obj
"$CC" "${FLAGS[@]}" -c src/observer_plugin.c -o build/observer_plugin.obj
"$LINK" /dll /noentry /machine:x64 /nodefaultlib /dynamicbase /highentropyva /nxcompat /opt:ref /opt:icf /timestamp:0 /out:build/RE7_OTS_Observer.dll build/observer_plugin.obj build/observer.obj build/core.obj build/observer_kernel32.lib build/observer_user32.lib
