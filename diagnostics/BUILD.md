# Build and verify

The installed package is already built. This source tree is for inspection/rebuilding, not an extra Steam Deck installation step.

## Build on Linux with LLVM

Requirements: `clang`, `lld-link`, Bash. No Windows SDK, MinGW, external C runtime, or network fetch is required. The small Win32 and REFramework prefix ABI declarations are included in `src/`. LLVM creates import libraries from the export names in `build.sh`.

```bash
./build.sh
# build/RE7_OTS_Diagnostics.dll
```

Compiler target: `x86_64-pc-windows-msvc`. DLL has no CRT entry point, no dynamic C/C++ initialization, no TLS, and no third-party runtime dependencies. Windows x64 unwind information and base relocations are retained. `/timestamp:0` makes the binary deterministic for identical toolchain/source. PE timestamp 1970 in a file inspector is intentional, not a release date.

The REFramework ABI major/minor requested is 1.0 (plugin API, not REFramework product version). Only the early `on_present` callback and log function slots are used. This does not assert every old REFramework/Proton release was tested. Minimal signatures were verified against the public `include/reframework/API.h`.

## Tests on Linux

Requirements: host C compiler `cc`, Python 3, LLVM for the Windows build. `liblua5.4` is optional for Lua status UI tests; a missing library is explicitly skipped.

```bash
./test.sh
```

This executes native core tests, differential tests, a synthetic Win32 host around the same plugin code, Lua UI with stub bindings, and inspection of the produced Windows PE DLL. The synthetic host executes the C implementation on Linux; it is **not** an actual Windows DLL load, RE7, SteamOS, or Proton test.

The extraction test uses the original CT only when available. It is not redistributed here:

```bash
RE7_CT_FILE=/path/to/re7.CT ./test.sh
python3 tools/extract_manifest.py /path/to/re7.CT
```

The immutable source SHA-256 must match the uploaded RT 8.02 CT. Generated `data/patch_manifest.json` and `src/manifest.h` are included, so original CT access is not needed to build or use the diagnostic DLL.

`evidence/final_tests.txt` records the actual build-session results. Earlier `red_*` files record pre-implementation failures, not unresolved failures in the delivered version.

## Review boundaries

No CT assembly is run. No `WriteProcessMemory`, `VirtualProtect`, remote thread, engine setter or hook installation APIs are called. `CreateThread` creates only a local worker. The diagnostic registers a normal REFramework present callback to start a scan/poll F9. `WriteFile` writes only its own reports/status. Patterns/signatures come from the original user-supplied CT; see credits and NOTICE.

Limitations: disk scan is section-raw-data only; in-memory scan is readable committed module spans only. Full pointer semantics, actual x64 game register contexts, mod conflict ownership, hot reload and real Steam Deck execution are not tested. The DLL is pinned until process exit. Native plugin unload/reload while running is not supported; quit the game to remove/replace it.
