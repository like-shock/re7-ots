#include "original_scalar.h"
#include <float.h>
#include <math.h>
#include <string.h>

#if LDBL_MANT_DIG != 64 || FLT_MANT_DIG != 24 || FLT_RADIX != 2
#error "Original scalar core needs x87 extended long double and IEEE binary32 float"
#endif
#ifdef __FAST_MATH__
#error "Original scalar core must not be compiled with fast math"
#endif
_Static_assert(sizeof(float) == sizeof(uint32_t), "binary32 representation required");

/* CT's fstp to a dd rounds here, rather than at each fmul/fadd. */
static float store32(long double x) { volatile float stored = (float)x; return stored; }
static uint32_t bits(float x) { uint32_t u; memcpy(&u, &x, sizeof(u)); return u; }

void re7_original_defaults(Re7OriginalParams *p) {
    /* CT 551-574, 657-658, 737-746, 778-781, 965-974. */
    *p = (Re7OriginalParams){
        .backdisadjust = 1.3f, .behinddis = 1, .aimbehinddis = 1,
        .camclipbehinddis = 1, .pitchmul = 0,
        .sidedisadjust = .3f, .sidedis = 1, .aimsidedis = 1, .camclipsidedis = 1,
        .heightdis = 1, .heightdisadjust = .9f, .camclipheightdis = 1
    };
}

void re7_original_view(const Re7OriginalViewInput *in, Re7OriginalViewState *s) {
    /* HEAD_Y_WRITE 1341-1365. Deliberately independent of 1367+ head hiding.
     * recentfpflag is sticky; a different original path owns clearing it. */
    s->firstpersonflag = in->stopflag == 1 || in->internalfpflag == 1 ||
        in->thirdpersonflag == 0 || (in->fpaimflag != 0 && in->aim == 1);
    if (s->firstpersonflag) s->recentfpflag = 1;
}

float re7_original_adjust_pitch(float attempted, float adjustment) {
    /* CAM_WRITES 5564-5567; target identity/lock checked by future hook. */
    return store32((long double)attempted - adjustment);
}

float re7_original_pitch_multiplier(float pitch) {
    /* 5807-5813: intermediate store BEFORE subtracting from 1. */
    float scaled = store32(fabsl((long double)pitch) * .6f);
    return store32(1.0L - scaled);
}

float re7_original_ots_axis(unsigned axis, float base, float yawx, float yawz,
                            float pitch, const Re7OriginalParams *p) {
    /* 5621-5640 / 5739-5747 / 6303-6322; also 1957-2026 for the
     * separately gated TP_INTERACTABLE_COORDS inspection transforms. */
    if (axis == RE7_ORIGINAL_Y) {
        float height = store32((long double)pitch * p->heightdis *
                                p->heightdisadjust * p->camclipheightdis);
        return store32((long double)base + height + p->yadd);
    }
    if (axis != RE7_ORIGINAL_X && axis != RE7_ORIGINAL_Z) return base;
    float behind = store32((long double)(axis == RE7_ORIGINAL_X ? yawx : yawz) *
        p->backdisadjust * p->behinddis * p->aimbehinddis * p->camclipbehinddis * p->pitchmul);
    float side = store32((long double)(axis == RE7_ORIGINAL_X ? yawz : yawx) *
        p->sidedisadjust * p->sidedis * p->aimsidedis * p->camclipsidedis);
    return store32((long double)base + (axis == RE7_ORIGINAL_X ? -behind : behind) + side);
}

float re7_original_fp_axis(unsigned axis, float base, float yawx, float yawz,
                           const Re7OriginalParams *p) {
    /* 5671-5684 / 5773-5775 / 6335-6348. Camera head OFFSET only;
     * no face/model visibility operation or dependency is involved. */
    if (axis == RE7_ORIGINAL_Y) return store32((long double)base + p->firstyadd);
    if (axis != RE7_ORIGINAL_X && axis != RE7_ORIGINAL_Z) return base;
    float head = store32((long double)(axis == RE7_ORIGINAL_X ? yawx : yawz) *
                         p->headadjust * p->pitchmul);
    float side = store32((long double)(axis == RE7_ORIGINAL_X ? yawz : yawx) * p->sideaimadjust);
    return store32((long double)base + (axis == RE7_ORIGINAL_X ? head : -head) + side);
}

void re7_original_focus_step(Re7OriginalParams *p, Re7OriginalFocusState *s,
                              const Re7OriginalFocusRates *r, unsigned action,
                              uint8_t guncheckflag, uint8_t holdinggun) {
    /* 6233-6295. CT uses unsigned integer comparisons of float bits, not
     * floating comparisons. Keep pre-step thresholds and resulting overshoot.
     * nozoomguncheck (6238) is a separate entry from zoomaimoldz (6233). */
    if (action == RE7_ORIGINAL_FOCUS_IN || action == RE7_ORIGINAL_FOCUS_IN_NO_GUN_CHECK) {
        if (action == RE7_ORIGINAL_FOCUS_IN && guncheckflag != 0 && holdinggun == 0) return;
        /* 6235 je @f targets 6247, skipping ALL flags and height adjustment. */
        if (action == RE7_ORIGINAL_FOCUS_IN_NO_GUN_CHECK || guncheckflag != 0) {
            s->unzoomflag = 1;
            s->unzoomsidedoneflag = 0;
            p->heightdisadjust = store32((long double)p->heightdisadjust - r->heighttick);
            if (bits(p->heightdisadjust) < bits(.3f)) p->heightdisadjust = .3f;
        }
        if (bits(p->aimbehinddis) >= bits(r->minaimbehinddis))
            p->aimbehinddis = store32((long double)p->aimbehinddis - r->aimtick);
        if (bits(p->aimsidedis) <= bits(r->maxaimsidedis))
            p->aimsidedis = store32((long double)p->aimsidedis + r->sideaimtick);
    } else if (action == RE7_ORIGINAL_FOCUS_OUT) {
        p->heightdisadjust = store32((long double)p->heightdisadjust + r->heighttick);
        if (bits(p->heightdisadjust) > bits(.9f)) p->heightdisadjust = .9f;
        if (bits(p->aimbehinddis) < bits(.99f))
            p->aimbehinddis = store32((long double)p->aimbehinddis + r->aimtick);
        else if (s->unzoomsidedoneflag == 1) s->unzoomflag = 0;
        else {
            /* 6283 jne @f targets 6293, bypassing the side-distance step. */
            s->unzoomsidedoneflag = 1;
            return;
        }
        if (bits(p->aimsidedis) > bits(.99f))
            p->aimsidedis = store32((long double)p->aimsidedis - r->sideaimtick);
        else s->unzoomsidedoneflag = 1;
    }
}

float re7_original_water_y(float y, uint32_t area, Re7OriginalVec player) {
    /* 5749-5765; retain unsigned ordering, especially for negative Y. */
    if (area == 4 && bits(player.x) >= bits(18.f) && bits(player.x) <= bits(36.5f) &&
        bits(player.y) >= bits(-8.f) && bits(player.y) <= bits(-8.8f) &&
        bits(player.z) >= bits(18.f) && bits(player.z) <= bits(26.f) &&
        bits(y) >= bits(-7.15f)) return -7.15f;
    return y;
}

Re7OriginalVec re7_original_firing_origin(Re7OriginalVec original,
                                          Re7OriginalVec camera, int available) {
    /* FIRING_COOR_WRITES 5513-5535. Defined native fault policy: restore the
     * full original vector if the future reader cannot supply a valid camera.
     * CT's exception path could leave a partial/old vector; do not copy that. */
    return available && isfinite(camera.x) && isfinite(camera.y) && isfinite(camera.z)
        ? camera : original;
}
