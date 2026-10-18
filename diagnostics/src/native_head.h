#ifndef RE7_OTS_NATIVE_HEAD_H
#define RE7_OTS_NATIVE_HEAD_H
#include <stddef.h>
#include <stdint.h>

typedef int (*NativeHeadRead)(void *, uint64_t, void *, size_t);
/* Caller-owned, serialized target cache. head is a transform base; shadow
 * and hair are the original Y addresses. No model/material writes occur. */
typedef struct {
    uint64_t head, shadow, hair;
    uint32_t characters;
} NativeHeadTargets;

enum { NATIVE_HEAD_NONE, NATIVE_HEAD_HEAD, NATIVE_HEAD_SHADOW, NATIVE_HEAD_HAIR };

void native_head_clear(NativeHeadTargets *targets);
/* Call only in first person at CUTSCENE_AUTOLOOK, after original_inputs has
 * validated the current head chain. Invalid inputs clear the entire cache;
 * a missing optional chain leaves the generic head usable. */
void native_head_refresh(NativeHeadTargets *targets, uint64_t image_base,
    uint64_t validated_head_base, uint32_t characters, NativeHeadRead read,
    void *ctx);
/* Exact cached address filtering precedes all reads. Every affirmative match
 * rereads the original head chain and exact 1.0 model marker, then the matching
 * optional chain. Pointer links are checked twice, but this is not a lifetime
 * guarantee and cannot detect ABA. FP/options/assumeheadless gating and the
 * original DWORD Y store belong to the caller. flashlight_on is a combined
 * boolean permission for the source's shadow branch (manual or observed light).
 * Original branch priority is shadow, four head transforms, then Mia hair. */
unsigned native_head_match(const NativeHeadTargets *targets, uint64_t image_base,
    uint64_t transform_base, uint32_t characters, int flashlight_on,
    NativeHeadRead read, void *ctx);
#endif
