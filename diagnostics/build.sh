#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
CC="${CC_WINDOWS:-clang}"
LINK="${LINK_WINDOWS:-lld-link}"
mkdir -p build
cat > build/kernel32.def <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
GetModuleHandleW
GetModuleFileNameW
GetModuleHandleExW
GetCurrentProcess
GetCurrentThread
SetThreadPriority
ReadProcessMemory
VirtualQuery
GetProcessHeap
HeapAlloc
HeapFree
CreateFileW
CreateDirectoryW
ReadFile
WriteFile
CloseHandle
FlushFileBuffers
GetFileSizeEx
SetFilePointerEx
GetLocalTime
GetTickCount64
GetLastError
OutputDebugStringA
CreateThread
DEF
cat > build/user32.def <<'DEF'
LIBRARY USER32.dll
EXPORTS
GetAsyncKeyState
DEF
"$LINK" /lib /machine:x64 /def:build/kernel32.def /out:build/kernel32.lib
"$LINK" /lib /machine:x64 /def:build/user32.def /out:build/user32.lib
FLAGS=(--target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-ident -funwind-tables -fasynchronous-unwind-tables -Isrc)
"$CC" "${FLAGS[@]}" -c src/core.c -o build/core.obj
"$CC" "${FLAGS[@]}" -c src/plugin.c -o build/plugin.obj
"$LINK" /dll /noentry /machine:x64 /nodefaultlib /dynamicbase /highentropyva /nxcompat /opt:ref /opt:icf /timestamp:0 /out:build/RE7_OTS_Diagnostics.dll build/plugin.obj build/core.obj build/kernel32.lib build/user32.lib
printf 'Built: build/RE7_OTS_Diagnostics.dll\n'
