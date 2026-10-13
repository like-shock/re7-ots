# State Observer Implementation Plan

Goal: Implement HANDOFF.md §5; deliver install/source ZIP and Korean test instructions.
Architecture: portable C observation + bounded Win32 worker + official Lua camera getter adapter.
Tech: C11, clang/lld Windows x64, Lua, Python unittest.
Spec: OBSERVER_DESIGN.md
Execution: inline in the extracted isolated task directory; no existing git repository.

## Constraints
No game memory/file/save writes, no inline hooks, no new executable requests, no repeated v0.1 scan request. Unknown stays unknown. Preserve original source/reference. No game-runtime claims from mocks.

## Tasks
- [x] 1. Test and implement src/observer.{h,c}. obs_sample(reader, context, base, supported, state, sample). Test exact CT chains, each failed hop, zero, malformed pointers, mutation during read, NaN, unusual flags, reload/reacquisition and no memory changes. Expected: C assertions pass; sanitizers clean.
- [x] 2. Test and implement src/observer_plugin.c, observer_build.sh. Reuse core PE/SHA and Win32 wrappers. Test known/unknown hash gating, capped recording, markers, restart, log failure, key focus, no game writes. Expected: synthetic host passes and PE exports/imports validated.
- [x] 3. Test and implement lua/re7_ots_observer.lua. Consume native status {session,seq,tick_ms,stage,phase}; produce separate bounded camera JSONL. Test absent/malformed/stale status, camera exceptions/nil/replacement/invalid values, unsupported methods, limits, I/O errors and reset. Expected: actual Lua runtime tests pass with stub engine.
- [x] 4. Full baseline/new suite, deterministic DLL build, independent code review and fixes. Package source/build evidence and Korean install/test instructions. Expected: all claimed tests actually executed, skip/failure counts stated, saved deliverables.

## Review focus
- A pointer changes between link read and value read: fail closed; not atomic, ABA remains unprovable.
- Status from old process: require advancing heartbeat before opening camera file; stale UI never READY.
- File write fails or is partial: explicit error, never COMPLETE.
- Lua adapter receives missing methods/types: UNKNOWN, no speculative alternative getters or engine worker calls.
- Native/Lua max size and idle polling must be finite/bounded; no whole-image scan per sample.
