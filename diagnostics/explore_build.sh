#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash observer_build.sh
# Same read-only import set, with atomic replacement of the bridge snapshot.
cp build/observer_kernel32.def build/explore_kernel32.def
echo MoveFileExW >> build/explore_kernel32.def
CC="${CC_WINDOWS:-clang}"
LINK="${LINK_WINDOWS:-lld-link}"
"$LINK" /lib /machine:x64 /def:build/explore_kernel32.def /out:build/explore_kernel32.lib
"$CC" --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-ident -funwind-tables -fasynchronous-unwind-tables -Isrc -c src/explore_plugin.c -o build/explore_plugin.obj
"$LINK" /dll /noentry /machine:x64 /nodefaultlib /dynamicbase /highentropyva /nxcompat /opt:ref /opt:icf /timestamp:0 /out:build/RE7_OTS_Explore.dll build/explore_plugin.obj build/observer.obj build/core.obj build/explore_kernel32.lib build/observer_user32.lib
if [[ -f tools/build_explore_lua.py ]]; then python3 tools/build_explore_lua.py; fi
