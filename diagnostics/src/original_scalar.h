#ifndef RE7_OTS_ORIGINAL_SCALAR_H
#define RE7_OTS_ORIGINAL_SCALAR_H

#include <stdint.h>

/* Derived from cheezeit / goldedtoad's RE7 RT 8.02 CT.
 * See diagnostics/NOTICE.md and docs/ORIGINAL_NATIVE_PORT_KO.md.
 * Value-only core, NOT installed hooks. Pointers below refer to caller-owned
 * structs, never game objects. Caller serializes state and resolves lifetime.
 * Requires x87 extended precision / round-to-nearest and no fast math.
 * Axis calls deliberately do not update pitch or focus state: the original
 * CAM_WRITES does those updates between X/Y and Z, while inspection does not.
 */
typedef struct {
    float backdisadjust, behinddis, aimbehinddis, camclipbehinddis, pitchmul;
    float sidedisadjust, sidedis, aimsidedis, camclipsidedis;
    float heightdis, heightdisadjust, camclipheightdis, yadd;
    float headadjust, sideaimadjust, firstyadd, tppitchadj;
} Re7OriginalParams;

typedef struct {
    uint8_t stopflag, internalfpflag, thirdpersonflag, fpaimflag, aim;
} Re7OriginalViewInput;

typedef struct { uint8_t firstpersonflag, recentfpflag; } Re7OriginalViewState;
typedef struct { uint8_t unzoomflag, unzoomsidedoneflag; } Re7OriginalFocusState;
typedef struct {
    float aimtick, sideaimtick, heighttick, minaimbehinddis, maxaimsidedis;
} Re7OriginalFocusRates;
typedef struct { float x, y, z; } Re7OriginalVec;

enum { RE7_ORIGINAL_X, RE7_ORIGINAL_Y, RE7_ORIGINAL_Z };
enum { RE7_ORIGINAL_FOCUS_IDLE, RE7_ORIGINAL_FOCUS_IN, RE7_ORIGINAL_FOCUS_OUT,
       RE7_ORIGINAL_FOCUS_IN_NO_GUN_CHECK };

void re7_original_defaults(Re7OriginalParams *p);
void re7_original_view(const Re7OriginalViewInput *in, Re7OriginalViewState *state);
float re7_original_adjust_pitch(float attempted_pitch, float tppitchadj);
float re7_original_pitch_multiplier(float pitch);
float re7_original_ots_axis(unsigned axis, float base, float yawx, float yawz,
                            float pitch, const Re7OriginalParams *p);
float re7_original_fp_axis(unsigned axis, float base, float yawx, float yawz,
                           const Re7OriginalParams *p);
/* Caller must resolve the original zoom/weapon/scene branch before this call.
 * IN enters zoomaimoldz; IN_NO_GUN_CHECK enters nozoomguncheck directly.
 * These are not equivalent to setting guncheckflag=0 (which skips height/flags).
 * Rates are original timer-produced per-call increments, not per-second rates.
 * This does NOT implement the gun-block transition state machine. */
void re7_original_focus_step(Re7OriginalParams *p, Re7OriginalFocusState *state,
                              const Re7OriginalFocusRates *rates, unsigned action,
                              uint8_t guncheckflag, uint8_t holdinggun);
float re7_original_water_y(float camera_y, uint32_t loadarea, Re7OriginalVec player);
/* Pass a complete live camera snapshot. No pointer dereference, stale cache,
 * or partial replacement. Unavailable/nonfinite snapshot retains original. */
Re7OriginalVec re7_original_firing_origin(Re7OriginalVec original,
                                          Re7OriginalVec camera, int available);
#endif
