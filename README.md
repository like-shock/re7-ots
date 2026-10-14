# RE7 OTS

Experimental over-the-shoulder camera for Resident Evil 7, built with REFramework for Steam Deck.

## Development version: Explore Lua v0.4.1-dev

- F6 toggles the exploration camera; every script load starts OFF.
- Adjustable camera distance, shoulder offset and height.
- First-person fallback when aim, inventory, interaction or uncertain runtime state is detected.
- Opt-in Ethan RT head mesh visibility tied to verified camera placement (experimental).
- F6 OFF remains effective during native outages, focus loss or an open menu.
- Bounded camera/model transition events, periodic logs and an offline log summary tool.
- Native state monitoring is read-only. Lua deliberately changes the current camera position through REFramework.

**User-observed status:** body visible in third person; head/face absent. Intermittent first-person fallback reported during walking/stairs. Its cause is still unknown; obstruction by a pillar is an unconfirmed hypothesis. Aiming has not been tested with a gun.

This prototype has no wall collision, verified in-game head support, third-person ballistics or DLC validation. It is not a complete port of the original Cheat Engine script.

## 설치 / 사용

[한국어 설치·플레이 안내](diagnostics/EXPLORE_README_KO.md)

기존에 작동하던 REFramework 환경에서 사용합니다. 이전 Observer/Diagnostics DLL과 Lua는 게임 종료 후 별도 폴더에 보관하고, Explore의 DLL과 Lua를 설치하세요. 게임 실행마다 기본 OFF이며 F6으로 켜고 끕니다. 별도 진단 단계 없이 플레이 로그가 자동으로 남습니다.

## Build and test

Linux x86_64 dependencies: clang 18+, lld-link, host C compiler, Python 3, liblua5.4.

```bash
cd diagnostics
bash test.sh
python3 tools/package_explore.py ../dist
```

`test.sh` builds the DLLs and generated Lua, then runs the tests. The optional original-CT re-extraction test requires your own matching `re7.CT` via `RE7_CT_FILE=/absolute/path/to/re7.CT`; otherwise that one test is skipped. Generated reference metadata is already included for normal builds.

Commit tested changes before packaging. Packages include the source commit/tree and full file
hashes; uncommitted tracked changes and stale generated Lua are rejected. GitHub Actions runs the
same build/tests and produces installer/source artifacts for `work`/`main` pushes and pull requests.
Download them from [Actions](https://github.com/like-shock/re7-ots/actions/workflows/offline-build.yml)
after a successful run; artifacts are kept for 14 days. See the [automation notes](diagnostics/EXPLORE_BUILD.md).

Outputs: `diagnostics/build/RE7_OTS_Explore.dll` and `diagnostics/build/re7_ots_explore.lua`. See [build notes](diagnostics/EXPLORE_BUILD.md) and [design](diagnostics/docs/EXPLORE_DESIGN.md).

The `diagnostics/` directory name is retained from the prototype's history. It now contains the active Explore source as well as earlier diagnostic/observer code and regression tests. Historical design/progress documents describe their respective versions; current repository instructions take precedence.

## Development

`work` now bundles an experimental binding for the supplied **Ethan with Head RT** model.
It selects the separate head mesh under the local player, hides it before first-person fallback,
and shows it only after camera readback and head clearance checks. The model option defaults OFF;
install the external model separately before enabling it. Native DLL/protocol remains v0.3.0.
See [오프라인 개발 완료 범위](docs/OFFLINE_PROGRESS_KO.md), [진행 내용](docs/MODEL_SUPPORT_PROGRESS_KO.md) and [asset analysis](docs/ETHAN_RT_MODEL_ANALYSIS_KO.md).

`main` holds the verified source baseline. Ongoing development uses `work` or focused feature branches. Keep user runtime logs, game executables and third-party model archives out of Git. Build artifacts are generated locally or by CI; this workflow does not publish a GitHub Release.

The first import preserves the shipped v0.3.0 runtime source. Development tests simulate the SDK/OS boundaries and do not prove in-game rendering. Further Steam Deck testing can wait until an integrated change is ready.

## Credits

Original Third-Person OTS Script: cheezeit / goldedtoad. REFramework: praydog and contributors. Model authors and original acknowledgements are preserved in [ORIGINAL_CREDITS.txt](diagnostics/ORIGINAL_CREDITS.txt). See [NOTICE](diagnostics/NOTICE.md).

The project's original implementation is MIT licensed; original mod-derived reference data and external model assets retain their own terms. The game, REFramework binaries, original CT archive and model assets are not redistributed in this repository.
