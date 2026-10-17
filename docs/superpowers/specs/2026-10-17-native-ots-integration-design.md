# Original native OTS integration

The user approved continuing the original native port without a separate probe
run. Existing byte diagnostics and the original CT define enough behavior to
implement first; live validation remains necessary for the resulting gameplay
build. This supersedes the probe-only development boundary, not its test results.

Deliver a separate RE7_OTS_Native.dll device-test plugin, an installer that backs
up Explore/Probe, and integrated bounded diagnostics. F6 toggles camera control;
F7 changes shoulder. Start disabled. No Lua or SDK model discovery in the native
camera path. Preserve installed model assets. First-person face hiding is omitted.

## Control and correctness

Use the exact RT executable hash and original full instruction bytes. Capture
camera addresses at CAM_AFTER_1000_READ; only the matching CAM_WRITES target may
receive original OTS coordinates. Preserve axis ordering and original view,
focus, scripted-state and weapon-block policy. Port source-determined state
producers, interaction pointer comparisons and DOF collision control. Collision
distance changes camera distance; it must not invent a first-person toggle.
Model transparency uses original native hooks; no facial material traversal.

The bridge saves GPRs, flags and enabled processor extended state before calling
native C. Callback arithmetic uses a clean FP environment, then restores the
game environment. Modified blocks reproduce original register outputs as well
as stores. Failure before a complete replacement resumes the original block;
do not partially replace register results. Read/write adapters use only the
current process. Original address checks and failed-read fallback are explicit.

Shared native state is serialized; no SDK callbacks/file I/O occur inside hooks.
Log bounded aggregate snapshots outside hooks. Preserve exact original timing
where source supplies it; label any deliberately unsupported optional path in
the delivery notes. Do not claim complete CT equivalence, performance gains or
fixed gameplay before the device test.

## Packaging and verification

Reuse pinned MinHook and retain published trampolines/DLL until process exit.
Reject mismatched bytes and competing project plugins. Installer and rollback
must preserve unrelated files and model assets. Source hash/commit/DLL manifests
tie the device ZIP to successful CI.

Tests execute the bridge with register/FP sentinels, source-derived state and
collision cases, nonmatching and invalid pointer targets, original block output
equivalence, toggling/scene resets, installation failure and package recovery.
An independent final review precedes GitHub publication and verified ZIP delivery.
