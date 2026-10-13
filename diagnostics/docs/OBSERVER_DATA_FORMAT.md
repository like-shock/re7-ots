# Observer data contract — native v0.2.0 / camera v0.2.1

All files use JSON Lines (one JSON object per LF-terminated record), UTF-8/ASCII.
`status.json` is a bounded, replace-in-place UI snapshot. A reader must tolerate partial JSON and require a changing session/tick/sequence before treating it as live.

## Fixed paths

Offsets are hexadecimal. `*` reads an 8-byte pointer. Leaf values are copied via ReadProcessMemory, not dereferenced by C.

| Field | Path from module base | Leaf | fail_step |
|---|---|---|---|
| player | *(base+8FB8018) → *(+D8) → *(+18) | +30: 3 float32 xyz | pointer reads 0..2; leaf 3 |
| aim | *(base+8FBB610) → *(+20) → *(+88) → *(+10) → *(+A8) | +20: uint8 | pointer reads 0..4; leaf 5 |
| inventory | *(base+8FAC380) → *(+208) → *(+58) → *(+148) → *(+30) → *(+48) | +10: uint8 | pointer reads 0..5; leaf 6 |
| interact | base+8FB17F0 | 8 raw bytes, no chain | no chain step |

CT source: original re7.CT L1182–1212; aim comparison to 1 occurs around L1353; inventory comparison to 1 and low16 interaction comparison around L3422–3433. Other interaction uses compare low32 around L1265. Source CT SHA is in NOTICE.md and HANDOFF.md.

READABLE_CANDIDATE only means bounded reads succeeded and basic numeric constraints held. player float32 must be finite and within ±1,000,000 (broad sanity bound, no claim about game units). aim/inventory accept only 0 or 1; unusual raw byte remains in INVALID_VALUE for diagnosis. Zero interaction slot is a valid raw value. Low16 and low32 interpretations remain separate.

`generation` increments on first valid read, recovery after invalidity, or a changed pointer path. It is scoped to one recorder session. It does not certify object identity, object lifespan, reload completion, or ABA absence. Links are rechecked after payload copy, but reads are not atomic.

## Native records

- `header`: schema, version, session, limits, no-write and semantic caveats.
- `build`: exact executable SHA and validation result. Fixed-offset reads occur only after expected SHA and memory PE metadata match.
- `sample`: seq, tick_ms (GetTickCount64), elapsed_ms, stage, player/aim/inventory results, interaction raw slot and CT comparisons.
- `end`: STOPPED (user), LIMIT (cap), UNSUPPORTED (build), or ERROR; reason and sample count.

On I/O failure the native trace can lack an end record. Status/UI may also fail if the destination is unwritable; REFramework error log is the remaining channel. No complete trace is inferred from a filename.

## Camera records

The v0.2.1 Lua-only patch accepts native schema 1, version 0.2.0. Its header reports `version: 0.2.1`, `native_version: 0.2.0`, and `getter_path: primary_camera -> get_GameObject -> get_Transform`. Native DLL and record format are unchanged.
The primary Camera component supplies `get_GameObject` (including inherited methods); the owner supplies `get_Transform`. Each sample resolves both anew. A missing method is `UNKNOWN_GAMEOBJECT_METHOD` or `UNKNOWN_TRANSFORM_METHOD`; a nil owner is `UNKNOWN_NO_GAMEOBJECT`. No failed sample retains coordinates.

The camera file has its own header/sample/end. `native_seq` and `native_tick_ms` identify the latest native snapshot observed by Lua. Camera capture happens later in the post UpdateBehavior callback, so pairing is not atomic and neither equality nor causality should be assumed.
`lua_clock_s` is the local Lua os.clock (or os.time fallback), not the native monotonic clock. `native_status_age_s` measures time since Lua last saw the token change, not exact acquisition latency.
Coordinates are world transform xyz and quaternion xyzw from documented REFramework accessors. Norm squared 0.5..1.5 is a broad sanity condition; values are not normalized or written back.
Only camera/transform address strings remain between samples; no game object is cached by this script. REFramework may itself retain wrappers internally as part of normal reference management.
Camera errors and nil/missing API/type/values are UNKNOWN, with old coordinates omitted.
