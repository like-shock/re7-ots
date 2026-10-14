# Runtime expansion implementation ledger

Plan/spec: docs/superpowers/specs/2026-10-14-runtime-expansion.md.
Base:2305131fe578b6561f774870396a48031a5a7a1c. Existing isolated work checkout is clean.
Execution: inline, TDD, one fresh final reviewer. User explicitly authorizes uninterrupted
implementation and work publication; no additional design/release permission gate applies.
Cloud skill helper scripts are not available locally; this committed file is the task ledger.

## Task1 — multi-part RT models

Files: model/ethan.lua (verified per-part backend + scalar discovery), new model/group.lua,
model/session.lua integration, explore/core.lua settings, adapter/bundler, test_rt_models.py,
model_backend_harness.lua and existing integration fixtures.
Interface: Ethan.new(sdk,profile_name default ethan); Ethan.discover(sdk,epoch) returns scalar
family/part names or reason. Group.new(sdk) mirrors HeadSession configure(enabled,epoch,full_pack),
run, before_restore, after_apply, status. Per-piece existing Visibility controllers keep ownership.
Discovery is bounded and throttled0.5s on failure, cleared for native epoch/mode changes.
- [x] RED full signatures, all model families, required Mia companion, duplicates/mixed families,
  no body/arm/NPC writes, independent original values, partial show cleanup, epoch/mode/disable.
- [x] Implement; existing standalone Ethan contract/tests remain; run focused+whole suite; commit.

## Task2 — obstacle avoidance

Files: new explore/physics.lua (SDK boundary), obstacle.lua (scalar math), tests/test_obstacle.py,
physics harness, adapter/core settings/bundler and generated adapter scenarios.
Interface: Physics.new(sdk):cast(origin,target) -> scalar {status=clear/hit/unavailable,distance,
reason,casts,contacts}; Obstacle.new():limit(base,target,hit,time,key,clearance) returns scalar target
and status. Failed API/count/ancestry/contact validation cannot masquerade as clear.
- [x] RED closest wall after player contacts, reversed/invalid/off-segment positions, saturation,
  missing methods/errors, reset, callback ordering, immediate contraction and bounded recovery.
- [x] Implement guarded center-ray adapter; zero/failed clearance hides model and restores camera;
  record collision evidence; whole suite; commit.

## Task3 — playable transition controls and integrated delivery

Files: core.lua/adapter.lua settings, tests/test_explore_lua.py, build/docs/version0.5.0-dev.
Interfaces: ready(time,settings) observes aim_release_delay; entry_blend config; F7 rising edge swap
only with fresh foreground input and closed menu. Defaults preserve existing transitions.
- [x] RED aim release hold/repeated aim, invalid config, F7 hold/menu/focus/disabled behavior,
  settings persistence/reset, failure isolation; implement GREEN; whole suite.
- [x] Update Korean install/remaining evidence, model/physics provenance and version; commit.
- [x] One immutable-range review; no Important fixes required; record Minors.
- [x] Prepare verified installer/source packaging and authorized work delivery.
  Final remote CI/artifact verification is recorded by the Actions run for the published commit.

## Review focus

1. Multiple model parts never share a boolean original or restore into replacements.
2. Missing/new/duplicate companions and mixed model families cannot receive an unsafe show.
3. Physics errors, layer/filter uncertainty, missing owners and excessive contacts fail conservatively;
   local-player exclusion never excludes the scene root or arbitrary walls.
4. Collision and model failures cannot block camera restoration on aim/OFF/stale/fault/reset.
5. SDK values do not escape callbacks, no unsafe native game writes, bounded loops and references.
6. New controls do not relax native identity/freshness/generation gates; defaults remain compatible.

## Decisions and evidence

Pre-flight: Task1 Group replaces only model lifecycle facade. Task2 consumes target from Core.offset
before camera setter, returns scalar geometry/status. Task3 settings are passed to existing gates;
all new modules precede adapter in generated bundle.
Ruling: implement with user authorization despite pending real-game tests; experimental options
are explicit and default OFF. Cost if layer/model assumptions differ: unavailable mode or visual
defects requiring runtime evidence, not a false verified-rendering claim.
Ruling: preserve first-person aim and do not invent bullet/culling setters; CT raw hooks differ by
mode and lack a validated Lua contract. Cost: full original CT firing/culling parity remains open.
Ruling: keep native observation/identity gates for DLC too; a verified model asset alone does not
prove native chains work in that DLC. Cost: some DLC scenes may remain unavailable until evidenced.

Task1: full profiles and per-piece lifecycle implemented. Seven group tests (including six individual
non-Ethan profiles) and a generated-adapter Mia/camera test observed RED then GREEN. Existing tests
also exposed fault reporting lost during successful cleanup; the group now retains the part fault.
Replacement/disable regression found HeadSession returned success after failed release: corrected
to return true only for actual DISABLED state. Full suite119 tests:118passed,1optional CT skipped;
core30/observer12 pass. Every show validates the whole discovered family before the per-part CAS.

Task2: query adapter and scalar limiter implemented, with actual adapter wiring. Ten new physics,
math and camera cases observed RED→GREEN. Full suite129:128pass/1optional CT skip, C30/observer12.
Additional head/collision integration case passes with all11 head adapter tests: a contracted camera
hides the head before FP restore; failed physics cannot block cleanup. Query counters/contact
positions stay scalar; 31 layers,64 contacts/layer,256total,32 ancestry steps are enforced.

Task3: configurable post-aim hold and entry blend, guarded F7 edge input, offsets-only reset,
UI/version0.5.0-dev and Korean installation/evidence documentation implemented. Five new transition
cases observed RED→GREEN. A polling test was corrected to advance beyond the50ms sample interval:
invalid disk content cannot affect input before it is observed. Full suite135:134pass/1optional CT
skip, C30/observer12. Existing camera/model/packaging/log tests also pass. Real device evidence is
explicitly pending, without blocking delivery or asking for another approval.

Final review: fresh gpt-6-astra/high reviewer, immutable2305131..e5f8f3a, no Critical/Important
findings. Reviewer reran135 tests:134pass/1skip. No production edits during or after review.
Final: minor (deferred): aim-release hold uses the last aiming observation, not the first released
observation; observed release-to-ready time can be shorter by the sample interval. Existing0.5s
warmup is unchanged, and the setting documentation explicitly describes the last-observed basis.
Final: Ruling: runtime rendering, empirical layer coverage, inside-collider behavior, Steam Deck
performance and DLC native chains cannot be judged offline — retain experimental opt-in controls,
existing native gates and unverified status — cost: visual clipping, missing support or performance
regressions may still require real game evidence; do not claim they are resolved.
Final verification: rebuilt DLL/Lua with clang/lld18 and ran test.sh with the supplied private CT
via RE7_CT_FILE: core30/observer12 pass, Python/Lua135/135 pass, zero skipped. Normal public CI still
skips the one private-CT case. Original archive/model assets remain excluded from publication.
Installer/source packaging passed CRC/payload/manifest verification on the reviewed code. Publish
work only, preserving main and the existing checkout; user already authorized repository management.
