# v0.3.0 independent review and resolution

One fresh independent reviewer examined the native publication path, camera lifecycle and ownership, replay/freshness gates, logging limits and tests. Three Important findings were returned; no deferred Minor findings were reported. Fixes were implemented in one pass after failing regressions, without a second review round.

1. Native publication stopped permanently on a transient write/rename failure. Initial publication is now best effort, and monitoring continues with 250 ms backoff after a failed publish, 50 ms after success, bounded by the existing 12-hour lifetime. Errors are logged on state transitions. Old snapshots expire in Lua. Tests cover failure before the first publication, failure during monitoring, persistent write/rename failures, and recovery.
2. A setter could partially mutate before throwing or returning an unexpected result. Rollback now captures the immediate finite readback only when a fresh lookup confirms the same current camera identity. Cleanup then reacquires the camera and compares that measured position before restoring the original base. Partial restore failures update the same rollback comparison and remain OFF. Tests inject partial apply and partial restore mutations.
3. Another relative camera writer could leave part of the Explore displacement in the next frame's base, accumulating offsets. An unexpected position which matches neither the owned target nor original base now releases ownership and latches OFF. It preserves the external position and cannot reapply automatically. A repeated external Y adjustment test confirms Z never accumulates. A position already matching the base is accepted as restored.

Tradeoff: legitimate camera changes between callbacks can conservatively latch OFF. This is preferable to accumulating displacement. An ownership conflict does not claim to have restored the original view; the external position remains intact. Immediate post-set readback is local evidence for rollback, not proof of exclusive ownership against arbitrary concurrent engine writers.

Evidence: `red_review.txt` reproduced 7 failing cases (3 Lua and 4 native subcases); `green_review.txt` passed all 20 Explore test methods. The final rebuilt suite passed 48 methods, core 30 checks and observer 12 scenarios. New native host cases are nested within one of the 48 methods.

Generated release Lua was separately executed with enable, 500-frame steady operation, OFF restoration and automatic logging. A second clang/lld build produced byte-identical DLL/Lua outputs; hashes are in `release_checks.txt`.

The reviewer and implementer did not run RE7, Steam Deck, Proton or a real Windows sharing-contention experiment. Existing observer user traces informed conservative readiness rules; they do not validate the new camera writes.
