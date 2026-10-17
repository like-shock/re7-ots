#ifndef RE7_OTS_ORIGINAL_COLLISION_H
#define RE7_OTS_ORIGINAL_COLLISION_H

#include "original_scalar.h"

/* Value-only port of RT 8.02 CT 2295-2360, 3779-3806 and 6443-7080.
 * Also includes EXAMINE_DOF_WRITE (4093-4126).
 * No scene raycast, FP proximity fallback, game pointers, or model/face writes.
 * Caller serializes state and resolves a complete live input snapshot.
 * Like original_scalar, requires binary32, x87 long double, round-to-nearest,
 * masked x87 exceptions, and no fast math. No external math library is needed.
 */
typedef struct {
    float camclipstartmult, camclipdegreeadjust, clipzoomintickrate;
    float clipzoomouttickrate, cczoomoutbuffer;
    float camclipbackminmult, camclipsideminmult;
} Re7OriginalCollisionConfig;

typedef struct {
    uint8_t firstpersonflag, tpcamclip, noclipautodisable, stopdofflag;
} Re7OriginalCollisionFlags;

typedef struct {
    float rawdofattempt, dofattempt, viewdofattempt, camtrigangle;
    float camclipangleadjust, oldwallping;
    uint32_t rdofdelayframes;
    uint8_t camiswest, camisnorth, bypassclipdeadzone, sample_valid;
} Re7OriginalCollisionState;

typedef struct {
    Re7OriginalCollisionFlags flags;
    float distance;
    uint32_t loadarea;
    float playerx, playerz;
    uint8_t available;
} Re7OriginalCollisionReadInput;

typedef struct {
    Re7OriginalCollisionFlags flags;
    Re7OriginalVec direction, camera_attempt;
    float yawx, yawz, framerate, autooldcamfov;
    uint8_t available;
} Re7OriginalCollisionInput;

enum {
    RE7_ORIGINAL_COLLISION_UNCHANGED = 0,
    RE7_ORIGINAL_COLLISION_APPLIED = 1,
    RE7_ORIGINAL_COLLISION_INVALID = 2
};
typedef struct {
    Re7OriginalVec direction;
    uint32_t status;
} Re7OriginalCollisionOutput;

void re7_original_collision_defaults(Re7OriginalCollisionConfig *config,
                                     Re7OriginalCollisionState *state);
/* DOF_DISTANCE_READ+3: original movss xmm0,[rcx+0x18] is always replayed
 * unmodified. Sample that float; this function updates only private state.
 * Returns APPLIED when captured, UNCHANGED for FP/delay/excluded samples,
 * INVALID for unavailable/nonfinite/negative distance. The native validation
 * marks sample_valid=0 until a subsequent usable distance/delay is supplied.
 */
int re7_original_collision_distance_read(Re7OriginalCollisionState *state,
                                         const Re7OriginalCollisionReadInput *in);
/* EXAMINE_DOF_WRITE: if rawdofattempt is exactly .25, capture low XMM7.
 * Always replay movss [RDI+0x580],xmm7 unchanged. Original helper preserves
 * RAX and every XMM7 lane; this API changes only private capture state.
 * Unlike distance_read, this path can capture in FP and has no exit delay.
 * Unavailable/nonfinite/negative capture marks sample_valid=0 after the gate.
 */
int re7_original_collision_examine_write(Re7OriginalCollisionState *state,
    const Re7OriginalCollisionFlags *flags, float distance, int available);
/* DOF_ORIGIN_ANGLE_WRITES names direction writes, NOT a world origin write.
 * Normal/EOZ at +4: X=ECX, Y=[RAX+4], Z=[RAX+8]; output stores to
 * [RDI+0x560/564/568], and final ECX=output.y, EAX=output.z (zero extends).
 * NAH: X/Y/Z=low XMM0/1/2; APPLIED also replaces low XMM0/1/2 and same stores.
 * Its active memory-loaded MOVSS clears each upper 96 bits; UNCHANGED/INVALID
 * only replay original stores and preserve all XMM lanes. Normal/EOZ leave
 * XMM registers untouched. Source X uses the previous
 * camtrigangle/camisnorth; Z uses this call's angle/west. Preserve call order.
 * Config is mutable because source entry clamps camclipsideminmult to 1.
 * Disabled paths reset only the three multipliers and preserve source state.
 * Native validation differs deliberately: an invalid complete snapshot leaves
 * the supplied direction unchanged, resets the three multipliers to 1, and
 * commits no partial angle/state changes. It never changes view mode.
 */
Re7OriginalCollisionOutput re7_original_collision_step(
    Re7OriginalParams *params, Re7OriginalCollisionConfig *config,
    Re7OriginalCollisionState *state, const Re7OriginalCollisionInput *in);
/* TIMESCALE_FRAMERATE: initial value is 60; caller supplies engine attempted
 * delta (XMM0) and truetimescalemult. Zero timescale retains previous rate.
 * Original rounds ratio to integral FPS then floors at 28. Invalid native
 * inputs retain previous if usable, else 60; no fabricated elapsed timing.
 */
float re7_original_collision_framerate(float previous, float attempted_delta,
                                      float truetimescalemult);
#endif
