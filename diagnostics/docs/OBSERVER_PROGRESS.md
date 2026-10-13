# Observer plan progress
Spec: OBSERVER_DESIGN.md; Plan: OBSERVER_PLAN.md
Approved scope: HANDOFF.md §5, user asked to proceed with development.
Baseline: C 30/30; Python 17/18, binary test blocked only by missing clang/lld and output DLL. Obtaining compiler as locally extracted Ubuntu packages; system package installation failed due user namespace privilege handling.
Decision: use extracted task directory (no git repository present). Preserve original source plus new independent observer files; no public push.
Decision: native and Lua observations use correlated but separate files. Pairing is not atomic and object identity/semantics stays unverified.
Task 1: portable observer implemented; 12 C scenarios pass. Every sample rewalks 3 chains and rechecks links; no cached dereference.
Task 2: native wrapper and synthetic host implemented; 12 host modes pass. SHA matching is explicitly substituted only by test harness; production validates actual SHA. Baseline SHA vectors test actual algorithm. First Windows link exposed compiler-required stack/memset/float symbols; moved worker buffers to single-worker-owned static storage, kept explicit diag_zero, added _fltused marker. Build now succeeds.
Task 3: Lua camera adapter implemented; actual Lua 5.4 with SDK stubs passes lifecycle scenarios. Native freshness requires advancing token, runtime method/type checks, no setters. One synthetic assertion corrected: log creation/write failure must exit before executable reads, not after.
Independent final review: no Critical; Important stale/malformed native status could bypass camera deadline; Minor callback capability errors overwritten by ordinary polling. Regraded callback diagnostic as Important for this diagnostic deliverable: missing camera collection must retain actionable reason, otherwise user cannot distinguish unsupported callback from waiting.
Review fixes: four actual Lua regression tests failed before edits, then passed. Deadline checked in frame poll before all native-state returns; callback capability result has separate persistent UI priority. Stale native values are marked UNKNOWN; paused camera file is labelled PAUSED_NATIVE_UNKNOWN.
Task 4 wiring: default test.sh now delegates to expanded observer_test.sh so a fresh source extraction builds both binaries before discovering all tests.
Sanitizers: ASan/UBSan C and synthetic worker tests pass with leak detection disabled. LSan could not inspect /proc task directories; leak analysis is unavailable, not passed.
Final: full suite 25/25, core 30/30, observer 12 scenario groups pass. Clean source copy rebuilt both DLLs and passed same suite. Rebuilt observer DLL matches release bytes. Installation ZIP CRC and DLL/Lua contents verified. No real RE7/Proton test performed.

## v0.2.1 follow-up — 2026-10-13

User provided one real run described as "run none desktop mode". Native 1,451 samples and camera 1,129 samples ended normally. Native executable SHA/PE matched, motion/aim/inventory candidates changed, and player/aim recovered with generation 2 after NULL_LINK during the RELOAD-labelled interval. Player read recovery at 112.936 s preceded aim recovery at 122.318 s by 9.382 s. One sampled aiming interval is insufficient to claim repeated aiming validation. Interaction values also changed in INVENTORY, so do not assert examine-only semantics.

All camera samples were UNKNOWN_TRANSFORM_METHOD. Root cause: Camera is a Component, while the Transform getter is on its owning GameObject. Earlier SDK stubs incorrectly offered get_Transform directly on Camera, hiding this defect. Corrected stub hierarchy and three new tests produced four failing tests before implementation. Adapter now resolves Camera -> GameObject -> Transform, reports nil/missing owner explicitly, and reacquires each sample. Lua v0.2.1 deliberately accepts unchanged native v0.2.0 and records both versions.

After fix: full 28/28 unittest, core 30/30, observer 12 scenario groups pass. Independent reviewer inspected the small diff and reran the same checks; no actionable findings. Real camera values and CT camera equivalence remain unverified. Evidence: evidence/v0.2.1/red_camera_owner.txt, final_tests.txt, user_trace_summary.json. The next user check focuses on camera stationary/rotation/motion and reload recovery; no request to repeat the pattern scan.
