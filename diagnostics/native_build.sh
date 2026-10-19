#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/fetch_minhook.py
python3 tools/fetch_lua.py
NATIVE_CC="${CC_NATIVE_WINDOWS:-x86_64-w64-mingw32-gcc-posix}"
mkdir -p build/native
NATIVE_REVISION=$(git -C .. rev-parse HEAD)
# Capture inputs before compiling; the final receipt rejects a source changed
# during this build instead of attributing newer bytes to older object files.
python3 - <<'PYINPUTS'
import hashlib,json,pathlib
root=pathlib.Path('.')
paths=[p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in {'.c','.h','.S'}]
paths += [root/p for p in ('native_build.sh','data/minhook.json','tools/fetch_minhook.py','tools/fetch_lua.py')]
(root/'build/native-inputs-before.json').write_text(json.dumps(
    {p.as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},sort_keys=True)+'\n')
PYINPUTS
NATIVE_FLAGS=("-DNATIVE_SOURCE_COMMIT=\"$NATIVE_REVISION\"" -std=c11 -O2 -Wall -Wextra -fno-ident -ffp-contract=off -fno-fast-math -ffunction-sections -fdata-sections -Isrc -Ibuild/minhook/include -Ibuild/lua-5.4.3/src)
NATIVE_OBJECTS=()
for name in core observer original_scalar original_state original_collision original_interaction original_model original_inputs native_bridge native_head native_av native_screen native_runtime native_options native_options_ui native_plugin; do
 "$NATIVE_CC" "${NATIVE_FLAGS[@]}" -Werror -MMD -MF "build/native/$name.d" -c "src/$name.c" -o "build/native/$name.o"
 NATIVE_OBJECTS+=("build/native/$name.o")
done
LUA_CORE=(lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio)
for name in "${LUA_CORE[@]}"; do
 "$NATIVE_CC" "${NATIVE_FLAGS[@]}" -MMD -MF "build/native/lua_$name.d" -c "build/lua-5.4.3/src/$name.c" -o "build/native/lua_$name.o"
 NATIVE_OBJECTS+=("build/native/lua_$name.o")
done
"$NATIVE_CC" -Isrc -MMD -MF build/native/detours.d -c src/native_bridge.S -o build/native/detours.o
NATIVE_OBJECTS+=(build/native/detours.o)
for name in buffer hook trampoline hde/hde64; do
 obj="build/native/mh_${name##*/}.o"
 "$NATIVE_CC" "${NATIVE_FLAGS[@]}" -MMD -MF "${obj%.o}.d" -c "build/minhook/src/$name.c" -o "$obj"
 NATIVE_OBJECTS+=("$obj")
done
"$NATIVE_CC" -shared -static-libgcc -Wl,--exclude-all-symbols -Wl,--gc-sections -Wl,--no-insert-timestamp \
 -Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--nxcompat -o build/RE7_OTS_Native.dll "${NATIVE_OBJECTS[@]}" -luser32 -lkernel32

python3 - "$NATIVE_REVISION" <<'PYBUILD'
import hashlib,json,pathlib,shlex,subprocess,sys
sys.path.insert(0,'tools')
import fetch_lua
root=pathlib.Path('.').resolve()
names={'native_build.sh','data/minhook.json','tools/fetch_minhook.py','tools/fetch_lua.py'}
# GCC dependencies include every actually used local source/header, including
# native_sites.h and pinned MinHook inputs. Store paths relative to diagnostics.
for depfile in (root/'build/native').glob('*.d'):
    body=depfile.read_text().replace('\\\n',' ')
    dependencies=shlex.split(body.split(':',1)[1])
    for dependency in dependencies:
        path=(root/dependency).resolve()
        name=path.relative_to(root).as_posix()
        if not name.startswith(('build/minhook/','build/lua-5.4.3/')):
            names.add(name)
manifest=json.loads((root/'data/minhook.json').read_text())
minhook_sources={name:hashlib.sha256((root/'build/minhook'/name).read_bytes()).hexdigest()
                 for name in manifest['files']}
assert minhook_sources==manifest['files'], 'Pinned MinHook input changed during build'
commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
assert commit==sys.argv[1], 'HEAD changed during native build; rebuild from the final commit'
sources={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in sorted(names)}
before=json.loads((root/'build/native-inputs-before.json').read_text())
changed=[name for name,digest in sources.items() if before.get(name)!=digest]
assert not changed, 'Sources changed during native build: '+', '.join(changed)
info={'schema':1,'version':'0.1.7','mode':'ORIGINAL_NATIVE_OTS_EXPERIMENTAL',
      'commit':commit,'source_commit':commit,
      'dirty':bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=normal'],text=True)),
      'game_data_writes':True,'game_writes':True,'code_patches':True,'face_hiding':True,
      'sources':sources,
      'dll_sha256':hashlib.sha256((root/'build/RE7_OTS_Native.dll').read_bytes()).hexdigest(),
      'minhook_commit':manifest['commit'],'minhook_sources':minhook_sources,
      'lua_version':fetch_lua.VERSION,'lua_archive_sha256':fetch_lua.SHA256,
      'lua_sources':fetch_lua.source_hashes(root/'build')}
(root/'build/native-build.json').write_text(json.dumps(info,indent=2,sort_keys=True)+'\n')
PYBUILD
