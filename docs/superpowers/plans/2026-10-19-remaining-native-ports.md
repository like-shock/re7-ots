# Remaining RT 8.02 Native Ports Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement these tasks sequentially. Each version must pass GitHub Actions before the next task begins.

**Goal:** Apply the user's five approved groups in order, producing Native 0.1.6, 0.1.7, 0.1.8, 0.1.9 and 0.1.10.
**Architecture:** Extend the existing serialized native runtime with small screen, HUD, FOV and optional-light/input modules. Preserve the current complete-instruction replay contract and guarded memory interfaces. Existing settings gain independent controls in the last version.
**Tech Stack:** C11, x87, x86-64 bridge, MinHook, Python unittest, GitHub Actions.
**Spec:** `docs/superpowers/specs/2026-10-19-remaining-native-ports-design.md`, the user's approved five-group review and supplied private RT 8.02 CT.

## Global Constraints

- The CT hash is `4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d`. Keep the CT and all game/model assets private. Use existing manifest patterns, full replaced instruction spans, exact source predicates and x87 rounding.
- Implement and validate one group at a time; increase the build/package version exactly once per group: 0.1.6 through 0.1.10. Fixes within a group retain its version. The controller publishes each reviewed version and waits for its complete Actions suite and package artifact before dispatching the next implementation.
- F6 off replays camera/effect originals and preserves the existing independent head-only option. Missing readiness, loading, failed or changed pointers must not authorize stale writes. Preserve complete register, flag, stack and XMM effects; fallbacks replay an untouched original frame.
- Build Windows DLLs and packages through GitHub Actions, never Docker or local game execution. Local x86_64 host arithmetic/adapter checks are permitted. No merge or release publication is requested; update the existing feature PR.
- Optional Fullbright, forced custom shader and controller mod hotkeys default OFF/unassigned. Mia hair hiding defaults ON independently of the main head option. Existing stored settings retain their meaning. Do not add Freecam, timescale cheats or invulnerability.

## Review Focus

- Producer and consumer order across loading, F6 and pointer replacement: add invalidation/reacquisition cases in the owning task.
- Full instruction effects, especially XMM upper lanes and CMP flags: compare complete frames, not only visible scalar values.
- Valid input at boundaries: source integer/float-bit predicates, psycho call cadence, HUD oxygen 100 and FOV per-frame gates.
- Once site count exceeds 64, no C/assembly mask shift may wrap or become undefined; cover site 64 and final site with F6 off and observation counters.
- Persisted option migration and held/reconnected controller inputs must not activate optional effects or toggle repeatedly.

### Task 1: Native 0.1.6 basic screens and Critter

**Files:** Create `diagnostics/src/native_screen.{c,h}`, `diagnostics/tests/native_screen_host.c`, `diagnostics/tests/test_native_screen.py`; modify `native_runtime.{c,h}`, `original_interaction.{c,h}`, `native_sites.h`, `native_bridge.{c,h,S}`, relevant runtime host compile lists, native build, test.sh, site/binary tests, version/package/docs fields.
**Interfaces:** `NativeRuntime.screen` owns `NativeScreenState`; `int native_screen_dispatch(NativeRuntime*,unsigned,NativeFrame*,void*)` returns the existing handled contract. `void native_screen_reset(NativeRuntime*)` clears transient state. Add sites 51–55 for VIGNETTE, PSYCHO_TIMER, CLOSE_DISTORTION, NAH_DOT_FORCE and NAH_CRIT_HP_EFFECT_CMP; total 56. Keep existing policy Config.critterexamine ABI but populate it from actual item observation as transient state, reset alongside Scrap and on loading/F6.

- [ ] Write failing source-driven adapter cases: N…22=>Critter 1, N…09=>Scrap 1, unrelated/failed read=>both 0; EOZ policy receives producer; TP vignette EAX=2 versus FP original; psycho producer 2 and decrement-before-check, FP1/TP2 intensity versus ordinary TP0/FP original; NAH area35+TP+live aim=>EBP0 while preserving LEA EAX; NAH TP redline suppression sets CMP AL,AL flags, FP original CMP. Read CT 4007–4060 and 9629–9759.
- [ ] Run focused RED with Darwin x86_64 host support (test_native_av.py is the precedent); missing new sites may be addressed by test-local discovery but production behavior must demonstrably fail before implementation.
- [ ] Implement five complete sites and Critter production, with full frame/vector and replay checks. Add load/F6 clear and failed pointer cases. Update relocation/unwind and compile sources everywhere needed. Bump all active native version fields to 0.1.6; retain historical version records.
- [ ] Run targeted adapter/source checks, report exact commands/results; controller runs complete suite in Actions. Self-review, commit, and write the task report.
- [ ] Controller: task review/fixes, publish lease-checked PR head, verify Actions tests+package for 0.1.6, record immutable commit/run/artifact before Task 2.

### Task 2: Native 0.1.7 blood, hurt filter and head delay

**Files:** Extend `native_screen.{c,h}`, `native_runtime.{c,h}`, `native_head` integration and tests, input pointer support as necessary; all site/bridge/build/version/package metadata.
**Interfaces:** Retain Task 1 dispatch/reset. Add `void native_screen_tick(NativeRuntime*)` at TIMESCALE cadence and `void native_screen_observe_player(NativeRuntime*)` at CUTSCENE_AUTOLOOK. Screen state owns guarded HP address, hurt filter status and head delay. Add SCREEN_BLOOD_WRITE_FOLLOW and HURT_FILTER_READ sites, total 58.

- [ ] Add RED cases for blood FP100/TP0 thresholds using source x87 HP/10 bit comparisons, stoppedbloodhide cancellation, missing/replaced HP pointer fallback; hurt TP2/FP0 plus forced CMP AX1,0 flags on all source return paths; delay refreshed to2 while stoppedbloodhide then decremented by source cadence, retaining validated head targets until expiry. Read CT1213–1217,1383–1447,3271–3279,9225–9271,10048–10114.
- [ ] Confirm RED; implement HP source chain with guarded identity revalidation, two complete hooks, full frame preservation and source-derived state. Coordinate head_allowed, cache refresh and TP clear with delay; clear on load/F6 and avoid stale targets. Do not add optional custom shader controls yet.
- [ ] Run screen/head/AV regressions including F6 head-only mode and character/load changes; bump version0.1.7 and package/docs; self-review and commit.
- [ ] Controller: review/fix, publish and wait for complete Actions tests+package0.1.7 before Task3.

### Task 3: Native 0.1.8 NAH HUD

**Files:** Create `native_hud.{c,h}`, HUD host/Python tests; modify runtime, bridge only as needed, site/build/version/package metadata.
**Interfaces:** `int native_hud_dispatch(NativeRuntime*,unsigned,NativeFrame*,void*)`, `void native_hud_reset(NativeRuntime*)`; NativeRuntime owns NativeHudState. Add NAH_OXYGEN_REF, NAH_HUD_WRITE, NAH_OXYGEN_HUD_SWITCH, total61. If emulating stack R12 load, add documented `uint64_t native_frame_game_rsp(const NativeFrame*)` based on bridge-owned frame layout and test against real bridge; do not expose arbitrary test-only state.

- [ ] Write RED for CT9274–9627 and10229–10244: oxygenref and display flags producer; only area35+TP hide; oxygen<100, contamination transitions, transmissions, filter/nightvision/RAMROD notices and every explicit spatial/story exception preserve required HUD. Include empty-hand initial load, bombtalk/freezetalk reset, first-hide-frame latch and original R12 stack load.
- [ ] Implement the three hooks together with exact source chains/predicates and full original effects. Multi-write failure must restore attempted writes or return safely without replaying from a mutated frame; no unconditional oxygen/HUD suppression. F6/load/area change invalidation and pointer replacement covered.
- [ ] Run HUD/bridge and prior adapter regressions; bump0.1.8, docs/package, self-review and commit.
- [ ] Controller: review/fix, publish and wait for complete Actions tests+package0.1.8 before Task4.

### Task 4: Native 0.1.9 FOV

**Files:** Create `native_fov.{c,h}`, FOV adapter/source tests; modify runtime/inputs/bridge/site/build/version/package metadata.
**Interfaces:** `int native_fov_dispatch(NativeRuntime*,unsigned,NativeFrame*,void*)`, `void native_fov_reset(NativeRuntime*)`; NativeRuntime owns NativeFovState. Extend TRUE_FOV_READ after retaining existing camera-rate effects. Add MENU_FOV_BYTE, REFRAMEWORK_FOV_WRITE_POINT, TRUE_FOV_REFERENCE, FOV_WRITE_MAIN, FOV_WRITE_NAH, FOV_WRITE_EOZ, total67.

- [ ] Write RED from CT2474–2853,3764–3777,4151–4188: menu70+5*index, custom FOV OFF, TP aim ratio suppression and run FOV defaultON/FP OFF, max20 percent and source7s/.7s rates, one-frame update gate, original FOV ref/load safety, RF recent-write grace, all three engine write sites. Verify competing writes are suppressed only with live valid mod control, and F6 returns control.
- [ ] Implement full source policy and guarded writes as one bundle; preserve x87/intermediate stores and unsigned bit comparisons. Never disable game stores without valid replacement control. Extend observation bookkeeping safely above64 (retain low-word existing consumers, add high word/helper as needed), guard head-only C and assembly masks; tests for site64 and66 F6 off and counters.
- [ ] Run FOV arithmetic/source and all adapter/bridge regressions; bump0.1.9, docs/package, self-review and commit.
- [ ] Controller: review/fix, publish and wait for complete Actions tests+package0.1.9 before Task5.

### Task 5: Native 0.1.10 independent optional features

**Files:** Create `native_fullbright.{c,h}`, `native_controller.{c,h}` and tests; extend screen custom-filter paths, head hair option, native_options/UI/plugin persistence, site/bridge/build/version/package metadata.
**Interfaces:** New modules expose dispatch/reset with the established contract. New runtime setters for hide_mia_hair, custom_filter (enabled+validated source shader ID), fullbright (enabled+RGB+finite brightness), controller camera/shoulder button bindings; serialization follows existing worker ownership and atomic UI request model. Add FLASHBACK_FILTER_READ/_2, XINPUT_WRITE/FLOATS, seven Fullbright sites, total78.

- [ ] Write RED for old three-option file migration; new defaults hairON, othersOFF/unassigned; head stays hidden when hair independentlyOFF; exact source shader substitution and hurt precedence; source24 controller button bytes produce one rising-edge camera/shoulder action, held keys do not repeat and load/disconnect/reassignment do not synthesize a press; Fullbright disabled replays all7 originals, enabled uses exact identified examine light, radius1000, cone-1.9, defaultwhite brightness1, far-500 and global .125/.27, all original register/stack effects.
- [ ] Implement custom filters from CT10116–10171, input collection3829–3960 and Fullbright15622–15920 with guarded identity and safe reset. Controller consumer is optional native camera/shoulder shortcuts using user-selected source button IDs; no new Freecam or timescale feature. Preserve F6 ability to re-enable camera from configured controller input without enabling other effect hooks while off.
- [ ] Add usable independent menu/settings persistence and validation, bounded RGB/brightness/filter selectors or fields supported by existing UI API; no dummy enabled options. Explain defaults and controller binding behavior in Korean docs. Restore optional global state on disable, without clearing another feature's shadow-darkening choice.
- [ ] Run new settings/UI/controller/fullbright/screens cases and prior regressions; bump0.1.10; self-review, commit and report.
- [ ] Controller: task review/fixes, whole-branch review, publish and verify complete Actions tests+package0.1.10. Deliver all five version artifact links and precise build/gameplay verification distinction; do not merge.
