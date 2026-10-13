# Build and verify Lua v0.4.0-dev

From `diagnostics/`, on Linux x86_64 with clang 18+, lld-link, host `cc`, Python 3 and liblua5.4:

```bash
bash explore_build.sh
bash test.sh
```

Outputs: `build/RE7_OTS_Explore.dll`, `build/re7_ots_explore.lua`.
The optional original-CT extraction test is skipped unless a matching private copy is supplied with `RE7_CT_FILE=/absolute/path/to/re7.CT`. The CT archive is not redistributed in Git; normal builds use the checked-in generated manifest.
The build also preserves compilation and testing of the previous observer/diagnostic code. The installer includes only Explore's new DLL and generated autorun Lua. Edit `lua/explore/` and `lua/model/` modules and rebuild; do not edit the generated Lua.

Package from the same directory:

```bash
python3 tools/package_explore.py /absolute/output/directory
```

The packaging command checks ZIP CRCs and compares the installer DLL/Lua against the build outputs. Source archives contain only tracked repository files and exclude generated output. Original credits, prior source and development documentation remain available. Runtime logs and model/game files are not published.

The DLL links only ordinary Windows KERNEL32/USER32 imports. No CRT, Windows SDK redistributable, engine hook or game-memory write function is added. Camera writes use Lua Transform setters at engine callbacks. Opt-in head control also writes the verified head GameObject DrawSelf value; no joints or body flags are changed. The DLL and native protocol remain v0.3.0. Existing REFramework supplies Lua and SDK APIs.

Host tests execute production C with synthetic Win32 boundaries and test-only SHA substitution for supported-build scenarios. Core SHA checks separately compare real outputs against hashlib. Lua 5.4 executes production control/adapter code with fake SDK/JSON/file boundaries. These are not Windows loading or RE Engine execution tests.

Historical v0.3.0 evidence: `evidence/v0.3.0/full_tests_final.txt`, `green_review.txt`, `release_checks.txt`, and `docs/EXPLORE_REVIEW.md`. Actual build used locally extracted clang/lld 18.1.3; the toolchain is not redistributed.

Current model binding analysis and verification ledger: `../docs/ETHAN_RT_MODEL_ANALYSIS_KO.md` and
`../docs/superpowers/plans/2026-10-13-ethan-head-binding.md`. Runtime rendering is pending.
