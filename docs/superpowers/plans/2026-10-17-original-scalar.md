# Original scalar core implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port original view/coordinate scalar behavior independently of face hiding.

**Architecture:** An allocation-free C core takes resolved values, never game pointers.
Axis routines allow the future hook to preserve CT ordering. Explore does not call it yet.

**Tech Stack:** C11, x86-64 GCC/MinGW, Python unittest, private pinned CT oracle.

**Spec:** `docs/superpowers/specs/2026-10-17-original-scalar-design.md`

## Global Constraints

- First-person face hiding is optional and must not gate this port.
- This increment is source/build-only: no game memory hooks, no new installer, no runtime/FPS claims.
- Preserve original float-store boundaries and unsigned float-bit comparisons.
- Existing Explore runtime and its read-only native contract stay intact.

## Review Focus

- Aim enabled with default FP-aim disabled: OTS policy stays active.
- Near-wall multipliers: coordinate contraction must not change view state.
- Pitch/focus changes between X and Z: caller can preserve original ordering.
- Focus thresholds and completion: retain overshoot and one-call completion delay.
- Firing camera unavailable: original firing vector is retained, never partial coordinates.

## Task 1: Native scalar core and verification

**Files:** `diagnostics/src/original_scalar.{c,h}`, `diagnostics/tests/test_original_scalar.py`,
`diagnostics/tests/original_ct_oracle.py`, `diagnostics/original_build.sh`,
`diagnostics/observer_test.sh`, `docs/ORIGINAL_NATIVE_PORT_KO.md`, README.

**Interfaces:** `re7_original_view`, `re7_original_ots_axis`, `re7_original_fp_axis`,
`re7_original_pitch_multiplier`, `re7_original_focus_step`,
`re7_original_water_y`, `re7_original_firing_origin`; caller-owned scalar structs.

- [x] Write behavior tests for all Review Focus cases, default values, both shoulders,
  three axes, sticky recent-FP, water boundaries and invalid camera fallback.
- [x] Run `python3 -m unittest discover -s diagnostics/tests -p test_original_scalar.py -v`.
  Expected: missing core implementation fails the test build.
- [x] Implement the C core with original source provenance. Extract only numeric
  x87 instruction ranges from the private CT for an independent arithmetic oracle.
- [x] Repeat the command. Expected: all numeric/policy tests pass; optional oracle
  skips only if the private CT is absent, rejects a wrong source hash.
- [x] Add Windows x64 object compilation to `diagnostics/test.sh` via observer_test.
  Run the full existing suite. Expected: host and Windows build plus all tests pass.
- [ ] Document implemented vs unconnected scope and latest priority, review, commit,
  publish to `work`, and verify remote source/CI. No installer replacement requested.

## Verification record

- Initial test run failed because the native core did not exist.
- 14 new test cases pass, including 6,000 bit-exact original x87 comparisons.
- Windows x64 object compiles without undefined imports. Full local suite: 250/250 pass.
- Runtime integration and actual game verification are pending, not implied by these results.

## Review and decisions

- Fresh review found two Important original-label branch mismatches in focus IN/OUT.
  Both were reproduced RED and fixed; 14 targeted tests and the full 250-test suite pass.
  No Critical or Minor findings. Review base f0d1e38, reviewed implementation de21324.
- The invalid-camera firing fallback deliberately keeps all original XYZ; unlike CT, it
  never leaves partial writes. Tradeoff: failed camera reads use the original origin.
- Game hook installation, register/object lifetime, snapshot collection, full camera
  dispatch, collision, weapon-block/state producers remain future integration scope.
  Scalar equivalence cannot authorize treating them as implemented.
- FPS, aim feel and interaction alignment remain game-verification questions.
- Face hiding is optional under the user instruction, independent of camera state policy.
- Other x87 precision/rounding modes are unverified; future hooks must establish or
  preserve the required environment. This is a source publication, not a playable port.
