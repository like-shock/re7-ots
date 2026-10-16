# RE7 OTS

Experimental over-the-shoulder camera for Resident Evil 7, built with REFramework for Steam Deck.

## Development version: Explore Lua v0.5.2-dev

- F6 toggles the exploration camera; every script load starts OFF.
- Adjustable camera distance, shoulder offset and height; F7 swaps shoulders.
- Configurable aim-release hold and camera return blend.
- First-person fallback when aim, inventory, interaction or uncertain runtime state is detected.
- Opt-in RT head visibility for eight model families, including separate Mia face/hair ownership.
- Optional experimental center-ray obstacle avoidance with immediate contraction and gradual recovery.
- Collision fallback uses separate entry/release distances and 0.4 seconds of stable space before recovery.
- F6 OFF remains effective during native outages, focus loss or an open menu.
- Bounded camera/model transition events, periodic logs and an offline log summary tool.
- Bounded same-poll retry for an empty/failed status read; indexed REFramework component arrays.
- Optional earlier HUD-camera timing experiment, with read-only phase and wall-hand diagnostics.
- Native state monitoring is read-only. Lua deliberately changes the current camera position through REFramework.

**User-observed status:** Ethan model visible in third person and both shoulders work. Open issues:
displaced interaction prompts/center, hands behind the neck at walls, repeated FP/OTS transitions.
The supplied v0.5.0 logs record seven invalid-native transitions while obstacle avoidance was OFF,
and the head controller remained at component discovery. v0.5.1 corrects the array API mismatch and
retries transient read failures; whether these eliminate the observed symptoms needs game evidence.
Aiming has not been tested with a gun. The user reports no benefit from the earlier HUD-camera
experiment, partial benefit from obstacle avoidance, and continued stationary FP/OTS switching.
v0.5.2 addresses a reproduced collision-jitter path; the latest switching cause is not confirmed
without that run's logs. HUD alignment, model occlusion and wall-edge coverage remain open.

Automatic head visibility, obstacle coverage/performance and DLC behavior remain unverified in-game. Center rays do not cover the camera volume. Aiming uses first person; third-person ballistics are not implemented. It is not a complete port of the original Cheat Engine script.

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

`work` includes standalone **Ethan with Head RT** and **Third Person Full Model Placeholders RT**
bindings. Install the matching external model package separately; model control and obstacle
avoidance default OFF. The full pack uses exact path/material signatures for Ethan, Chris, Joe,
Mia, Clancy and Zoe variants. Parts are hidden before first-person fallback and shown only after
camera readback and clearance checks. Native DLL/protocol remains v0.3.0.
See [gameplay feedback fixes and limits](docs/GAMEPLAY_FEEDBACK_KO.md),
[v0.5.0 implementation and evidence](docs/RUNTIME_EXPANSION_KO.md),
[prior offline work](docs/OFFLINE_PROGRESS_KO.md) and [Ethan asset analysis](docs/ETHAN_RT_MODEL_ANALYSIS_KO.md).

`main` holds the verified source baseline. Ongoing development uses `work` or focused feature branches. Keep user runtime logs, game executables and third-party model archives out of Git. Build artifacts are generated locally or by CI; this workflow does not publish a GitHub Release.

The first import preserves the shipped v0.3.0 runtime source. Development tests simulate the SDK/OS boundaries and do not prove in-game rendering. Further Steam Deck testing can wait until an integrated change is ready.

## Credits

Original Third-Person OTS Script: cheezeit / goldedtoad. REFramework: praydog and contributors. Model authors and original acknowledgements are preserved in [ORIGINAL_CREDITS.txt](diagnostics/ORIGINAL_CREDITS.txt). See [NOTICE](diagnostics/NOTICE.md).

The project's original implementation is MIT licensed; original mod-derived reference data and external model assets retain their own terms. The game, REFramework binaries, original CT archive and model assets are not redistributed in this repository.
