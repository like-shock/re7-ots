# Coordinate and Model Performance Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Correct verified prompt reprojection and reduce remaining RT head CPU work.
**Architecture:** Preserve existing camera/visibility rollback. Batch read-only head resolutions within one synchronous session operation; invalidate around every setter. Reproject the supported RE7 interaction GUI at draw time using its world anchor, preserving game ownership and restoring after drawing.
**Tech Stack:** REFramework Lua; existing native read-only monitor; Python/Lua regression harness.
**Spec:** User request on 2026-10-16: review/fix coordinate correction and further improve still-slow third person. Existing compatibility and rollback requirements in diagnostics/EXPLORE_README_KO.md apply.

## Global Constraints
- No raw CT or user logs in GitHub; no guessed native offsets.
- No SDK object retained across model callback boundaries.
- Restore owned state on FP/failure/reset; never write replaced or foreign model objects.
- Gameplay accuracy and FPS require Steam Deck confirmation.
- Preserve FP aiming until the firing/interaction native origins are mapped.

## Review Focus
- Getter wrappers change while underlying model owner remains the same.
- A setter reorders materials, replaces owners, adds a shared mesh, or throws after mutation.
- HUD control parent/scale differs from expected screen layout.
- GUI reentry or missing post callback must not accumulate a correction.
- HUD feature unavailable must not disable exploration or model cleanup.

### Task 1: Head read batching
Files: diagnostics/lua/model/ethan.lua, session.lua; diagnostics/tests/test_model_performance.py, model_backend_harness.lua.
Interfaces: Ethan:with_read_scope(fn) executes one synchronous operation and discards SDK references on success/failure; all mutations invalidate its memo.
- [x] Add SDK-call budget and post-setter replacement regression cases; run and observe RED.
- [x] Eliminate duplicate reflected method lookups in call(); scope repeated read-only target resolutions without relaxing per-write revalidation.
- [x] Run model suite; record measured counts and commit.

### Task 2: Prompt reprojection
Files: diagnostics/lua/explore/prompt.lua, adapter.lua, core.lua, tools/build_explore_lua.py; tests/test_prompt.py and adapter harness wiring.
Interfaces: Prompt.new(...), frame/context and pre/post draw methods; status returns bounded scalar evidence.
- [x] Confirm RE7 UIWorldPosAttach / _NowTargetPos / _TargetGUIElem._Element reference and restrict correction to validated screen controls.
- [x] Add projection, restore, reentry, replacement, unsupported layout and integration regressions; observe RED.
- [x] Apply final-camera reprojection only when the source projection/layout can be validated; retain original game prompt when uncertain.
- [x] Run focused suite and document limits; commit.

### Task 3: Delivery
Files: README.md, diagnostics/EXPLORE_README_KO.md, docs/GAMEPLAY_FEEDBACK_KO.md.
- [x] Run complete diagnostics/test.sh with private CT locally; inspect results.
- [x] Fresh whole-change code review; fix significant findings with regression cases.
- [ ] Publish to authorized work branch, obtain passing CI artifact, verify package contents and deliver.

## Investigation / progress
- Baseline 6411f73, Explore 0.5.7-dev. Clean worktree.
- User reports faster OTS but still slow.
- Existing logs: 1,334 paired camera Transform/Camera joint observations, zero differences. A second joint-position write is unjustified.
- Original FIRING_COOR_WRITES synchronizes firing XYZ with camera; this is not proof that unarmed interaction uses the same field.
- Native aiming patch deferred pending object/caller mapping; shipping it from an unknown field would risk unrelated gameplay.

## Verification / review evidence
- SDK boundary calls in the two-head / 60-other-node steady-frame fixture: 15,251 → 6,631 (56.5% fewer); component-array reads: 275 → 243. These measure mock workload, not game FPS.
- Four-head workload: 10,760 SDK boundary calls, 303 component-array reads per frame.
- SDK-faithful missing-method regression first failed, then passed: REFramework can return nil for an absent method. Non-nil calls avoid duplicate reflection; nil results explicitly verify method availability so tree/chain discovery cannot silently truncate.
- Fresh independent review found two important issues, both fixed with failing-then-passing regressions: an absent parent getter could authorize prompt correction, and unrelated HUD draws unnecessarily resolved the full camera.
- Prompt layout now requires an identity chain reaching the exact GUI view; missing/unrelated roots are rejected. An early cursor filter avoids primary-camera lookups for unrelated GUI while preserving pending cleanup.
- Prompt tests cover nested draws, missing post, partial/ignored writes, replacement controls, external ownership, unsupported layout, and a changed final camera pose.
- Native interaction selection and firing-origin correction remain deferred: no verified managed field or hook boundary identifies the original CT's native coordinate owner.
- Final local verification: C core 30 checks, observer 12 scenarios, Python/Lua 219 tests passed, including the private original-CT provenance comparison. GitHub delivery/CI verification follows this commit.
