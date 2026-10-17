#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/fetch_minhook.py
CC="${CC_NATIVE_WINDOWS:-x86_64-w64-mingw32-gcc-posix}"
mkdir -p build/probe
REVISION=$(git -C .. rev-parse HEAD)
FLAGS=("-DPROBE_SOURCE_COMMIT=\"$REVISION\"" -std=c11 -O2 -Wall -Wextra -fno-ident -ffp-contract=off -fno-fast-math -ffunction-sections -fdata-sections -Isrc -Ibuild/minhook/include)
OBJECTS=()
for name in core observer original_scalar probe_capture probe_lifecycle probe_plugin; do
 "$CC" "${FLAGS[@]}" -Werror -c "src/$name.c" -o "build/probe/$name.o"
 OBJECTS+=("build/probe/$name.o")
done
"$CC" -Isrc -c src/probe_capture.S -o build/probe/detours.o
OBJECTS+=(build/probe/detours.o)
for name in buffer hook trampoline hde/hde64; do
 obj="build/probe/mh_${name##*/}.o"
 "$CC" "${FLAGS[@]}" -c "build/minhook/src/$name.c" -o "$obj"
 OBJECTS+=("$obj")
done
"$CC" -shared -static-libgcc -Wl,--exclude-all-symbols -Wl,--gc-sections -Wl,--no-insert-timestamp \
 -Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--nxcompat -o build/RE7_OTS_NativeProbe.dll "${OBJECTS[@]}" -luser32 -lkernel32

python3 - <<'PYBUILD'
import hashlib,json,pathlib,subprocess
root=pathlib.Path('.')
names=['src/'+name for name in ('probe_capture.c','probe_capture.h','probe_capture.S','probe_lifecycle.c','probe_lifecycle.h','probe_plugin.c','probe_sites.h','original_scalar.c','original_scalar.h','observer.c','observer.h','core.c','core.h','ref_min.h')]+['probe_build.sh','data/minhook.json','tools/fetch_minhook.py']
info={'schema':1,'version':'0.1.0','mode':'PASS_THROUGH_HOOK_PROBE',
'commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
'dirty':bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],text=True)),
'sources':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in names},
'dll_sha256':hashlib.sha256((root/'build/RE7_OTS_NativeProbe.dll').read_bytes()).hexdigest(),
'minhook_commit':'c3fcafdc10146beb5919319d0683e44e3c30d537'}
(root/'build/probe-build.json').write_text(json.dumps(info,indent=2,sort_keys=True)+'\n')
PYBUILD
