# Model support foundation

**Goal:** prepare Ethan RT head support without guessing model assets or REFramework bindings.
**Architecture:** offline package inventory plus a Lua visibility controller with an injected,
head-only backend. The controller is deliberately absent from the autorun bundle until an
actual asset and its runtime binding are verified. Existing Explore behavior stays v0.3.0.
**Stack:** Python 3 standard library, Lua 5.4, existing unittest/build harness.
**Spec:** `docs/MODEL_SUPPORT_DIRECTION_KO.md`.

## Constraints and evidence

- No Steam Deck run required for this increment. No model/rendering success claim.
- No third-party model assets committed or redistributed; no invented `app.*` methods.
- Official REFramework commit `d052e8fd3fd41d0a1a33394fcf6e6c19be39b1ee`,
  `scripts/utility/RE7.lua` identifies the local player through `app.ObjectManager.PlayerObj`.
  This does not identify Ethan, the replacement mesh, or a head-only visibility operation.
- `src/mods/FirstPerson.cpp` manipulates joint matrices for other supported camera code;
  that C++ implementation is not evidence for a RE7 Lua head-visibility API.
- A compatible RT archive is missing. Its absence blocks the concrete engine binding,
  not the offline inventory and transition/ownership work.
- Do not alter the existing camera fallback gates to address an unconfirmed pillar hypothesis.

## Task 1 — package inventory

Files: `diagnostics/tools/inspect_model_package.py`, `diagnostics/tests/test_model_package.py`.
Read a ZIP or an already extracted directory without installing anything. Produce deterministic
JSON with relative file paths, sizes, SHA-256, resource kinds, observed numeric suffixes and
archive SHA-256 where applicable. Never infer character, RT compatibility, joint topology,
head separation or runtime validity from filenames. Reject ambiguous duplicate paths, links,
unsafe paths and oversized inputs. Treat RAR/7z as explicit unsupported inputs, advising local
extraction. Keep reports/archives outside tracked source.

RED: fixtures verify actual inventory, ZIP/directory equivalence, corrupt archives, duplicate
and unsafe paths, bounded reads, unsupported archives and CLI failure behavior. GREEN: implement.

## Task 2 — visibility lifecycle

Files: `diagnostics/lua/model/visibility.lua`, `diagnostics/tests/test_model_visibility.py`.
Injected backend contract: `read()` returns a scalar identity and boolean visibility for a
verified local-player head-only target; `write(id, expected, desired)` is a guarded compare/set.
Backend must re-resolve and check current identity/state immediately before any engine write.
No concrete backend or automatic target search is included in this increment.

Controller operations:
- `before_camera_restore()` hides before the caller restores the camera. The caller must still
  restore the camera if hiding fails; report/latch the model fault and retry cleanup later.
- `after_camera_apply(confirmed_third_person)` shows only following successful camera readback
  and sufficient clearance, as determined by the eventual integration adapter. False hides.
- `release()` restores only a currently owned value on the same identity. Retry failed cleanup;
  never touch a replacement using the old baseline. An explicit fresh controller is required
  after a latched fault; no automatic re-enable.

Hold only scalar snapshots, never managed objects. Read back every attempted write, including
exceptions. Preserve external changes. Head binding and camera safety are independent: model
failure must not prevent camera cleanup. The boolean backend must implement real ownership of
any underlying joint values if the eventual model needs more than a renderer visibility bit.

RED: end-to-end fake-backend sequences for hide/show order, fallback, absent/replaced targets,
external changes, ignored setters, mutation then exception, recovery/release and API failures.
GREEN: implement. Run the real Lua interpreter, not a Python reimplementation.

## Task 3 — documentation, verification and review

Files: `docs/MODEL_SUPPORT_PROGRESS_KO.md`, root README and direction document.
Document supported input formats, CLI, backend contract, precise missing inputs, and no runtime
integration yet. No new binary release. Run the complete existing suite and compare production
DLL/Lua hashes to the shipped baseline. Stage files before source-package verification.
Perform one fresh independent final review focused on identity/ownership, cleanup on failures,
inventory accuracy and honest scope. Fix Important findings with RED→GREEN, then publish `work`.

## Execution ledger

- Research complete: local player lookup grounded; head-only binding remains blocked by absent asset.
- Pre-flight: Tasks 1/2 share no interfaces; Task 3 documents their actual APIs.
- Task 1: complete. Inventory tests initially RED (module absent), then GREEN.
- Task 2: complete. Lua lifecycle tests initially RED (module absent), then GREEN.
  Additional RED cases found file/directory conflicts and compare/set rejection ownership;
  both fixed. `python3 -m unittest discover -s diagnostics/tests -p 'test_model_*.py' -v`
  → 17/17 passed. A failed post-write read also verifies pending cleanup.
- Task 3: complete; final independent review and important fixes completed.
  `bash diagnostics/test.sh` with local clang/lld 18 → 65 tests, 64 passed,
  1 optional original-CT test skipped; native core/observer checks passed.
  Packaging CRC/payload checks passed for installer and source after staging files.
  Production DLL SHA-256: `2576e27533380368a52e53db2f21d1893395d42507f78ef33eb5aa83df1cdde7`.
  Production Lua SHA-256: `ca6cb6522f3ec7b1bd3d29e5fd6b33b5a63ec059d3984e5662a8dca09b3c16d1`.
  Both exactly match v0.3.0. No head-support installer or binary release is claimed.

### Final independent review

Fresh reviewer inspected `b77a772..eea43e0`. No Critical, two Important, two Minor.
Both Important findings were reproduced as failing regression tests before fixing:

- Final: fixed adoption of an external post-write value —
  `test_post_write_external_value_is_not_adopted_for_release` RED→GREEN.
  Contradictory readback now relinquishes ownership; preserving an external value takes
  precedence over speculative cleanup of an ignored setter. The documented cost is that
  a previous hidden state can remain until a verified backend resolves it.
- Final: fixed deep ZIP paths bypassing resource limits —
  `test_path_depth_length_and_implicit_directories_are_bounded` RED→GREEN.
  Bound UTF-8 path length/depth before building prefixes; count implicit directories.
- Final suite: `bash diagnostics/test.sh` → 67 tests, 66 passed, 1 optional CT skip.
  Native core 30 checks and observer 12 scenarios passed. Production hashes unchanged.
- Final: minor (deferred): malformed LZMA ZIP raises an unnormalized decompression exception
  (CLI traceback/exit 1 rather than concise exit 2). No installation or source modification occurs.
- Final: minor (deferred): implicit parent case collisions (`A/one`, `a/two`) can be accepted
  in ZIP input where explicit directory entries or an extracted case-sensitive tree are rejected.
- Final: Ruling: real target identity, engine writes, RT compatibility, rendering and Steam Deck
  behavior cannot be judged without an asset/backend — keep integration disabled and request
  the RT model archive next — cost: the binding/hide technique may need to change after inspection.

The one Important-fix pass is verified by regression tests and the full suite; no second review.
No merge to `main`, device diagnostic run or additional approval round is part of this increment.

Ruling: native branch isolation already exists (`work` in a separate clone); execute inline.
The user's repeated authorization covers implementation and pushing to the specified repository.
No further approval checkpoint or device-only diagnostic run is needed for this increment.
