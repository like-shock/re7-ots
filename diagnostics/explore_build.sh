#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash observer_build.sh
python3 tools/fetch_lua.py
CC="${CC_NATIVE_WINDOWS:-x86_64-w64-mingw32-gcc-posix}"
FLAGS=(-std=c11 -O2 -Wall -Wextra -fno-ident -D__USE_MINGW_ANSI_STDIO=0 -D__NO_ISOCEXT=1 -fno-stack-protector -ffunction-sections -fdata-sections -Isrc -Ibuild/lua-5.4.3/src)
mkdir -p build/native-lua
# Core only: no standard libraries, package loader, dynamic linker or Lua CLI.
LUA_CORE=(lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio)
OBJECTS=()
for name in "${LUA_CORE[@]}"; do
    "$CC" "${FLAGS[@]}" -c "build/lua-5.4.3/src/$name.c" -o "build/native-lua/$name.o"
    OBJECTS+=("build/native-lua/$name.o")
done
for name in explore_plugin native_lua observer core; do
    "$CC" "${FLAGS[@]}" -Werror -c "src/$name.c" -o "build/native-lua/$name.o"
    OBJECTS+=("build/native-lua/$name.o")
done
# MinGW's default math import entries expect libmingwex wrappers. Generate
# ordinary function thunks instead; never rely on CRT startup/pseudo-relocation.
cat > build/native_msvcrt.def <<'DEF'
LIBRARY msvcrt.dll
EXPORTS
pow
ldexp
fmod
DEF
"${LINK_WINDOWS:-lld-link}" /lib /machine:x64 /def:build/native_msvcrt.def /out:build/native_msvcrt.lib
# No CRT startup/DllMain. Only the two explicit REFramework entry exports survive.
"$CC" -shared -nostdlib -Wl,--entry,0 -Wl,--exclude-all-symbols \
    -Wl,--disable-auto-import -Wl,--disable-runtime-pseudo-reloc -Wl,--gc-sections -Wl,--no-insert-timestamp -Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--nxcompat \
    -o build/RE7_OTS_Explore.dll "${OBJECTS[@]}" build/native_msvcrt.lib -lmsvcrt -luser32 -lkernel32 -lgcc
python3 tools/build_explore_lua.py
