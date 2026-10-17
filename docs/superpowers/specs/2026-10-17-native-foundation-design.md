# Native state bridge and persistent OTS model visibility

## Intent and scope
The user authorizes native migration together with eliminating per-frame head hiding/showing.
Prior instructions authorize autonomous implementation and publication on `work` without renewed approval.
This first integrated step removes status-file reads from the available native path and preserves
model visibility between healthy OTS frames. It does not claim a complete port of the original CT.

Current baseline: `b77c183`, Explore 0.5.9-dev, native 0.3.1. Result: Explore 0.6.0-dev,
native 0.4.0; retain the validated snapshot protocol 0.3.0 and existing build/health gates.

## Architecture
- Register `re7_ots_native` through REFramework's Lua-state-created callback. API version 1 exposes
  `read()` (fresh, read-only state encoded as the existing JSON protocol) and `clock()` (monotonic seconds).
- Read the known state chains directly on demand after the background executable validation completes.
  The direct path neither reads nor writes status files. Keep the file monitor for old consumers.
- Share only immutable session/build metadata with the background validator using release/acquire atomics;
  protect direct sampling state with a nonblocking atomic flag. Never hold that flag during a Lua API call.
- New Lua prefers the direct interface and parses its returned string. If the interface is absent it retains
  explicit legacy-file compatibility. Once selected, a failing direct read never falls back to an old file.
  Poll at the existing 50ms cadence. Use the native clock consistently when the direct interface is available.
- Follow the official plugin's static Lua linkage pattern. Pin Lua 5.4.3, matching the inspected REFramework
  source `d052e8f`, verify its source archive hash, retain its license, and require a Lua 5.4 state.
  Use MinGW cross compilation for Explore; keep the preserved diagnostics/observer builds unchanged.
- Internal camera-base restoration may preserve the current head state only across a healthy OTS frame,
  unchanged model configuration/epoch, and a working render guard. Actual fallback still hides the head.
- `BeginRendering` verifies the current frame reached a valid OTS pose; otherwise hide owned heads and
  restore owned camera state. Camera restoration at this locked-scene phase uses `no_dirty=true`.
  A missing guard API disables persistence and retains the old conservative lifecycle.
- Keep per-write identity/ownership validation and readback. Unchanged displayed values require no setters.
  Proximity, aim, interaction, menu, invalid state, epoch change, exceptions and reset still hide/clean up.

## Constraints and remaining work
No guessed native game writes or inline hooks; native reads remain restricted to the known executable hash.
No raw CT, model assets or user logs enter GitHub. DLL pinning and the twelve-hour monitor limit remain.
Do not claim game FPS or visual stability from mock counts. The Lua model backend, collision-layer selection,
prompt/interaction integration and asynchronous journal remain separate later native-migration work.
Original firing/interaction hook transplantation requires verified instruction context and object ownership
for this exact build; raw offsets from the CT are not a drop-in implementation.

## Verification
Exercise the real C bridge with a real Lua runtime, not a fake Lua stack. Cover validation/unsupported/limit,
same-tick reads, direct reads despite file publication failure, and registration failure.
Lua integration covers direct preference, no stale-file substitution, clock use, healthy-frame zero setters,
near-camera/actual FP cleanup, missing or delayed camera callback, render-guard restoration and partial failures.
Build and inspect the Windows DLL imports/exports, run the complete suite, obtain fresh independent review,
and verify CI package/source manifests. Actual runtime callbacks, custom Lua ABI builds and game FPS remain unverified.
