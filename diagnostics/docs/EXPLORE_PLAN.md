# RE7 OTS Explore Implementation Plan

> For agentic workers: use superpowers:executing-plans; execute inline with one final independent review.

Goal: integrated exploration camera with automatic state monitoring and play logs.
Architecture: read-only C monitor + pure Lua control rules + engine adapter and bounded journal.
Tech: C11, clang/lld Windows x64, Lua 5.4, Python unittest.
Spec: EXPLORE_DESIGN.md.

Global constraints: exact current-build gate; no raw game writes/inline hooks; setters only in Lua engine callbacks; OFF on startup; no body/collision/DLC claim; preserve previous sources; no repeated diagnostic-only user task. Existing isolated extracted workspace has no git repository, so archive/evidence and this ledger replace git-only helper bookkeeping. Prior user approval covers the integrated prototype; implement without another approval round.

## Task 1 — automatic native monitor
Files: src/explore_plugin.c, explore_build.sh, tests/explore_host.c, tests/test_explore_plugin.py.
Consumes observer.h/c and core.h/c unchanged. Produces status.json schema=1 version=0.3.0 session/phase/seq/tick_ms/foreground/player/aim/inventory/interact. MONITORING only after hash/PE validation. 50 ms sleep, 43,200,000 ms lifetime, atomic temp-file replacement.
- [x] Write failing tests for automatic start, hash/PE failures, NULL/read failures, atomic failure and short write, thread creation failure, lifetime, focus.
- [x] Run new unittest: expected missing implementation assertion.
- [x] Implement, run new unittest and existing suite: expected all pass.

## Task 2 — pure control and journal
Files: lua/explore/core.lua, lua/explore/journal.lua, tests/test_explore_lua.py with shared Lua runner.
Core produces new(), ingest(status,time), ready(time), offset(position,quaternion,settings,blend), settings(input). Gate validates complete status; increasing tokens and 0.5-second stable generation window. Journal produces new(io,json,dir,max_bytes), record(record), close(reason), state.
- [x] Write behavioral tests for valid geometry and invalid input; delayed recovery, stale/replay and malformed state; bounded rotation and failure cleanup.
- [x] Run new tests: expected absent-module failure.
- [x] Implement, rerun: expected all named scenarios pass.

## Task 3 — playable adapter
Files: lua/explore/adapter.lua, tools/build_explore_lua.py, tests/explore_lua_harness.lua, tests/test_explore_lua.py.
Consumes Core and Journal via generated single autorun file. Uses documented Transform position/rotation/set_position, REFramework callbacks/UI/input, JSON config. Generates re7_ots_explore.lua; no module loader dependency.
- [x] Write failing integration tests for OFF/key/enable, engine restoration and nonaccumulation, fallback/reload, replacement, setter/readback/restore failure, callbacks, log failure.
- [x] Build/test expected failing behavior before adapter implementation.
- [x] Implement lifecycle/UI/config/logging; run all tests expected pass.

## Task 4 — review and release
- [x] Build entire suite and inspect x64 ABI/imports and deterministic DLL/Lua.
- [x] Independent reviewer checks complete diff against v0.2.1 archive, using this spec/plan and ledger.
- [x] Fix important findings via failing regression tests then passing suite.
- [x] Package installer/source with Korean guide, explicit prototype limits and one-button play check. Verify ZIP bytes. Delivery saving is tracked by the active task.

Review focus: status replacement blocked by open readers; stale/replayed status after script reset; old camera identity during reload; setter throwing after partial mutation; restoration when game/another mod changes position. Tests belong to native publication, core freshness and adapter ownership respectively.
