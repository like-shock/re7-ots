# Ethan RT head binding implementation plan

> Execute inline with `superpowers:executing-plans`; one final independent review.

**Goal:** connect the supplied Ethan RT head model to the existing camera without moving bones.
**Architecture:** select a head-only GameObject under the current local player by exact mesh resource
path and five material names, then control DrawSelf with the existing visibility lifecycle. Show only
after camera readback and >=0.60 m from the head joint. An opt-in setting persists installation intent;
camera activation still starts OFF. No asset files are committed or redistributed.
**Tech:** Lua/REFramework reflection, Python unittest with real Lua 5.4, existing C build.
**Spec:** `docs/MODEL_SUPPORT_DIRECTION_KO.md`, amended by the supplied model analysis below.

## Asset evidence

RAR SHA256 `231bb2c129eb52eff7d78a1ea09ae456bbdd1646eba24d8504d3549ec2687cfb`.
`pl0001.mesh.220128762` is body (158 bones, body/leg/VR materials).
`pl0002.mesh.220128762` is head (253 bones; face, eyes, hair, tears, eye shadow).
Both have internal MESH version 21041600. Face: 9,800 vertices; 456 outside the Head subtree,
528 with mixed Head-subtree/other influences. Thus moving only Head is not a complete hide.
Analysis used RE-Mesh-Editor commit `622daa75b41ec622c3444ecb44ef31a651692687` in scratch;
no GPL code or geometry is copied into this MIT repository. Store hashes/counts/conclusions only.

## Grounded runtime interfaces

- REFramework `d052e8fd...` scripts/utility/RE7.lua: app.ObjectManager.PlayerObj.
- REFramework shared/sdk/RETransform.cpp: getJointByName, via.Joint.get_Position/get_Owner.
- EMV-Engine `a8a89961fde640d542afc72bd66fd8e12f3b82b3` init.lua:
  get_Child/get_Next; get_Components; Mesh.getMesh; resource ToString();
  get_MaterialNum/getMaterialName; GameObject.get_DrawSelf/set_DrawSelf.
  This is API usage evidence, not proof that DrawSelf alone defeats RE7's other culling flags.
- Reflect every method/type before use; absent methods/targets produce a visible wait/fault.
- Never use EMV's raw draw-byte offsets, UpdateSelf toggles or global scene search.

## Global constraints

- Existing camera gates, native monitor and native protocol version 0.3.0 stay unchanged.
- Experimental Lua version 0.4.0-dev; head mode defaults disabled, user enables after model install.
- Identify exactly `character/player/pl0000/pl0002/pl0002.mesh` and the five `pl0050_*` materials.
- Never change the body/root object. Require exactly one Mesh on the candidate GameObject.
- Only numeric addresses/routes/epoch strings survive callbacks; reacquire objects before each write.
- Bound local-player descendant traversal to 128 nodes, depth 12; reject cycles/ambiguous matches.
- DrawSelf changes preserve animation and descendants; do not claim to restore shadows separately.
- On first-person fallback, hide before camera restore. Head failure cannot prevent camera restore.
- On script reset, hide and leave hidden (original visibility restoration could put a face inside FP).
- Explicit head-mode disable restores owned original values; document that the model itself remains installed.
- No immediate Steam Deck test request. Runtime object layout, visual result and culling remain unverified.

## Tasks / ledger

### 1. Head-only backend and asset profile

Files: `diagnostics/lua/model/ethan.lua`, `diagnostics/tests/model_backend_harness.lua`,
`diagnostics/tests/test_ethan_model.py`, `docs/ETHAN_RT_MODEL_ANALYSIS_KO.md`.
API: `Ethan.new(sdk)` -> backend with `set_epoch(string)`, `read()`,
`write(id,expected,desired)`, `head_position(id)`, `note`.
Read/write use the visibility module contract (false rejects before write). Match path, materials,
local-player ancestry and component uniqueness. Cache a scalar child-index route, not userdata.
Tests must prove decoy NPC/body meshes survive, ancestry/path/material/method mismatch blocks writes,
duplicate matches and malformed hierarchies block, CAS rechecks epoch/identity/current value,
head position comes from the selected transform and cached route is revalidated.
- [x] RED backend tests, implement GREEN: 6 tests, including malformed trees and replacement.

### 2. Camera lifecycle integration

Files: `diagnostics/lua/model/session.lua`, Explore adapter/core/bundler, Lua test harness/tests.
API: session `configure(enabled,epoch)`, `before_restore()`, `after_apply(camera_xyz)`,
`reset()`, `status()`; controller re-created on epoch change, faults remain latched in same epoch.
Session errors independent from camera. Source config adds `ethan_head` boolean (default false).
Adapter calls before_restore from every camera restoration path, after_apply only after verified
camera write, and reset keeps head hidden. UI uses opt-in checkbox; journal records model status.
Tests: show follows camera write; F6/aim/inventory/interaction/stale/menu fallback hides first;
near camera stays hidden; failure cannot block camera cleanup; generation change reacquires;
disabled mode has no model SDK reads, config persistence does not auto-enable camera.
- [x] RED integration tests, implement GREEN: 8 tests with fallback/opt-in/native subcases.

### 3. Documentation and verification

Update root/current install docs and development version packaging metadata (native protocol unchanged).
Document Fluffy model installation, opt-in, limits, first-person hide, restore vs reset distinction.
Run full `diagnostics/test.sh`; inspect bundle and archive payload, no third-party assets.
One independent final review; fix Important findings via RED→GREEN. Publish work with lease check.
- [x] Full suite, packaging and one independent review complete; ready for leased GitHub work update.

## Review focus

1. Cached route follows a replaced/reparented object: no stale writes or old-baseline restore.
2. Head DrawSelf is reset by the game: fault/camera cleanup must stay independent.
3. Several meshes or several matching heads: do not pick the first ambiguous target.
4. Stale native epoch while loading: show remains gated, cleanup never follows a reused address blindly.
5. Exceptions on fallback/reset: preserve camera restoration even when model operations fail.

Ruling: continue under the user's existing development/GitHub authorization; the attachment supplies
the missing asset. Use the existing isolated work branch and inline execution. No new approval gate.
Ruling: use DrawSelf on the separate head mesh rather than joint displacement, because the supplied
weights prove Head-only displacement is incomplete. If DrawSelf is overridden in game, a later
binding change is required; no raw memory fallback is silently introduced.


## Implementation ledger

Task 1: complete — backend tests first failed for the absent module, then all six passed.
Task 2: complete — integration tests first failed for the absent session module, then all eight passed.
A later regression test exposed acquisition from unsupported/stale native input; added the native
build/freshness gate and observed RED→GREEN. Existing 18 camera/core/journal tests remain green.

Ruling: re-scan the bounded player subtree on every resolution instead of caching a child route —
otherwise a new duplicate elsewhere could go unnoticed — cost: extra SDK reads, performance unmeasured.
Ruling: use the same before_restore path for script reset, with no original-value release —
this keeps an enabled head binding hidden in first person — cost: hidden DrawSelf survives script unload.
Ruling: cloud skill companion scripts/template were not available via the resource provider in this
session; this checked-in plan is the task brief/ledger, with direct test commands and one fresh reviewer.

Pre-review verification: full diagnostics/test.sh exits 0: 81 Python tests, 80 passed,
1 optional original-CT extraction skipped; C core 30 checks and observer 12 scenarios passed.
Native DLL SHA-256 remains 2576e27533380368a52e53db2f21d1893395d42507f78ef33eb5aa83df1cdde7.
API/asset evidence and unverified runtime conditions are recorded in ETHAN_RT_MODEL_ANALYSIS_KO.md.


## Final independent review and fix pass

Fresh reviewer inspected 7ccb79f..d416f46 and independently ran 59 Lua/model tests.
No Critical findings. One Important finding: health-invalid native snapshots could contain a
scalar player; model epoch preparation indexed it before camera cleanup, blocking fallback/reset
with head mode enabled OR disabled.

Final: fixed malformed-player cleanup — test_malformed_player_cannot_block_fallback_or_reset
and test_malformed_player_keeps_owned_head_cleanup_available RED→GREEN. Numeric/boolean player
values failed fallback and reset before the fix. Type-check the player and include all epoch
preparation inside the model exception boundary. Strings, nil and invalid generations are covered.
The acknowledged EOF whitespace warning was also removed; git diff --check is clean.
No additional Minor findings were raised.

Final: Ruling: reviewer declined to judge actual RE7 layout, DrawSelf/culling/shadows and rescan
performance without the game — ship this as opt-in experimental development code with those limits
explicitly documented — cost: a later runtime binding/performance adjustment may be necessary.

Final verification: diagnostics/test.sh exits 0: 83 Python tests, 82 passed, 1 optional private
original-CT extraction skipped. C core 30 checks and observer 12 scenarios pass.
Native DLL SHA-256 is unchanged. Generated Lua SHA-256:
8d89901c50f80a1af6ba05fd9733138c0fcdae843f49af91944c5aec5b4125ed.
Generated autorun is exercised directly for 500 frames, F6 OFF/reset and malformed-player cleanup.
Installer/source ZIP CRC and exact payload checks pass; no external model asset is included.
Publish the reviewed change plus this verified fix to work using an expected-head lease; no main merge.
