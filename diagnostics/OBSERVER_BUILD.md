# Build / verify v0.2.0

Linux x86_64: clang 18+, lld-link, host `cc`, Python 3, liblua5.4.

```bash
bash observer_build.sh
RE7_CT_FILE=../reference/original_ct/re7.CT bash observer_test.sh
```

Outputs: `build/RE7_OTS_Observer.dll` (new) and `build/RE7_OTS_Diagnostics.dll` (preserved baseline).
Runtime dependencies: Windows KERNEL32/USER32 only; no Windows SDK/CRT redistributable or Lua native library is linked into observer DLL.
Lua is run by the user's existing REFramework.

`tests/observer_host.c` includes the production wrapper and supplies synthetic Windows APIs. SHA final output is substituted there for known-build session tests; actual SHA behavior is covered by the baseline core vector/differential tests. This is **not** a Windows load or Steam Deck test.
`tests/observer_lua_harness.lua` runs the actual shipped Lua with mocked SDK/filesystem callbacks; it is **not** an RE Engine test. JSON dump serialization there is stubbed.
The observer uses the unchanged public REF prefix ABI and existing REFramework present registration. Only ordinary Win32 reads/own logging are added.

ASan/UBSan:
```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_observer.c src/observer.c src/core.c -o build/test_observer_sanitized
ASAN_OPTIONS=detect_leaks=0 build/test_observer_sanitized
```
LeakSanitizer cannot inspect process tasks in this hosted execution environment. Its disabled leak pass must not be counted as a successful leak check.

The actual build session used locally extracted, SHA-256-checked Ubuntu clang/lld 18.1.3 packages because system package-manager privilege setup failed. Toolchain binaries are not redistributed in the source ZIP.
