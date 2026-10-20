# Build and verify Lua v0.6.0-dev / native v0.4.0

From `diagnostics/`, on Linux x86_64 with clang 18+, lld-link, host `cc`, Python 3, liblua5.4 and `gcc-mingw-w64-x86-64-posix`:

```bash
bash explore_build.sh
bash test.sh
```

Outputs: `build/RE7_OTS_Explore.dll`, `build/re7_ots_explore.lua`.
The optional original-CT extraction test is skipped unless a matching private copy is supplied with `RE7_CT_FILE=/absolute/path/to/re7.CT`. The CT archive is not redistributed in Git; normal builds use the checked-in generated manifest.
The build also preserves compilation and testing of the previous observer/diagnostic code. The installer includes only Explore's new DLL and generated autorun Lua. Edit `lua/explore/` and `lua/model/` modules and rebuild; do not edit the generated Lua.

Commit tested source changes, then package from the same directory:

```bash
python3 tools/package_explore.py /absolute/output/directory
```

The packaging command rejects uncommitted tracked changes and a generated Lua bundle that differs
from the source modules. It checks ZIP CRCs and compares every archived payload with its inputs.
Both ZIPs include `BUILD_INFO.json` (source commit, source tree, version) and complete `SHA256SUMS`.
Fixed ZIP timestamps, order and permissions make packaging the same inputs repeatable with the same
Python/zlib toolchain. Source archives contain only tracked repository files and exclude build output.
Original credits, prior source and development documentation remain available. Runtime logs and
model/game files are not published. The metadata identifies the packaging source; it does not prove
that an arbitrary pre-existing DLL was built from that commit. Always run `test.sh` first, as CI does.

## GitHub automation

`.github/workflows/offline-build.yml` builds on Ubuntu 24.04 for pushes to `work`/`main` and pull
requests targeting those branches. It installs clang/lld 18, MinGW x64 GCC and Lua 5.4, runs the same complete
`test.sh`, then packages only after success. Actions are pinned to full commit IDs. No private CT,
game or model files, repository write permission, deployment credentials or Steam Deck are needed.

Open the repository's **Actions → Offline build and package → successful run → Artifacts** to
download `re7-ots-<commit>`. It contains the installer ZIP, source ZIP and outer SHA256SUMS. Test
output is uploaded separately even when tests fail. Artifacts are retained for 14 days; this is
not a permanent GitHub Release. Manual dispatch becomes available when the workflow exists on
the default branch; automatic pushes to `work` do not require merging to `main`.

Explore links Windows KERNEL32/USER32 and system msvcrt imports. A pinned Lua 5.4.3 core is
statically linked, following the official REFramework plugin example. `tools/fetch_lua.py` verifies
its archive SHA-256 before extracting source files into `build/`. Its license is retained in NOTICE.
`CC_NATIVE_WINDOWS` overrides the MinGW compiler; `CC_WINDOWS` and `LINK_WINDOWS` still select
clang/lld for preserved diagnostics and the explicit math import library. The Explore build disables
auto-import and runtime pseudo-relocation, has no CRT startup/DllMain, and uses direct CRT math thunks.
There are no engine hooks, executable patches, dynamic loaders, network APIs or native game writes.
The host supplies the existing Lua allocator; no separate Lua DLL is installed.

Native API 1 exposes `re7_ots_native.read()` (bounded JSON, no files) and `clock()` (GetTickCount64
seconds). The script polls on the existing 50 ms cadence. A background validator publishes immutable
build metadata with release/acquire atomics; separate read-only sampling states serve direct and
legacy consumers. Same-tick reads reuse one sequence/snapshot. Direct sampling is nonblocking and
releases its atomic flag before Lua calls, including any allocation. Native version is 0.4.0;
snapshot protocol remains 0.3.0. Readiness, health and replay thresholds remain unchanged.

Lua's `_VERSION` check rejects clearly incompatible states; it does not prove patch-level internal
ABI compatibility. The pinned lua.h/luaconf.h/lstate.h/lobject.h/ldo.c match REFramework reference
`d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee` byte for byte. Custom Lua builds are unverified.
Existing REFramework supplies SDK APIs and game callbacks. Camera/model writes remain in Lua.


Host tests execute production C with synthetic Win32 boundaries and test-only SHA substitution for supported-build scenarios. Core SHA checks separately compare real outputs against hashlib. Lua 5.4 executes production control/adapter code with fake SDK/JSON/file boundaries. These are not Windows loading or RE Engine execution tests.

Historical v0.3.0 evidence: `evidence/v0.3.0/full_tests_final.txt`, `green_review.txt`, `release_checks.txt`, and `docs/EXPLORE_REVIEW.md`. Actual build used locally extracted clang/lld 18.1.3; the toolchain is not redistributed.

Historical Ethan binding analysis and verification ledger: `../docs/ETHAN_RT_MODEL_ANALYSIS_KO.md` and
`../docs/superpowers/plans/2026-10-13-ethan-head-binding.md`. Runtime rendering is pending.

Current offline stabilization: `../docs/OFFLINE_PROGRESS_KO.md`.

Current integrated runtime: `../docs/RUNTIME_EXPANSION_KO.md` and
`../docs/superpowers/plans/2026-10-14-runtime-expansion.md`. Lua bundles the guarded physics adapter,
scalar obstacle limiter and per-part model group. Physics queries never write game memory.
