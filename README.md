# RE7 OTS

Experimental over-the-shoulder camera for Resident Evil 7, built with REFramework for Steam Deck.

**Current priority: native port of the original RT 8.02 control paths.**
First-person face hiding is optional and is not a prerequisite for that port.
The [Native OTS 0.1.0 device build](diagnostics/native/README_KO.md) connects
41 original instruction sites for actual camera, focus, firing, inspection,
collision and model-transparency control. F6 enables/disables; F7 swaps shoulders.
It has no Lua/SDK model traversal and no face-hiding prerequisite. The installer
backs up Explore/Probe and preserves installed model assets. Automatic bounded
diagnostics accompany the gameplay test; a separate Probe run is not required.
See [integration scope and limitations (Korean)](docs/NATIVE_INTEGRATION_KO.md).
This is an experimental device-test build, not a claim that the reported screen,
FPS or switching issues are fixed. Explore remains available as the prior build.

**2026-10-17 field report:** third person is faster but remains slower than first person;
the other reported issues have not improved. v0.5.9 is not a validated resolution.
The [architecture reassessment (Korean)](docs/ARCHITECTURE_REASSESSMENT_KO.md) documents
per-frame model churn, 31-layer obstacle queries, a consumer-delay fallback loop, and
the criteria for a limited integration prototype. The subsequent native foundation is described in [migration notes](docs/NATIVE_FOUNDATION_KO.md).

## Development version: Explore Lua v0.6.0-dev (native state bridge and persistent model visibility)

- Direct native state/monotonic clock API, with explicit legacy-file compatibility.
- Healthy OTS frames preserve model visibility; actual fallback still hides owned heads.
- A render guard checks the final camera pose and handles a missed camera callback.
- F6 toggles the exploration camera; every script load starts OFF.
- Adjustable camera distance, shoulder offset and height; F7 swaps shoulders.
- Configurable aim-release hold and camera return blend.
- First-person fallback when aim, inventory, interaction or uncertain runtime state is detected.
- Opt-in RT head visibility for eight model families, including separate Mia face/hair ownership.
- First-person head hiding also disables each verified head material and preserves its original enable value.
- Independently hide up to four verified Ethan head instances; restore each copy’s original display values in third person.
- Identify render targets by their live components, independently of temporary resource/joint getter objects.
- Use a bounded, owner-checked component chain when an array result lacks the indexed SDK API.
- Share group validation within each callback instead of scanning the entire tree for every material write.
- Bounded automatic evidence distinguishes multiple head objects from multiple mesh components and repeated references.
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
Before v0.5.6 the user reported floating facial features in first person. The supplied v0.5.3 logs show
model selection blocked by `WAIT_MODEL_AMBIGUOUS`; material hiding never acquired a target.
v0.5.4 runtime diagnostics consistently identify two distinct verified Ethan head objects.
v0.5.5 removes the single-Ethan-head assumption in group discovery and binds each verified copy
by identity, with independent DrawSelf/material ownership. First person hides every acquired copy;
third person restores each copy’s original display values after camera/clearance checks. Shared
head/body objects and shared mesh components still block control. This fixes the reproduced
selection failure. The user still reports floating facial features with v0.5.5: the new logs show
two discovered heads, but every enabled sample is blocked before reading their display baseline.
Most part records show `WAIT_MODEL_TARGET`, with intermittent `COMPONENT_ARRAY_ACCESS` failures.
v0.5.6 removes resource-holder/joint getter addresses from render ownership identity while retaining
the exact path, material and joint-owner checks. It also validates the complete component chain
when indexed array access is unavailable. Fresh-wrapper and array-access failures are reproduced
offline; the logs do not identify which old identity field changed. Object names and getter
addresses now provide additional diagnostic evidence.

The user confirms facial remnants disappear with `RT head model (experimental)` enabled, but
reports severe third-person slowdown that clears in first person. v0.5.7 removes repeated group
scans from each material write while preserving per-write target validation, value comparisons,
readbacks and hiding before camera restoration. A final group scan rejects changes during showing.
Release writes keep fresh checks; a failed channel invalidates authorization before further writes.
In 100 simulated steady frames with two heads and 60 unrelated nodes, full scans fall from 37 to
3 per frame and component enumerations from 2,393 to 275. These are code-path counts, not game FPS.
Steam Deck speed improvement, shadow roles, obstacle coverage and DLC behavior remain unverified.
Center rays do not cover the camera volume. Aiming uses first person; third-person ballistics are
not implemented. It is not a complete port of the original Cheat Engine script.

## 설치 / 사용

[한국어 설치·플레이 안내](diagnostics/EXPLORE_README_KO.md)

기존에 작동하던 REFramework 환경에서 사용합니다. 이전 Observer/Diagnostics DLL과 Lua는 게임 종료 후 별도 폴더에 보관하고, Explore의 DLL과 Lua를 설치하세요. 게임 실행마다 기본 OFF이며 F6으로 켜고 끕니다. 별도 진단 단계 없이 플레이 로그가 자동으로 남습니다.

## Build and test

Linux x86_64 dependencies: clang 18+, lld-link, host C compiler, Python 3, liblua5.4 and MinGW x64 GCC (posix). The Explore build fetches hash-pinned Lua 5.4.3 source.

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
camera readback and clearance checks. Native DLL is v0.4.0; snapshot protocol remains v0.3.0.
See [gameplay feedback fixes and limits](docs/GAMEPLAY_FEEDBACK_KO.md),
[v0.5.0 implementation and evidence](docs/RUNTIME_EXPANSION_KO.md),
[prior offline work](docs/OFFLINE_PROGRESS_KO.md) and [Ethan asset analysis](docs/ETHAN_RT_MODEL_ANALYSIS_KO.md).

`main` holds the verified source baseline. Ongoing development uses `work` or focused feature branches. Keep user runtime logs, game executables and third-party model archives out of Git. Build artifacts are generated locally or by CI; this workflow does not publish a GitHub Release.

The first import preserves the shipped v0.3.0 runtime source. Development tests simulate the SDK/OS boundaries and do not prove in-game rendering. Further Steam Deck testing can wait until an integrated change is ready.

## Credits

Original Third-Person OTS Script: cheezeit / goldedtoad. REFramework: praydog and contributors. Model authors and original acknowledgements are preserved in [ORIGINAL_CREDITS.txt](diagnostics/ORIGINAL_CREDITS.txt). See [NOTICE](diagnostics/NOTICE.md).

The project's original implementation is MIT licensed; original mod-derived reference data and external model assets retain their own terms. The game, REFramework binaries, original CT archive and model assets are not redistributed in this repository.

### v0.5.8-dev

- Reprojects the validated RE7 interaction cursor from its world anchor using the confirmed final camera pose. Defaults ON; unknown layouts keep their original prompt. GUI writes are scoped to draw and restored afterward.
- Shares read-only head resolutions within one synchronous operation and removes duplicate reflected method searches. All writes still reacquire their target; scopes are discarded on setters, exceptions and reentry. Mock SDK calls per steady frame: 15,251 → 6,631 (two heads, 60 unrelated branches).
- Adds scalar head callback timing and prompt source/destination evidence to automatic logs.
- A-button rendering remains unverified on hardware. Native interaction selection and third-person firing-origin correction are still open; aiming continues to fall back to first person. SDK call reduction is not an FPS benchmark.

### v0.5.9-dev

- Keeps native state sampling at 50ms after failed atomic publication. The previous 250ms failure backoff amplified two brief replacement failures into a 550ms gap, exceeding the 350ms camera freshness limit. The same host scenario now has a 150ms publication gap.
- Adds native publication error counters and precise gate-reset evidence for stationary/walking fallback investigation. No heartbeat or invalid-state checks are relaxed.
- Includes native DLL v0.3.1, retaining protocol v0.3.0. Replace both installer files; the UI shows the native monitor version.
- Reproduction injects file replacement failures; it does not prove that every reported in-game switch has this cause.
