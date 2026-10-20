#ifndef RE7_OTS_ORIGINAL_STATE_H
#define RE7_OTS_ORIGINAL_STATE_H
#include "original_scalar.h"

/* Derived from cheezeit / goldedtoad's RE7 RT 8.02 CT. See diagnostics/NOTICE.md.
 * Pure caller-owned state: no process reads, OS calls, model writes, or hooks.
 * Caller serializes invocations and supplies a coherent live snapshot. No
 * camera/player-distance fallback or model-visibility prerequisite is used.
 */
enum { RE7_ORIGINAL_CAMERA_TP, RE7_ORIGINAL_CAMERA_FP };
enum { RE7_ORIGINAL_WEAPON_OTHER, RE7_ORIGINAL_WEAPON_GUN, RE7_ORIGINAL_WEAPON_MELEE };
enum { RE7_ORIGINAL_MELEE_AXE, RE7_ORIGINAL_MELEE_SAW, RE7_ORIGINAL_MELEE_KNIFE,
    RE7_ORIGINAL_MELEE_CROWBAR, RE7_ORIGINAL_MELEE_CHAINSAW, RE7_ORIGINAL_MELEE_SURVIVAL_KNIFE,
    RE7_ORIGINAL_MELEE_GOLD_CROWBAR, RE7_ORIGINAL_MELEE_CHRIS_KNIFE, RE7_ORIGINAL_MELEE_BEDROOM_KNIFE,
    RE7_ORIGINAL_MELEE_AMG78A, RE7_ORIGINAL_MELEE_JOE_FISTS, RE7_ORIGINAL_MELEE_SPIRIT_BLADE,
    RE7_ORIGINAL_MELEE_AMG_DUAL, RE7_ORIGINAL_MELEE_COUNT };
enum { RE7_ORIGINAL_CHAR_ETHAN, RE7_ORIGINAL_CHAR_MAIN_CLANCY, RE7_ORIGINAL_CHAR_PRESENT_MIA,
    RE7_ORIGINAL_CHAR_PAST_MIA, RE7_ORIGINAL_CHAR_DLC_CLANCY, RE7_ORIGINAL_CHAR_CHRIS,
    RE7_ORIGINAL_CHAR_JOE, RE7_ORIGINAL_CHAR_ZOE, RE7_ORIGINAL_CHAR_COUNT };
enum { RE7_ORIGINAL_AD_ETHAN_MAIN, RE7_ORIGINAL_AD_DERELICT_TAPE,
    RE7_ORIGINAL_AD_BIRTHDAY_TAPE, RE7_ORIGINAL_AD_OLDHOUSE_TAPE,
    RE7_ORIGINAL_AD_MIA_SHIP, RE7_ORIGINAL_AD_PAST_SHIP, RE7_ORIGINAL_AD_NIGHTMARE,
    RE7_ORIGINAL_AD_BEDROOM, RE7_ORIGINAL_AD_21, RE7_ORIGINAL_AD_DAUGHTERS,
    RE7_ORIGINAL_AD_NOT_A_HERO, RE7_ORIGINAL_AD_END_OF_ZOE, RE7_ORIGINAL_AD_JACK55,
    RE7_ORIGINAL_AD_ETHAN_MUST_DIE, RE7_ORIGINAL_AD_COUNT };

typedef struct {
    uint8_t adisablecheck, thirdpersonflag, fpaimflag, fptptransitionsflag;
    uint8_t zoomaimflag, guncheckflag, grabzoomin, truegbusesfocus, gbaimdelay;
    uint8_t fovcheckflag, inventorydisable, loccheckflag, autodetectmodel, fpmeleeflag;
    uint8_t fpflashlight, manualflashflag, critterexamine, tpinteractables;
    uint8_t adonlygrabs, floorgrabad, gbflag, gbcrouchdisable, noclipautodisable;
    uint8_t ffscriptallowfp;
    uint8_t tp_melee[RE7_ORIGINAL_MELEE_COUNT];
    uint8_t autodisable_character[RE7_ORIGINAL_CHAR_COUNT];
    uint8_t autodisable_location[RE7_ORIGINAL_AD_COUNT];
    float minaimbehinddis, maxaimsidedis, aimforwardzoomrate;
    float gbheadhidedis, gbheadhidediscrouch, gbfowardaimrate, gununblockdelay;
    uint8_t runningfov;
    float maxrunpulldis, runbaserate, sideminrunpulldis, siderunbaserate;
} Re7OriginalStateConfig;

typedef struct {
    /* Raw source values, not generalized bools: literal cmp byte semantics
     * matter. firstperson/recentfp are the HEAD_Y_WRITE publication (1341+),
     * not recomputed here; CAM_WRITES transitions deliberately lag it. */
    uint8_t player_available, flycamflag, firstpersonflag, recentfpflag;
    uint8_t aim, inventory, ladder, crouchflag, holdinggun, holdingmelee;
    uint8_t telescopeflag, guardflag, flashlightflag, interactableflag, lockboxflag;
    uint8_t bodybagarea, loadscreenflag, moldgrabflag, nogunaccess, itempickuppop;
    uint8_t aimblockedflag, ffscripton, ffscriptfpexception;
    /* Optional model-marker read: never require this to select thirdperson.
     * HEAD_Y address+0x53c, or +0x4bc for main/DLC Clancy (7587-7601). */
    uint8_t model_marker_available;
    uint16_t flash;
    uint32_t loadarea, characters, inexitdelay;
    /* Read pointer VALUE at re7.exe+0x8fb17f0. Most original checks use only
     * low 16 bits; bedroom clock uses a direct memory dword compare. */
    uint64_t interaction_pointer;
    /* camera is the raw CAM_WRITES attempted XYZ, not the displaced render
     * pose. Final-boss/floor-grab guards compare cam*attempt in the CT. */
    Re7OriginalVec player, camera;
    float autooldcamfov, igtimer, framerate, model_marker;
    uint8_t runflag;
    uint32_t frametrack; /* TIMER_AOB counter, not render-hook invocation count. */
} Re7OriginalStateInput;

typedef struct {
    uint8_t stopflag, blockedgun, internalfpflag, truegbflag, fakegbflag;
    uint8_t gbprocessflag, gbdelayset, gbunzoomingprocessflag, gunblockunzoombackdoneflag;
    uint8_t stoppedbloodhide, assumeheadless, badtransitionflag, spiderpuzfpvhsexc;
    uint8_t timertripflag, finaligtimereset;
    uint32_t readjustxyzfp, gununblockdelay2, bedclockframes;
    float starttime, endtime, finaligtimer, finaligtimepassed;
    uint32_t frametrackfovrecord;
} Re7OriginalState;

typedef struct {
    Re7OriginalFocusRates focus;
    float gbaimtick, gbsideaimtick, gbheighttick;
    float clippedgbheadhidedis, clippedgbheadhidediscrouch;
} Re7OriginalStateRates;

void re7_original_state_defaults(Re7OriginalStateConfig *config);
/* HELD_WEAPON_CMP: ID is dword [rbx+0x3b0], compared unsigned (2858-3018).
 * The return is an exclusive classification; other includes TP-enabled melee. */
unsigned re7_original_state_weapon(const Re7OriginalStateConfig *config, uint32_t id);
/* TRUE_FOV_READ frame-gated calculations, 2608-2703. Rates are per camera call;
 * invoke at the original once-per-frame point, not each individual XYZ write.
 * Caller must validate rate denominators like the original settings path. */
void re7_original_state_rates(const Re7OriginalStateConfig *config,
    const Re7OriginalParams *params, float framerate, Re7OriginalStateRates *rates);
/* TRUE_FOV_READ 2524-2712 camera-state wrapper. Invoke at that hook's cadence:
 * returns 0 if frametrack equals the recorded value, changing nothing; else 1.
 * Applies bit-pattern >=32 repair, immediate FP reset, and run-pull updates,
 * then computes rates. FP reset deliberately leaves previous rates intact.
 * It does not modify game FOV. Initial record is zero, like the source. */
int re7_original_state_frame(const Re7OriginalStateConfig *config,
    const Re7OriginalStateInput *input, Re7OriginalState *state,
    Re7OriginalParams *params, Re7OriginalStateRates *rates);
/* AFTER_ADAPTIVE_DIFF+5, 7567-8647. Only the original branch-owned flags are
 * changed; clearing blockedgun does NOT cancel a running transition. Disabled
 * autodisable additionally applies player-hook clears at 1110-1116. */
void re7_original_state_policy(const Re7OriginalStateConfig *config,
    const Re7OriginalStateInput *input, Re7OriginalState *state);
/* CAM_WRITES Z, 5795-6231 + scalar focus at 6233-6295. Call after X/Y and pitch
 * multiplier update, before computing Z. Result chooses that Z's FP/TP math;
 * it is not always equal to internalfpflag or the last HEAD_Y publication.
 * Camera target identity/flycam skip belongs in the runtime adapter. */
unsigned re7_original_state_camera(const Re7OriginalStateConfig *config,
    const Re7OriginalStateInput *input, Re7OriginalState *state,
    Re7OriginalParams *params, Re7OriginalFocusState *focus,
    const Re7OriginalStateRates *rates);

/* Source producer contract for runtime adapters:
 * CUTSCENE_AUTOLOOK pointer resolution (1182-1210): player base is
 * module+0x8fb8018 -> 0xd8 -> 0x18 (all dereferenced), xyz at base+0x30/34/38.
 * Aim byte: module+0x8fbb610 -> 0x20 -> 0x88 -> 0x10 -> 0xa8, final +0x20.
 * Inventory byte: module+0x8fac380 -> 0x208 -> 0x58 -> 0x148 -> 0x30 -> 0x48,
 * final +0x10. Each arrow dereferences, final offset does not.
 * LADDER_CMP captures rax+0x53; byte cleared/re-read across loading (2248+).
 * FOV_AUTOOLDCAM_WRITE captures OLD [rax+0xb4] before its xmm0 store (2278+).
 * TIMESCALE_MULTIPLIER samples the player-position pointers (3247-3257).
 * loadarea: LOAD_AREA_READ (3747-3751).
 * held weapon: HELD_WEAPON_CMP above; crouch: CROUCH_FLAG_READ [rbx+0x144].
 * gun block: GUN_BLOCK_DIS_CALC / NAH counterpart, unsigned bits(xmm0)<=.1f;
 * frame timer clears aimblockedflag after >3 absent producer frames (2081+).
 * telescope: TELESCOPE_FLAG [rbx+0x60]!=0; guard: GUARD_FLOAT's birthday candle
 * location/heldweapon=1802/r9!=0/speed=.5 conjunction (7494-7518).
 * moldgrabflag includes TIMESCALE_MULTIPLIER extension + Mia exception (3602+).
 * itempickuppop: HEAD_Y_WRITE inspection tracker (1460+); lockbox/bodybag/
 * inexitdelay and model character bits are TIMESCALE_MULTIPLIER state (3200+).
 * nogunaccess: NO_GUN_ACCESS_FLOAT [rbx+0xac] bitwise != 1.f (4071+).
 * recentfpflag clears in TIMESCALE_MULTIPLIER only when firstpersonflag!=1,
 * old FOV bits exactly 75.f, and moldgrabflag!=1 (3239-3245).
 * These input producers must be live; an absent read is not evidence of 0.
 */
#endif
