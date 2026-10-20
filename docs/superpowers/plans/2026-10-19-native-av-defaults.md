# Native RT 8.02 audiovisual defaults

**Goal:** Port the original default flashlight, environmental audio, camera wobble, cutscene blur and VHS corrections requested on 2026-10-19.
**Architecture:** Extend the existing validated native hook adapter with eight complete instruction spans. Keep audiovisual calculations/state in a separate module. Use the existing serialized runtime and memory callbacks.
**Tech Stack:** C11, x87 extended arithmetic, x86-64 assembly, MinHook, Python unittest, GitHub Actions.
**Spec:** Existing `docs/superpowers/specs/2026-10-17-native-ots-integration-design.md`, bounded by the user's five-item request and the supplied RT 8.02 source SHA256 `4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d`.

## Global constraints

- F6 off replays originals. Existing readiness/loading safeguards apply. No FOV, unrelated filters, new SDK traversal or settings UI.
- Use source identity predicates, integer bit comparisons and transition exceptions. Default flashlight distance is 0.2 from the automatically activated defaults entry, height -0.2; brightness FP 1 / TP 2. Raw camera attempts drive audio.
- Preserve full replaced register/memory effects. HEAD_BOB handled continuation takes the original JE target; its unhandled trampoline must relocate that branch. Failed access replays the original span with unchanged registers.
- Keep original CT private. Produce version 0.1.5 using GitHub Actions, as explicitly requested; Steam Deck gameplay verification remains separate.

## Review focus

- Incorrect handled continuation or MinHook branch relocation.
- Lost XMM upper lanes or accidentally changed GPRs/flags.
- Flashlight transition cadence, defaults override and stale identities across loading/F6.
- Audio source/destination aliases and partial writes.
- VHS character/scene exceptions and writes outside its Y coordinate.

### Task 1: Port and test eight source-derived audiovisual hooks

**Files:** `diagnostics/src/native_av.{c,h}`, `native_runtime.{c,h}`, `native_sites.h`, `native_bridge.{c,h,S}`, `original_inputs.h`; `diagnostics/tests/test_native_av.py`, `native_av_host.c`, `test_native_sites.py`, `test_native_binary.py`; host compile lists and native build.
**Interfaces:** `NativeRuntime` owns audiovisual transition/cache state. `native_av_dispatch(runtime, site, frame, fp)` returns the existing handled contract. `native_av_tick(runtime)` follows TIMESCALE cadence. Hook IDs 43–50; capacity 51. Full-vector XMM setters mark XSAVE SSE state. Handled target helper selects HEAD_BOB JE target or ordinary span end.

1. Add source-derived runtime cases for all five user features and observe failure with missing hook implementations.
2. Add complete site metadata, continuation and 51 detours; port formulas/gates and hook dispatch; validate no-op/failure/FP/loading/transition cases.
3. Extend relocation tests for the conditional branch and unwind coverage for all detours. Keep exact source tests optional through `RE7_CT_FILE`.
**Validation:** Targeted host C runtime test followed by Actions `bash diagnostics/test.sh`.
**Expected:** Original-derived outputs match; original replay paths preserve state; full offline suite passes.

### Task 2: Review, build and deliver Native 0.1.5

**Files:** version fields, `diagnostics/native/README_KO.md`, package tests and review evidence.
**Interfaces:** Clean committed sources must match build receipt and packaged DLL. GitHub Actions PR targeting work builds all artifacts.

1. Document additions and original defaults; update version consistently.
2. Commit changes, perform independent whole-branch review, fix findings with regression cases.
3. Push the feature branch, open PR against work, wait for Actions full suite and package artifact. Preserve source/DLL provenance and deliver verified installer artifact.
**Validation:** GitHub Actions offline-build job, package provenance and archive inventory.
**Expected:** Passing check and downloadable Steam Deck installer. Report gameplay as not yet tested.
