# State Publication Recovery Implementation Plan

> Use superpowers:executing-plans for inline implementation and a fresh final review.

**Goal:** Remove a reproduced state-publication stall that can force FP while still or walking; identify future fallback causes precisely.
**Architecture:** Keep atomic JSON publication and the existing 350ms freshness gate. The producer continues its normal 50ms sampling after failed publication instead of sleeping 250ms. Add bounded native failure counters and scalar gate-reset evidence; never substitute cached state for a failed read.
**Tech Stack:** C Win32 plugin, REFramework Lua, C host / Python / Lua tests.
**Spec:** User report 2026-10-16 15:48 KST: repeated FP/OTS while stationary and walking as well as near walls. Existing `docs/FP_OTS_TRANSITION_REVIEW_KO.md` distinguishes collision and state-driven fallback. We must not claim this explains every observed switch without current runtime evidence.

## Global Constraints
- Keep atomic publication, build checks, generation/replay checks and 350ms freshness limits.
- Native monitor 0.3.1 uses unchanged protocol 0.3.0; Explore 0.5.9-dev.
- No guessed game offsets, game-memory writes, raw CT or private logs in the repository.
- Existing camera/model cleanup still precedes FP fallback.

## Review Focus
- Two transient rename failures must not turn into a >350ms gap through our sleep policy.
- Persistent publication/read failure must still expire the camera heartbeat.
- Capture Win32 error before cleanup can replace it; counters describe prior attempts.
- Gate diagnostics must preserve the actual invalid/gap/generation cause across warmup.
- Extra native metadata must fit bounded logs, including populated model/phase records.

### Task 1: Producer cadence
Files: `diagnostics/src/explore_plugin.c`, `diagnostics/tests/explore_host.c`, `diagnostics/tests/test_explore_plugin.py`.
Interface: unchanged snapshot protocol; add `monitor_version` and `publication={failures,last_error}`.
- [x] Add consecutive-rename-failure scenario measuring successful-publication gaps; observe RED (>350ms).
- [x] Keep 50ms cadence on failed publication, capture failure counters and Win32 error.
- [x] Verify transient/persistent write failures, short writes and normal build checks; commit.

### Task 2: Fallback evidence and delivery
Files: `diagnostics/lua/explore/core.lua`, `adapter.lua`, transition tests, README and feedback documents.
Interface: `gate.reset_evidence` contains bounded scalar cause, native/poll gaps and generation/jump flags; include it in transition context.
- [x] Add invalid-read and native-gap regressions asserting distinct retained causes; observe RED.
- [x] Record evidence without changing control decisions; surface native monitor version in UI; bump Explore to 0.5.9-dev.
- [x] Run full `diagnostics/test.sh`, review complete diff, fix important findings with regressions.
- [ ] Publish authorized work branch and verify CI installer/source manifests against tested tree.

## Evidence / ledger
- Base: `189e26b`. Isolated task checkout on user-authorized `work`; clean before task.
- Original producer: `Sleep(published?50:250)` versus gate native gap >350ms. Two failed updates can introduce a 550ms successful-publication gap even when each file operation itself is fast.
- REFramework pinned `d052e8f`, `src/mods/bindings/Json.cpp`: load_file parses std::ifstream. Microsoft documents replacement sharing restrictions. Actual Deck sharing-error code is not yet available, so contention is an injected failure condition, not a proven diagnosis of this user's runtime.
- Task 1 metadata is additive and Task 2 consumes it only for evidence/UI; old protocol acceptance remains unchanged.

- Task 1: RED consecutive replacement failures produced a 550ms gap; after cadence fix GREEN produced 150ms. Error-count/error-code assertions failed before metadata implementation and passed afterward, including an I/O error whose cleanup overwrites GetLastError.
- Task 2: gap, invalid input, generation and transition-evidence regressions failed before implementation and passed afterward. Clock-rollback reporting and non-string optional version inputs were then covered RED→GREEN. Full populated model/phase log tests use additive metadata in the shared native fixture.
- Control decisions remain unchanged: freshness, replay, generation, collision and cleanup gates retain their prior thresholds.

- Final verification: C core 30 checks, observer 12 scenarios, Python/Lua 224 tests passed with private CT provenance check.
- Fresh independent whole-change review of `189e26b..cafe36b`: no important findings. Remaining filesystem/scheduling stalls are explicitly not claimed fixed; persistent failure still expires the heartbeat.
- Authorized GitHub publication and CI artifact verification follow this commit.
