# Offline stabilization and completion scope

The user authorized all work that can be completed without a Steam Deck, following the proposed
camera stability / transition evidence / head search optimization / integrated delivery sequence.
Implement these existing flows in work, verify actual production Lua/C offline, independently review,
and publish a usable installer plus GitHub source. No repeated approval or device diagnostic gate.

## Behavior

- F6 OFF always honors the user's cancellation, including stale/malformed native input, focus loss
  and an open REFramework menu. Only enabling retains the fresh/foreground/menu gate. Restoration
  still occurs in engine callbacks; the frame callback never performs engine writes.
- Preserve health, generation, age, distance, jump and ownership thresholds. The stairs incident has
  no current Explore play trace; available uploaded logs are earlier observer 0.2.0 with missing
  camera data. No evidence justifies weakening collision/cutscene-related heuristics.
- Preserve the exact fallback reason on repeated LockScene callbacks. Record actual state changes
  promptly in a bounded scalar queue, flush only from on_frame or reset, and report dropped events.
  Include native age/generations, motion step, camera/player distance and head state when available;
  distinguish current from last observed camera distance. Camera outcomes must survive logging faults.
- Continue periodic samples and rotating 2 x 2 MiB journals, max line 4096 bytes. Add transition
  records with per-run sequence numbers. Identify Lua runs within the native session. No unmanaged
  SDK references in telemetry; no disk I/O in UpdateBehavior/LockScene.
- Provide an offline JSONL summary tool for the two rotating files: exact recorded transitions,
  fallback counts/reasons, ranges, head failures, dropped-event evidence and invalid/truncated lines.
  Deduplicate rotation overlap, group separate sessions/runs, retain a sampled-only label for older
  logs. Never infer the stairs/pillar cause from missing data or count periodic repeats as events.
- Reduce repeated head tree scans using a scalar route cache for reads/head-position queries.
  Reacquire and validate player, ancestry, mesh/material/joint identity on every read. Before EVERY
  write, fully rescan the bounded local-player tree to reject newly added duplicate candidates.
  Clear route on epoch change; replacements never receive an old baseline. No managed userdata cache.
- Complete the two existing offline inventory limitations: normalize LZMA corruption to a clean
  CLI error and reject conflicting casing of implicit parent directories.
- Package Lua v0.4.1-dev with the unchanged native DLL/protocol 0.3.0. Build/source reproducibility,
  production-bundle execution, exact installer payload and checksum verification are delivery gates.

## Limits

This completes the justified offline increment, not all possible RE7 mod features. Engine object
layout, final DrawSelf culling/shadows, actual Steam Deck cost and stair behavior still need ordinary
play later. Wall collision and third-person ballistics require verified engine interfaces/game data;
there is no grounded binding in this project, so adding guessed hooks is outside this increment.
Aim/inventory/interaction continue their first-person fallback. Model assets are not redistributed.
