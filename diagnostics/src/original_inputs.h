#ifndef RE7_OTS_ORIGINAL_INPUTS_H
#define RE7_OTS_ORIGINAL_INPUTS_H
#include <stddef.h>
#include <stdint.h>
#include "native_bridge.h"
#include "original_state.h"

typedef int (*Re7OriginalInputsRead)(void *, uint64_t, void *, size_t);
enum {
 RE7_INPUT_PLAYER=1u<<0, RE7_INPUT_AIM=1u<<1, RE7_INPUT_INVENTORY=1u<<2,
 RE7_INPUT_INTERACTION=1u<<3, RE7_INPUT_LADDER=1u<<4, RE7_INPUT_CHARACTER=1u<<5,
 RE7_INPUT_FOV=1u<<6, RE7_INPUT_WEAPON=1u<<7, RE7_INPUT_CROUCH=1u<<8,
 RE7_INPUT_TIMESCALE=1u<<9, RE7_INPUT_FRAMERATE=1u<<10, RE7_INPUT_TIMER=1u<<11,
 RE7_INPUT_AREA=1u<<12, RE7_INPUT_NOGUN=1u<<13, RE7_INPUT_GRAB=1u<<14,
 RE7_INPUT_TELESCOPE=1u<<15, RE7_INPUT_GUARD=1u<<16, RE7_INPUT_FLASHLIGHT=1u<<17,
 RE7_INPUT_GUNBLOCK=1u<<18, RE7_INPUT_SCENE=1u<<19, RE7_INPUT_MODEL=1u<<20
};
#define RE7_INPUT_REQUIRED (RE7_INPUT_PLAYER|RE7_INPUT_AIM|RE7_INPUT_INVENTORY|RE7_INPUT_INTERACTION|RE7_INPUT_FOV|RE7_INPUT_AREA|RE7_INPUT_TIMESCALE|RE7_INPUT_SCENE)

/* CT RT 8.02 input producers, not game writes. Exact supported hook-site
 * validation and serialization belong to the caller. Capture receives the
 * PRE-INSTRUCTION saved registers, including the original FXSAVE/XSAVE state.
 * State.camera, view publication, interaction flags/lockbox and optional
 * user toggles belong to the runtime; capture does not overwrite those.
 * Runtime must filter CAM_WRITES to its exact camera target before capture;
 * that site refreshes current aim. HEAD reads aim only with nonzero fpaimflag.
 * TIMESCALE does perform the source recentfpflag clear (L3239-3245).
 *
 * valid includes original initialization defaults; observed records sites
 * actually captured. failed is cleared only by a successful corresponding
 * read/default reset. Failed samples retain prior scalar values and become
 * unavailable, never evidence of a safe zero. ready requires current basic
 * pointers/scene facts, excludes optional model checks, and blocks loading.
 * Actual failed memory-backed event inputs retry at the two recurring policy
 * and timescale sites, using only their last producer-derived addresses.
 * Persistently unreadable inputs keep ready false until recovery/reacquisition.
 * Defaults for event-only producers are intentional: ladder/grab absent=0,
 * flashlight=1, framerate=60, Joe character=1 (source EOZ fallback).
 */
typedef struct {
 Re7OriginalStateInput state;
 uint64_t player_base, aim_address, inventory_address, ladder_address;
 uint64_t head_address, grab_address, oldaim_address, flashlight_address;
 uint64_t weapon_address, character_address, crouch_address, telescope_address;
 uint64_t nogun_address, guard_address, guard_r9;
 uint64_t valid, failed, observed, frame;
 uint64_t producer_calls[48], read_failures; /* all supplied site invocations */
 uint32_t heldweaponid, weapon_raw, pcharcflag, frametrack, gbaimcheck;
 uint32_t extendedgrabframes, igtupcheck, updateigt;
 float timescale;
 uint8_t ready, igtpaused;
} Re7OriginalInputs;

void re7_original_inputs_init(Re7OriginalInputs *inputs);
void re7_original_inputs_capture(Re7OriginalInputs *inputs, unsigned site,
 const NativeFrame *frame, const void *fp, uint64_t module_base,
 Re7OriginalInputsRead read, void *ctx, const Re7OriginalStateConfig *config);
#endif
