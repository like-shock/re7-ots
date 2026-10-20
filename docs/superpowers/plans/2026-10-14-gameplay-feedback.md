# Gameplay feedback correction ledger

Base: 5e5eb22e51953e54a566c12df0e5f9abe42e5962; existing work checkout.
User authorizes continued implementation, device-dependent work and work-branch publication.
Execute inline with regression tests and one fresh final review; no additional permission gate.
Raw gameplay logs remain outside the repository.

## Evidence and scope

User confirms Ethan model rendering and shoulder switching. Reports displaced world A prompts,
off-center interactions, hands behind neck against walls, and repeated FP/OTS transitions.
Two logs: 1127 samples; collision disabled in 1126; seven WAIT_NATIVE_INVALID transitions recover
to a fresh native sample about 67ms later and restart the 0.5s warmup. Four occur at unchanged
player position. Exact file-read failure was not recorded. Native writes already use temp/rename.
Model controller never bound: WAIT_MODEL_COMPONENTS949 / WAIT_MODEL_NATIVE177 / DISABLED1.

REFramework d052e8fd: SystemArray.get_elements returns a C++ vector, not a guaranteed Lua table.
Use bounded get_size/get_element rather than table-only enumeration. Model body/arms remain
outside head signatures. RE7.lua identifies PlayerHandTouch.HandIK and Target/TargetGameObject
for read-only evidence. No verified safe arm-only correction exists yet.

Translation-only OTS necessarily produces interaction-ray parallax. HUD phase ordering is a
separate hypothesis: PrerenderGUI precedes LockScene in Enums_Internal.hpp. Test an opt-in early
application path, retain ownership checks and record phase camera/joint positions. Do not claim
this changes native targeting, fixes ordinary A prompts or resolves hand animation offline.

## Tasks

- [x] RED/GREEN same-poll status retry: max2 attempts only for nil/error; decoded invalid data
  still reaches unchanged gate immediately. No old-snapshot freshness extension. Log outcomes.
- [x] RED/GREEN indexed component enumeration: count validation before reads, reject nil holes,
  64 component limit, preserve duplicate/signature/ancestry/epoch safeguards.
- [x] RED/GREEN opt-in early camera application before PrerenderGUI, late verification and
  missing/reordered/duplicate callback guards; bounded read-only phase/hand diagnostics.
- [x] Version/docs/log summary, full integration verification, immutable review; no fix pass needed.
- [ ] Publish work and verify CI-built installer/source provenance.

## Verification and limitations

Offline checks must exercise the SDK array shape and callback sequence, not just table mocks.
Actual screen/HUD alignment, hand animation and file-read recovery rate remain device evidence.
Record new outcomes without asking the user to repeat already confirmed model/shoulder tests.

## Execution evidence

Pre-flight: retry produces only a decoded value for unchanged Core.ingest; array enumeration is
shared by every head profile and before-write rescan. Early application consumes the existing
camera ownership path, and LockScene must verify rather than apply a second offset. Probe captures
only copied scalar data, never writes, and uses the model's current validated epoch for player reads.
The cloud helper/template resource was unavailable; this committed plan is the implementation and
review ledger. No separate spec: the user's four symptoms and the Evidence and scope section bind it.

Task1/2 complete: observed RED for one-shot/persistent read handling and indexed SDK arrays;
GREEN for4 snapshot tests,12 backend tests,11 existing camera/head tests and7 model-family tests.
Task3 complete:6 camera phase and4 scalar probe cases RED then GREEN; added early-camera/head
cleanup integration. Full test.sh with local original CT: C30/observer12, Python/Lua154/154 pass.
Full-suite failures exposed the package fixture's missing new module and disabled-model diagnostic
reads. Fixture now bundles Probe; player diagnostics require model opt-in + current valid epoch,
preserving zero player SDK reads for disabled model mode. Existing log summarizer accepts the new
fields without changes; confirmed it still reports7 invalid-native transitions from supplied logs.

Ruling: keep early GUI timing opt-in and unchanged native targeting — source evidence establishes
possible timing mismatch and unavoidable translated-ray parallax, not a verified HUD/selection
correction — cost: the A prompt and off-center selection may remain unchanged in the next build.
Ruling: record hand IK/motion without arm/body writes — no safe arm-only object signature is proven
and the full model shares arm/body geometry — cost: hands-behind-neck remains unresolved.

Review focus: same-poll retry never bypasses identity/freshness/health; array scans remain exhaustive
within explicit bounds; early duplicate/missing/reordered callbacks cannot double offset or clobber
external pose; fallback/reset hides owned heads before restoring camera; optional diagnostics cannot
fault camera or leak SDK objects; package/version/docs accurately separate fixes from hypotheses.

Final review: fresh gpt-6-astra/high reviewer checked immutable5e5eb22..bd56ede; no Critical,
Important or actionable Minor findings. Independently ran154 tests (153pass, optional private CT1skip)
and checked pinned SDK array bindings and callback enum order. Parent test.sh ran154/154 including CT,
C core30 and observer12. Local package CRC, all manifest entries and commit/tree provenance pass.
Final: Ruling: runtime HUD/selection, hand animation, head visibility, actual retry recovery rate,
native SDK behavior/performance remain unverified — retain strict gates, read-only probes, optional
early mode and explicit limits — cost: visual defects, unavailable APIs or performance issues may
remain on the device. Published artifact provenance is checked separately after authorized push.
No production changes followed the final review. Only this ledger records review/publication status.
