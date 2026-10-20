# Runtime expansion v0.5.0-dev

The user explicitly requests continuing all work, including features that require later Steam Deck
testing, without asking for intermediate approval. This supersedes the earlier offline-only scope.
The intended outcome is one integrated playable build, not another diagnostic-only device task.
The agent cannot run the user's Steam Deck; implementation and synthetic/CI verification must be
reported separately from actual rendering, wall coverage, combat and performance verification.

## Concrete implementation

1. Extend head control to the supplied Full Model Placeholders RT 1.0 package. Match exact resource
paths/material sets under app.ObjectManager.PlayerObj, never names or Head bones alone. There are
eight model families and nine head parts. Present-Mia has two required parts, pl2102 and pl2120.
Every part keeps its own initial DrawSelf value and compare/write ownership. Partial show failure
hides every owned part; camera restoration always proceeds. Default Ethan-only mode remains
available; full-pack auto-detection is opt-in. A missing companion or mixed family blocks showing.
No model assets are redistributed.
2. Optional experimental camera obstacle avoidance using evidenced via.physics raycasts. Query
the base-to-target segment over the RE7 layers used by the reference (0..32 except 7,18), mask2,
all hits. Exclude local-player descendants by a bounded ancestry walk, take the nearest valid
segment hit, keep 0.12 units clearance, contract immediately and recover outward at a bounded
speed. Zero clearance returns to first-person without weakening other readiness checks. Failed
queries are unavailable, never clear space. This is a center ray, not a volume sweep or guaranteed
wall protection; layer semantics, inside-collider behavior and cost need real-game confirmation.
3. Preserve first-person aiming and game firing coordinates. Add configurable post-aim hold and
camera re-entry blend, preserving defaults (0.5s readiness, 0.15s blend), and an F7 shoulder swap.
Aim observation still has native/file scheduling latency. No claim of frame-exact input protection.
4. UI/config/logs expose selected model group, part states, collision status and tuning. F6 startup
OFF, existing freshness/geometry gates and camera ownership restoration stay intact.

## Runtime boundaries

Native DLL/protocol remains0.3.0 and read-only. Engine operations remain inside existing engine/reset
callbacks. Physics query/result objects are callback-local ordinary add_ref-managed objects;
contacts are copied to scalar tables before result.clear, with no forced releases. Cached model
paths/identities are scalar only and every visibility write performs a complete target scan.
Bounds: model128nodes/depth12; collision31 layers,64 contacts per layer,256 contacts total,
ancestry32; corrupt/missing APIs yield explicit WAIT states. No arbitrary resource configuration.

True OTS firing correction, raw culling patches and full scene/weapon parity are not implemented by
guessing fields. The CT has distinct main/DLC firing-coordinate hooks; no verified managed setter
or lifecycle contract was found. Their dependency is reverse-engineering evidence, beyond merely
running an already specified feature on the device. Record this gap without asking the user to approve.

## Evidence

Full model archive SHA256 af750097e2486238370015aa87794a8014f77e611139451249f6afe53699b4b2,
83entries/35,735,307 expanded bytes. Read-only mesh analysis uses the same external RE-Mesh-Editor
revision as ETHAN_RT_MODEL_ANALYSIS_KO.md. Only resource signatures/facts enter this repository.
REFramework/EMV references and exact API details will be recorded in RUNTIME_EXPANSION_KO.md.

## Completion

All existing tests remain, new regressions exercise actual Lua with SDK boundaries simulated,
whole-suite build, one independent review, verified installer/source archives, publish work using
expected-head lease and verify actual GitHub CI. Runtime limitations remain explicit in the guide.
