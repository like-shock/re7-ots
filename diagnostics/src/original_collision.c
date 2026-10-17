#include "original_collision.h"
#include <float.h>
#include <math.h>
#include <string.h>

#if LDBL_MANT_DIG != 64 || FLT_MANT_DIG != 24 || FLT_RADIX != 2
#error "Original collision core needs x87 extended long double and binary32"
#endif
#ifdef __FAST_MATH__
#error "Original collision core must not be compiled with fast math"
#endif

static float store32(long double x) { volatile float result = (float)x; return result; }
static uint32_t bits(float x) { uint32_t u; memcpy(&u, &x, sizeof(u)); return u; }
static long double x87_sqrt(long double x) {
    __asm__("fsqrt" : "+t"(x));
    return x;
}
static long double x87_atan(long double opposite, long double adjacent) {
    long double result;
    __asm__("fpatan" : "=t"(result) : "0"(adjacent), "u"(opposite) : "st(1)");
    return result;
}
static long double x87_pi(void) {
    long double result;
    __asm__("fldpi" : "=t"(result));
    return result;
}
static int32_t x87_truncate(long double x) {
    int32_t result;
    __asm__("fisttpl %0" : "=m"(result) : "t"(x) : "st");
    return result;
}
static int32_t x87_round(long double x) {
    int32_t result;
    __asm__("fistpl %0" : "=m"(result) : "t"(x) : "st");
    return result;
}
static int finite_vec(Re7OriginalVec v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
static void reset_multipliers(Re7OriginalParams *p) {
    p->camclipbehinddis = p->camclipsidedis = p->camclipheightdis = 1;
}
static int disabled(Re7OriginalCollisionFlags f) {
    return f.firstpersonflag == 1 || f.tpcamclip != 1 ||
           f.noclipautodisable == 1 || f.stopdofflag == 0;
}

void re7_original_collision_defaults(Re7OriginalCollisionConfig *c,
                                     Re7OriginalCollisionState *s) {
    /* CT 794-807. Other original allocations begin zero initialized. */
    *c = (Re7OriginalCollisionConfig){1.5f, 10, 0, 1, .6f, .3f, .7f};
    *s = (Re7OriginalCollisionState){.sample_valid = 1};
}

int re7_original_collision_distance_read(Re7OriginalCollisionState *s,
                                         const Re7OriginalCollisionReadInput *in) {
    /* CT 2307-2352: FP and delay paths do not fetch even raw distance. */
    if (in->flags.firstpersonflag == 1) {
        s->rdofdelayframes = 3;
        return RE7_ORIGINAL_COLLISION_UNCHANGED;
    }
    if (s->rdofdelayframes != 0) {
        --s->rdofdelayframes;
        s->dofattempt = 50;
        s->sample_valid = 1;
        return RE7_ORIGINAL_COLLISION_UNCHANGED;
    }
    /* Intentional native fault policy: no stale invalid sample is consumed. */
    if (!in->available || !isfinite(in->distance) || in->distance < 0 ||
        (in->loadarea == 37 && (!isfinite(in->playerx) || !isfinite(in->playerz)))) {
        s->sample_valid = 0;
        return RE7_ORIGINAL_COLLISION_INVALID;
    }
    s->rawdofattempt = in->distance;
    if (bits(in->distance) == bits(.25f) ||
        (in->loadarea == 37 && bits(in->playerx) >= bits(8.f) &&
         bits(in->playerx) <= bits(9.f) && bits(in->playerz) >= bits(115.f) &&
         bits(in->playerz) <= bits(117.f) && bits(in->distance) == bits(.2f)))
        return RE7_ORIGINAL_COLLISION_UNCHANGED;
    s->dofattempt = s->viewdofattempt = in->distance;
    s->sample_valid = 1;
    /* Read hook compares tpcamclip==0; angle hook requires tpcamclip==1. */
    if (in->flags.firstpersonflag == 1 || in->flags.tpcamclip == 0 ||
        in->flags.noclipautodisable == 1 || in->flags.stopdofflag == 0)
        s->viewdofattempt = 0;
    return RE7_ORIGINAL_COLLISION_APPLIED;
}

int re7_original_collision_examine_write(Re7OriginalCollisionState *s,
    const Re7OriginalCollisionFlags *flags, float distance, int available) {
    /* CT 4098-4116. The caller always replays the unchanged XMM7 store. */
    if (bits(s->rawdofattempt) != bits(.25f)) return RE7_ORIGINAL_COLLISION_UNCHANGED;
    if (!available || !isfinite(distance) || distance < 0) {
        s->sample_valid = 0;
        return RE7_ORIGINAL_COLLISION_INVALID;
    }
    s->dofattempt = s->viewdofattempt = distance;
    s->sample_valid = 1;
    if (flags->firstpersonflag == 1 || flags->tpcamclip == 0 ||
        flags->noclipautodisable == 1 || flags->stopdofflag == 0)
        s->viewdofattempt = 0;
    return RE7_ORIGINAL_COLLISION_APPLIED;
}

float re7_original_collision_framerate(float previous, float delta, float timescale) {
    /* CT 3783-3797; fistp obeys the round-to-nearest x87 control word. */
    if (!isfinite(previous) || previous <= 0) previous = 60;
    if (bits(timescale) == 0) return previous;
    if (!isfinite(delta) || delta <= 0 || !isfinite(timescale) || timescale <= 0)
        return previous;
    long double ratio = (1.0L / delta) * timescale;
    if (ratio > INT32_MAX) return previous; /* intentional native overflow guard */
    float fps = store32(x87_round(ratio));
    return bits(fps) >= bits(28.f) ? fps : 28;
}

static int valid(const Re7OriginalParams *p, const Re7OriginalCollisionConfig *c,
                 const Re7OriginalCollisionState *s, const Re7OriginalCollisionInput *in) {
    return in->available && s->sample_valid && finite_vec(in->direction) &&
        finite_vec(in->camera_attempt) && isfinite(in->yawx) && isfinite(in->yawz) &&
        isfinite(in->framerate) && in->framerate > 0 && isfinite(in->autooldcamfov) &&
        isfinite(p->backdisadjust) && isfinite(p->behinddis) && p->behinddis > 0 &&
        isfinite(p->pitchmul) && isfinite(p->sidedisadjust) && isfinite(p->sidedis) &&
        isfinite(c->camclipstartmult) && c->camclipstartmult > 0 &&
        isfinite(c->camclipdegreeadjust) && isfinite(c->clipzoomintickrate) &&
        c->clipzoomintickrate >= 0 && isfinite(c->clipzoomouttickrate) &&
        c->clipzoomouttickrate >= 0 && isfinite(c->cczoomoutbuffer) &&
        c->cczoomoutbuffer >= 0 && isfinite(c->camclipbackminmult) &&
        isfinite(c->camclipsideminmult) && isfinite(s->camtrigangle) &&
        isfinite(s->camclipangleadjust) && isfinite(s->oldwallping) &&
        isfinite(s->dofattempt) && s->dofattempt >= 0;
}

/* CT 6556-6578 / 6963-6985: one reflection, not trig normalization/clamping. */
static float reflect_once(float v) {
    if (bits(v) < bits(1.f)) return v;
    if (bits(v) < UINT32_C(0x80000000)) {
        float remainder = store32((long double)v - 1.f);
        return store32(1.0L - remainder);
    }
    if (bits(v) < bits(-1.f)) return v;
    float remainder = store32((long double)v - -1.f);
    return store32(-1.0L - remainder);
}

static float camera_angle(const Re7OriginalParams *p, const Re7OriginalCollisionInput *in,
                          float side) {
    /* CT 6626-6730. Keep every source binary32 temporary store. */
    float behindx = store32((long double)in->yawx * p->backdisadjust * p->behinddis * p->pitchmul);
    float sidex = store32((long double)in->yawz * p->sidedisadjust * side);
    float x = store32((long double)in->camera_attempt.x - behindx + sidex);
    float behindz = store32((long double)in->yawz * p->backdisadjust * p->behinddis * p->pitchmul);
    float sidez = store32((long double)in->yawx * p->sidedisadjust * side);
    float z = store32((long double)in->camera_attempt.z + behindz + sidez);
    float dx = store32((long double)in->camera_attempt.x - x);
    float dx2 = store32((long double)dx * dx);
    float dz = store32((long double)in->camera_attempt.z - z);
    float dz2 = store32((long double)dz * dz);
    float hyp = store32(x87_sqrt((long double)dx2 + dz2));
    dx = store32((long double)x - sidex - in->camera_attempt.x);
    dx2 = store32((long double)dx * dx);
    dz = store32((long double)z - sidez - in->camera_attempt.z);
    dz2 = store32((long double)dz * dz);
    float adjacent = store32(x87_sqrt((long double)dx2 + dz2));
    float adjacent2 = store32((long double)adjacent * adjacent);
    float opposite = store32(x87_sqrt((long double)hyp * hyp - adjacent2));
    float radians = store32(x87_atan(opposite, adjacent));
    float degrees = store32((180.0L / x87_pi()) * radians);
    if (bits(side) >= UINT32_C(0x80000000)) degrees = store32(-(long double)degrees);
    return store32((long double)degrees / 90.f);
}

static void clamp_return(Re7OriginalParams *p) {
    /* CT 6885-6895 intentionally rounds >=.99 straight to 1. */
    if (bits(p->camclipbehinddis) >= bits(.99f)) p->camclipbehinddis = 1;
    if (bits(p->camclipsidedis) >= bits(.99f)) p->camclipsidedis = 1;
    if (bits(p->camclipheightdis) >= bits(.99f)) p->camclipheightdis = 1;
}

static void update_multipliers(Re7OriginalParams *p, const Re7OriginalCollisionConfig *c,
                               Re7OriginalCollisionState *s, float fps, float max,
                               float side, float fov) {
    /* CT 6731-6927: comparisons are unsigned binary32 bit comparisons. */
    float out = store32((1.0L / fps) / c->clipzoomouttickrate);
    float in = store32((1.0L / fps) / c->clipzoomintickrate);
    if (bits(s->dofattempt) >= bits(max)) {
        s->bypassclipdeadzone = 0;
        if (bits(c->clipzoomouttickrate) == 0) reset_multipliers(p);
        else {
            p->camclipbehinddis = store32((long double)p->camclipbehinddis + out);
            p->camclipsidedis = store32((long double)p->camclipsidedis + out);
            p->camclipheightdis = store32((long double)p->camclipheightdis + out);
            clamp_return(p);
        }
        goto minima;
    }
    if (s->bypassclipdeadzone != 1 &&
        bits(store32(fabsl((long double)s->dofattempt - s->oldwallping))) < bits(.001f))
        return;
    float inverse = store32(1.0L / max);
    float goal = store32((long double)s->dofattempt * inverse);
    if (bits(p->camclipbehinddis) > bits(goal)) {
        if (bits(c->clipzoomintickrate) == 0 ||
            bits(store32((long double)p->camclipbehinddis - in)) <= bits(goal)) goto instant;
        p->camclipbehinddis = store32((long double)p->camclipbehinddis - in);
        p->camclipheightdis = store32((long double)p->camclipheightdis - in);
        p->camclipsidedis = store32((long double)p->camclipsidedis - in);
        if (bits(p->camclipbehinddis) > UINT32_C(0x80000000) ||
            bits(p->camclipsidedis) > UINT32_C(0x80000000) ||
            bits(p->camclipheightdis) > UINT32_C(0x80000000)) goto instant;
        s->bypassclipdeadzone = 1;
        goto minima;
    }
    if (bits(c->cczoomoutbuffer) != 0 && bits(c->clipzoomouttickrate) != 0 &&
        bits(fov) != bits(25.f)) {
        float range = store32(1.0L - c->camclipbackminmult);
        float bracket = store32((long double)c->camclipstartmult * range * c->cczoomoutbuffer);
        float deadzone = store32((long double)c->camclipbackminmult * c->camclipstartmult);
        float remaining = store32((long double)s->dofattempt - deadzone);
        int32_t bracket_index = x87_truncate((long double)remaining / bracket);
        float floor = store32((long double)bracket * bracket_index + deadzone);
        float next = store32(((long double)p->camclipbehinddis + out) * c->camclipstartmult);
        if (bits(next) >= bits(floor)) {
            clamp_return(p);
            goto minima;
        }
    }
    if (bits(c->clipzoomouttickrate) == 0 ||
        bits(store32((long double)p->camclipbehinddis + out)) >= bits(goal)) goto instant;
    p->camclipbehinddis = store32((long double)p->camclipbehinddis + out);
    p->camclipheightdis = store32((long double)p->camclipheightdis + out);
    p->camclipsidedis = store32((long double)p->camclipsidedis + out);
    s->bypassclipdeadzone = 1;
    clamp_return(p);
    goto minima;
instant:
    p->camclipbehinddis = p->camclipheightdis = p->camclipsidedis = goal;
    s->bypassclipdeadzone = 0;
minima:;
    float minback = store32(fabsl((long double)c->camclipbackminmult) / p->behinddis);
    if (bits(p->camclipbehinddis) <= bits(minback)) p->camclipbehinddis = minback;
    if (bits(p->camclipheightdis) <= bits(minback)) p->camclipheightdis = minback;
    float absside = store32(fabsl((long double)side));
    float minside = store32(fabsl((long double)c->camclipsideminmult) / absside);
    if (bits(p->camclipsidedis) <= bits(minside)) p->camclipsidedis = minside;
    s->oldwallping = s->dofattempt;
}

Re7OriginalCollisionOutput re7_original_collision_step(
    Re7OriginalParams *params, Re7OriginalCollisionConfig *config,
    Re7OriginalCollisionState *state, const Re7OriginalCollisionInput *in) {
    Re7OriginalCollisionOutput out = {in->direction, RE7_ORIGINAL_COLLISION_UNCHANGED};
    Re7OriginalParams p = *params;
    Re7OriginalCollisionConfig c = *config;
    Re7OriginalCollisionState s = *state;
    /* CT 6488-6502 performs unsigned clamps before disabled flag checks. */
    if (bits(p.camclipbehinddis) > bits(1.f)) p.camclipbehinddis = 1;
    if (bits(p.camclipsidedis) > bits(1.f)) p.camclipsidedis = 1;
    if (bits(p.camclipheightdis) > bits(1.f)) p.camclipheightdis = 1;
    if (bits(c.camclipsideminmult) > bits(1.f)) c.camclipsideminmult = 1;
    if (disabled(in->flags)) {
        reset_multipliers(params);
        *config = c;
        return out;
    }
    if (!valid(&p, &c, &s, in)) goto invalid;
    float adjustment = bits(p.sidedis) > UINT32_C(0x80000000)
        ? store32(-(long double)s.camclipangleadjust) : s.camclipangleadjust;
    float max = store32((long double)c.camclipstartmult * p.behinddis);
    if (!isfinite(max) || max <= 0) goto invalid;
    s.camclipangleadjust = store32((long double)c.camclipdegreeadjust / 90.f);
    s.camiswest = bits(in->direction.x) <= UINT32_C(0x80000000);
    float x = store32(-(long double)in->direction.x +
                       (s.camisnorth == 1 ? -(long double)s.camtrigangle : s.camtrigangle));
    x = reflect_once(x);
    x = store32((long double)x + (s.camisnorth == 1 ? -(long double)adjustment : adjustment));
    float y = store32(-(long double)in->direction.y);
    float side = bits(p.sidedis) == 0 ? .01f : p.sidedis;
    s.camtrigangle = camera_angle(&p, in, side);
    update_multipliers(&p, &c, &s, in->framerate, max, side, in->autooldcamfov);
    s.camisnorth = bits(in->direction.z) <= UINT32_C(0x80000000);
    float z = store32(-(long double)in->direction.z +
                      (s.camiswest == 1 ? (long double)s.camtrigangle : -(long double)s.camtrigangle));
    z = reflect_once(z);
    z = store32((long double)z + (s.camiswest == 1 ? (long double)adjustment : -(long double)adjustment));
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(p.camclipbehinddis) ||
        !isfinite(p.camclipsidedis) || !isfinite(p.camclipheightdis)) goto invalid;
    *params = p;
    *config = c;
    *state = s;
    out.direction = (Re7OriginalVec){x, y, z};
    out.status = RE7_ORIGINAL_COLLISION_APPLIED;
    return out;
invalid:
    reset_multipliers(params);
    out.status = RE7_ORIGINAL_COLLISION_INVALID;
    return out;
}
