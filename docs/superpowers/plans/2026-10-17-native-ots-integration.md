# Native OTS Integration Plan

**Goal:** Deliver original native camera control with integrated device diagnostics.
**Spec:** docs/superpowers/specs/2026-10-17-native-ots-integration-design.md
**Architecture:** Pure source-derived state/collision/interaction/model modules,
an x64 context bridge, a serialized runtime adapter and reversible device package.
**Tech:** C11, x64 GNU assembly, pinned MinHook, MinGW, Python host tests.
**Authorization:** User explicitly requested autonomous implementation/publication;
no repeated approval or separate Probe-log prerequisite. Independent modules are
implemented in parallel under dispatching-parallel-agents; integration is inline.

## Global constraints
- Exact known executable and all original instruction spans before enable.
- Preserve original target comparisons and CPU outputs; no generic scene writes.
- No face hiding, SDK object scans or Lua camera updates.
- Preserve original collision/state semantics; no proximity-triggered FP fallback.
- Disabled on startup; F6 toggle, F7 shoulder; bounded integrated logs.
- Resident trampolines until exit; installer backs up competing project components.

## Review focus
- Mixed objects at shared instruction sites must never receive camera transforms.
- Original instructions that load registers must retain their post-block outputs.
- Scene/target replacement and failed pointer reads must not reuse stale ownership.
- Concurrent callbacks and mode changes must not cause alternating FP/TP writes.
- Installation or log failure must not lose existing files or free live code.

## Tasks
- [ ] Source-derived original_state, original_collision, original_interaction,
  original_model modules and focused tests. Each owns its header/API and declares
  producer registers/addresses; runtime consumes those explicit interfaces.
- [ ] native_bridge.h/.S with NativeFrame GPR layout and XSAVE/FXSAVE context,
  per-site original/handled continuations; real machine-state preservation tests.
- [ ] native_runtime.c/.h and native_plugin.c: wire matching camera, source state
  producers, collision, firing, interaction and model paths. Test original block
  effects, rejection, mode transitions, target replacement and bounded snapshots.
- [ ] native_build.sh and native installer/package/README; test Windows PE,
  original prefix relocation, installer rollback and source provenance.
- [ ] Full offline suite, independent immutable review, fixes and CI publication.
  Download and verify the resulting device-test ZIP before delivery.
