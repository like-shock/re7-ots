# Offline stabilization implementation plan

> Execute inline with superpowers:executing-plans. One fresh final reviewer; no implementer delegation.

**Goal:** finish grounded offline camera/model improvements and deliver one integrated build.
**Architecture:** retain the native monitor and camera restoration contract; add bounded scalar
transition telemetry, a read-only log summarizer, and a guarded scalar route cache for model reads.
**Tech Stack:** Lua 5.4 / REFramework, Python stdlib unittest and existing clang/lld C build.
**Spec:** docs/superpowers/specs/2026-10-14-offline-stabilization.md.

## Global constraints

- User authorized all device-independent work and GitHub work updates; no repeated approval checkpoint.
- work starts at ba2cbac3b6b5bfeaa38e6f3561709bb39dc0ffc4; existing separate clone provides isolation.
- Native DLL/protocol 0.3.0 unchanged; Lua/package 0.4.1-dev. Head option defaults OFF.
- Engine operations only UpdateBehavior/LockScene/reset; no userdata survives callback boundaries.
- Do not weaken freshness, generation, geometry or ownership gates to speculate about stairs.
- Full target scan before each DrawSelf write; max 128 nodes / depth 12, materials/path exact.
- Event queue 64, drain max 64; JSONL 4096-byte lines, journal 2 x 2 MiB. Log failure is independent.
- No assets, game binaries, user play logs or reference implementation sources committed.

## Review focus

1. F6 cancellation with broken native input; recovery cannot silently re-enable it.
2. A log/format/queue failure during a transient state must not strand the camera offset.
3. New duplicate/reparented/replaced mesh after caching must block incorrect writes.
4. Rotated, truncated, duplicated or mixed-run JSONL must not imply a cause unsupported by evidence.
5. Source installer version/hash consistency and unchanged native monitor compatibility.

## Tasks

### 1. Camera cancellation and transition telemetry
Files: lua/explore/adapter.lua, core.lua, new trace.lua, bundler, tests/test_explore_lua.py,
new tests/test_explore_trace.py and existing Lua harness.
Interfaces: Trace.new(session,run) -> observe(channel,state,t,context), drain(sink), stats().
Only state changes queue events; event_seq and drop totals are scalars. Details capture exact native
reason and current/last geometry; repeated callback reports exact OFF/menu/native/update reason.
- [x] RED F6 OFF during invalid/stale/menu and exact repeated fallback reason; implement GREEN.
- [x] RED immediate transitions between 5 Hz samples, overflow accounting, reset and log-failure
  isolation; implement queue plus adapter plumbing; existing camera/head tests remain GREEN.
- [x] Full task tests and commit.

### 2. Log summary tool
Files: tools/summarize_play_log.py, tests/test_play_log_summary.py.
CLI: paths (1..16 files), --json; stdout report, access/limits errors exit 2. Default Korean text.
Interface: summarize(paths) -> schema1 report grouped by session/run. Read bounds: 8 MiB per file,
64 KiB per line; malformed lines counted, no arbitrary code evaluation or writes. Legacy samples
report sampled state changes separately from explicit transition events; event identities deduplicated.
- [x] RED reason counts, duplicate rotations/mixed sessions, malformed/truncated input, old samples,
  missing/dropped events, CLI and bounded reads; implement GREEN.
- [x] Integrate a real generated-Lua trace fixture into parser verification; commit.

### 3. Model read optimization and offline inventory fixes
Files: lua/model/ethan.lua, tests/model_backend_harness.lua, test_ethan_model.py;
tools/inspect_model_package.py and test_model_package.py.
Interfaces: existing Ethan read/write/head_position unchanged. Cache only epoch/root/target id and
child-index route. Reads validate cached route and selected signature; cache miss/full scan refreshes.
Writes always perform a complete scan and CAS. Add scalar scan counters to model status if useful.
- [x] RED repeated-read unrelated-component visit count, duplicate after caching, route change,
  reparent/replacement/epoch/missing methods; implement GREEN with fewer full scans.
- [x] RED implicit parent case conflict and damaged LZMA archive CLI; fix GREEN.
- [x] Full task tests and commit.

### 4. Delivery
Update README / Korean install and current progress notes, version 0.4.1-dev and evidence ledger.
- [x] Full diagnostics/test.sh, generated autorun exercise, package CRC/payload/hash verification.
- [x] One fresh independent review of the complete base..head range; reproduce/fix Important findings.
- [x] Prepare final sources and installer for delivery, with completed scope and real-game limits.

Final external handoff gates: publish work with an expected-head lease, verify the remote tree, and
provide the installer and commit links. Their results are recorded in the delivery response.

## Ledger

Pre-flight: Task 1 produces transition schema consumed by Task 2; session/run/channel/event_seq,
from/to/lua_clock_s/context and dropped_total will be kept explicit in tests. Task 3 changes no
controller API; Task 4 packages all modules through the existing bundler.
Ruling: proceed under the user's explicit all-offline authorization; no further design/permission round.
Ruling: preserve heuristic thresholds until an Explore trace identifies the stairs fallback cause.
Ruling: cache reads only; full scan before mutation retains duplicate rejection while reducing scans.
Source finding: OFF currently shares the enabling freshness/menu gate and can be ignored during a
native outage. Existing uploaded observer logs cannot identify the later Explore stairs incident.


Tasks 1–3 complete: regression tests observed RED then GREEN for cancellation, repeated-callback
reason, transition queue, summary tool, route-cached reads and both archive limitations. Existing
standalone Ethan.read() full-scan tests are preserved; the lifecycle now uses read_cached(), while
write() always rescans and CAS-checks. This keeps duplicate detection at every mutation boundary.
The native READY label in captured ACTIVE events was also corrected via a failing trace assertion.

Pre-review whole suite: diagnostics/test.sh exits 0; 103 Python tests, 102 passed and 1 optional
private original-CT test skipped. C core 30 checks / observer 12 scenarios pass. Real generated
Lua -> JSONL -> summary tool integration passes, including exact aim/invalid-input fallback counts.

Performance experiment: the same 62 descendant-component lists over 120 steady active frames:
v0.4.0-dev 59,520 component visits; v0.4.1-dev 15,600. Full scans 8 -> 2 per active frame.
This is a synthetic SDK call-count measurement, not a Steam Deck frame-time/FPS result.
Native DLL SHA256 unchanged: 2576e27533380368a52e53db2f21d1893395d42507f78ef33eb5aa83df1cdde7.
Pre-review generated Lua SHA256: d6a0f6fe88a772ee7990303f7e251853e6f07a8e887b9a6450663297bdafb7bb.
Ruling: retain the full-scan read() contract and introduce read_cached() only for the guarded lifecycle;
this preserves prior explicit-read guarantees while reducing repeated readback/position query cost.

Final independent review: immutable ba2cbac3..d645f948, no Critical or Important findings.
The reviewer independently passed 42 focused tests and checked generated Lua/source equality,
DLL/Lua hashes, cancellation recovery and full-scan/CAS preservation before model writes.
One Minor is deferred: conflicting periodic samples are correctly excluded from evidence, but the
top-level conflicts counter counts event conflicts only. A pair with the same session/run/timestamp
and different states yields sample_count=0, insufficient_data and conflicts=0. This does not create
false fallback evidence; the missing sample-conflict count is a reporting limitation.
Review declined actual game layout/rendering, Steam Deck performance and all-address reuse between
native snapshots. These require runtime evidence; no thresholds or identity rules were relaxed.
No production code changed after review; the 103-test full-suite result remains the final code gate.
Both archives pass ZIP CRC/exact-payload checks; the installer has seven entries, verified SHA256SUMS,
Lua version 0.4.1-dev, and no original game/model assets. The final source archive also includes this
review ledger. Source and installer publication is scoped to work, with no main-branch merge.
