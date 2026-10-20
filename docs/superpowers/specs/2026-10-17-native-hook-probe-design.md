# Native original-hook device boundary

User asks to continue until device testing is necessary, without more approval
stops. Original camera/control port is the priority; face hiding is optional.

The next unknown is live hook/object identity and ordering, not scalar math.
The supplied executable hash and diagnostic log prove five code locations match
original instructions, but do not contain the register/object traffic needed to
bind camera, firing, inspection and distance paths. Deliver a device-ready
REFramework plugin that installs those hooks and records their input registers
without changing game-data values. This is an explicitly labeled native hook
probe, not a working original third-person port or a gameplay fix.

Targets: CAM_AFTER_1000_READ, CAM_WRITES, FIRING_COOR_WRITES,
TP_INTERACTABLE_COORDS, DOF_DISTANCE_READ. Match exact executable SHA/PE and
full original bytes at known RVAs before creating or enabling any hook.
Use pinned MinHook v1.3.4, with its license, for relocation/thread coordination.
Record only with game foreground and F6 capture enabled; F6 stops recording.
A session lasts at most 90 seconds and writes at most 8 MiB. Restart is required
for another run. Pin the DLL and retain trampolines until process exit, including
partial-install failures: never free a trampoline a thread may still execute.

Native assembly detours save every register they touch and RFLAGS, capture
XMM values only with movss stores, call no C/API code and do no floating math.
Per-site bounded queues use a nonblocking try-lock; full/busy queues drop and
count records. A worker drains queues, correlates addresses/order and reads
observer values with ReadProcessMemory. Deferred scalar predictions are labeled
asynchronous and not used to write the game. Disk/log work never runs in hooks.
Default state is uninstalled/idle after background validation; F6 requests one
installation/capture. Code patches are process-local; stopping recording keeps
pass-through detours resident until game exit. No persistent game-file patches.

Block if Explore DLL or its autorun exists; exact-byte checks reject existing
patch conflicts. A reversible Steam Deck installer backs up the project's old
Explore/Observer/Diagnostics files, installs only the probe, and restores backups
on uninstall. No model installation/change. Provide one test procedure covering
idle, walking, stairs, wall approach, inspection and aiming if a gun is available.
Logs distinguish missing hooks, queue drops, byte rejection, read failures,
missing frame callbacks and incomplete samples; no 'compatible' verdict based
solely on a signature match.

Offline validation must execute the real detour assembly with sentinel GPR,
flags and FP/SIMD state; validate queue full/busy/disabled paths; verify known
bytes with the existing manifest and relocation decoder; exercise lifecycle
failures with mocked platform hooks; compile Windows DLL; check PE/imports;
test installer rollback/restore and deterministic package manifests. Then one
fresh review, full regression suite, publish work, CI and verified test ZIP.
Device testing is required next because Linux fixtures cannot establish the
actual game's callback object identities/order or Proton thread behavior.
