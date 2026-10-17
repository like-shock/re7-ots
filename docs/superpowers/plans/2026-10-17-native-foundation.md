# Native Foundation Implementation Plan

> Use superpowers:executing-plans inline; one fresh whole-change review at completion.

**Goal:** Add direct native state delivery and remove unnecessary healthy-OTS head writes together.
**Architecture:** A read-only C/Lua bridge shares the existing validated native state schema. Lua keeps
visible models through internal camera restoration and uses an independent render guard for cleanup.
**Tech Stack:** C11, pinned Lua 5.4.3 core, MinGW x64, REFramework Lua, existing C/Python/Lua harnesses.
**Spec:** `docs/superpowers/specs/2026-10-17-native-foundation-design.md`.

## Global Constraints
- Explore 0.6.0-dev / native 0.4.0 / snapshot protocol 0.3.0.
- Preserve known executable hash, health, replay, generation and freshness gates.
- No guessed native game writes or inline hooks; no private CT/model/log redistribution.
- Direct failure never substitutes cached file input; no lock survives a Lua allocation/error.
- No FPS or full-original-port claim from offline tests.

## Review Focus
- Lua creation before validation, script reset, multiple states and incompatible Lua version.
- Same-millisecond direct reads must not produce a sequence/tick replay mismatch.
- External model changes and a lost/missing camera callback while visibility persists.
- Locked-scene cleanup must use no_dirty and preserve external camera ownership.
- New static Lua dependency must not introduce undeclared exports, network access or runtime code patching.

### Task 1: Native state delivery
Files: `src/native_lua.c/.h`, `src/explore_plugin.c`, build/dependency/license files,
`lua/explore/transport.lua`, adapter clock/read wiring, C/Lua integration tests.
Interface: `re7_ots_native={api_version=1,read=function()->string|nil,clock=function()->seconds}`;
`Transport.new(api,json,dir)` exposes read(), now(), status().
- [x] Add and observe failing real-Lua bridge and direct/legacy transport tests.
- [x] Implement the C registration/provider, source pin/build integration and Lua transport.
- [x] Verify unsupported/validation/limit, publication failures, clock and same-tick behavior.
- [x] Run the full native/Lua suite and commit.

### Task 2: Persistent model visibility
Files: `lua/explore/adapter.lua`, model lifecycle integration/performance fixtures/tests.
Interface: distinguish internal restore from actual FP cleanup; register a BeginRendering guard.
- [x] Add healthy-OTS zero-setter and missed-camera/render cleanup regressions; observe RED.
- [x] Preserve visibility only for a valid unchanged OTS session; retain fallback cleanup.
- [x] Update old phase assertions to distinguish computation from rendered FP and verify locked restoration.
- [x] Run model/adapter/full suites, measure call counts and commit.

### Task 3: Review and delivery
Files: README/installation/feedback/native-migration notes; CI dependency installation.
- [x] Document the direct transport indicator, required DLL+Lua replacement and remaining work.
- [x] Run complete tests and fresh independent review; no critical/important findings.
- [ ] Publish work, verify CI artifacts and deliver an experimental installer.

## Ledger
- Start: clean user-authorized work checkout at b77c183. Existing isolated workspace retained.
- Design review: native integration is the lower-risk foundation; original hook transplantation has stronger
  behavioral reference for aiming/interaction but higher mapping/build-specific risk. No numeric probability claimed.
- Existing autonomous authorization supersedes repetitive skill approval gates. Spec/plan still recorded for review.

- Tasks 1–2 implemented together in the shared adapter. RED: direct API/transport absent; healthy
  frames toggled heads; missing camera callback and render-time menu did not clean up; failed native
  clock could retain a ready gate. GREEN: real Lua/pinned-core bridge and adapter regressions.
- Complete verification before implementation commit: `bash diagnostics/test.sh` → 236/236 tests,
  plus C core/observer checks and x64 DLL import/export checks. Original-CT extraction supplied privately.
- Performance probe: same 2-head/60-node model fixture, 6631 → 2113 SDK calls, 24 → 0 visibility writes
  per healthy OTS model frame. This excludes other adapter/physics/log work and is not a game FPS result.
- Build integration: explicit CRT math thunks and disabled auto-import/pseudo-relocation avoid a hidden
  CRT-startup requirement. Pinned Lua ABI headers match the inspected REFramework revision exactly.

- Final review: fresh read-only reviewer, b77c183..749d2f7. Ready as bounded foundation; no critical or
  important findings. Reviewer additionally verified callback loss and external camera ownership.
- Final: minor (deferred): `native_read_snapshot` reads tick before phase acquisition; validation can
  publish a later start between the two, transiently producing LIMIT through unsigned subtraction.
  The next read recovers. No play-time or persistent limit failure was found; keep as a follow-up.
- Final: Ruling: Windows/Proton interoperability — publish as experimental with host/PE evidence,
  not runtime certification; if incompatible, the DLL may fail to load or execute correctly.
- Final: Ruling: actual RE7 callback ordering/FPS — preserve explicit game-verification limit;
  mock control correctness does not guarantee rendering, visual stability or performance improvement.
- Final: Ruling: foreign/custom Lua ABI — support the inspected default 5.4.3 layout only;
  `_VERSION` is not a patch-level ABI proof, and incompatible custom builds may fail.
- Final: Ruling: original CT hooks/full native port — keep unverified instruction patches disabled;
  this foundation leaves the reported aiming/interaction and other gameplay issues open.
