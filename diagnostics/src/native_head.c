#include "native_head.h"
#include "original_state.h"

/* RT 8.02 user-space chains, supplied by the caller's guarded reader. */
static int span(uint64_t base, uint64_t offset, size_t size, uint64_t *out) {
    const uint64_t maximum = UINT64_C(0x7FFFFFFFFFFF);
    if (base < 0x10000 || base > maximum || offset > maximum - base || !size)
        return 0;
    *out = base + offset;
    return size - 1 <= maximum - *out;
}
static int known_character(uint32_t characters) {
    const uint32_t known = (1u << RE7_ORIGINAL_CHAR_COUNT) - 1;
    return characters && !(characters & (characters - 1)) && !(characters & ~known);
}
static unsigned marker_offset(uint32_t characters) {
    const uint32_t clancy = (1u << RE7_ORIGINAL_CHAR_MAIN_CLANCY) |
                            (1u << RE7_ORIGINAL_CHAR_DLC_CLANCY);
    return characters & clancy ? 0x4BC : 0x53C;
}
static int get(NativeHeadRead read, void *ctx, uint64_t base, unsigned offset,
               void *out, size_t size) {
    uint64_t address;
    return read && span(base, offset, size, &address) && read(ctx, address, out, size);
}
static int inputs(uint64_t image_base, uint64_t head_base, uint32_t characters,
                  NativeHeadRead read) {
    uint64_t address;
    return read && known_character(characters) &&
           span(image_base, 0x8FBB610, 8, &address) &&
           span(head_base, marker_offset(characters), 4, &address);
}
static int chain(uint64_t image_base, const unsigned offsets[6], unsigned leaf,
                 NativeHeadRead read, void *ctx, uint64_t *result) {
    uint64_t locations[7], links[7], current = image_base, next;
    for (unsigned i = 0; i < 7; i++) {
        unsigned offset = i ? offsets[i - 1] : 0x8FBB610;
        if (!span(current, offset, 8, &locations[i]) ||
            !get(read, ctx, current, offset, &next, 8) ||
            !span(next, 0, 1, &current)) return 0;
        links[i] = current;
    }
    /* Check every link again, including the leaf owner, to reject changes
     * during traversal. This cannot prove object lifetime or prevent ABA. */
    for (unsigned i = 7; i > 0; i--) {
        if (!get(read, ctx, locations[i - 1], 0, &next, 8) ||
            next != links[i - 1]) return 0;
    }
    return span(current, leaf, 4, result);
}
static int current_head(const NativeHeadTargets *targets, uint64_t image_base,
                        NativeHeadRead read, void *ctx) {
    static const unsigned offsets[] = {0xA0, 0xE0, 0x78, 0x10, 0x18, 0xC0};
    uint64_t head_base; uint32_t marker;
    return chain(image_base, offsets, 0, read, ctx, &head_base) &&
           head_base == targets->head &&
           get(read, ctx, head_base, marker_offset(targets->characters), &marker, 4) &&
           marker == 0x3F800000u;
}
static int shadow_target(uint64_t image_base, NativeHeadRead read, void *ctx,
                         uint64_t *result) {
    static const unsigned primary[] = {0xA0, 0x68, 0x88, 0x10, 0xC0, 0xC0};
    static const unsigned fallback[] = {0x10, 0x68, 0x28, 0x18, 0x88, 0xC0};
    return chain(image_base, primary, 0x34, read, ctx, result) ||
           chain(image_base, fallback, 0x34, read, ctx, result);
}
static int hair_target(uint64_t image_base, NativeHeadRead read, void *ctx,
                       uint64_t *result) {
    static const unsigned offsets[] = {0xA0, 0xE0, 0x80, 0x10, 0x18, 0xC0};
    return chain(image_base, offsets, 0x1B4, read, ctx, result);
}

void native_head_clear(NativeHeadTargets *targets) {
    if (targets) *targets = (NativeHeadTargets){0};
}
void native_head_refresh(NativeHeadTargets *targets, uint64_t image_base,
    uint64_t validated_head_base, uint32_t characters, NativeHeadRead read,
    void *ctx) {
    uint64_t target;
    if (!targets) return;
    native_head_clear(targets);
    if (!inputs(image_base, validated_head_base, characters, read)) return;
    targets->head = validated_head_base;
    targets->characters = characters;
    if (shadow_target(image_base, read, ctx, &target)) targets->shadow = target;
    if (characters == (1u << RE7_ORIGINAL_CHAR_PRESENT_MIA) &&
        hair_target(image_base, read, ctx, &target)) targets->hair = target;
}
unsigned native_head_match(const NativeHeadTargets *targets, uint64_t image_base,
    uint64_t transform_base, uint32_t characters, int flashlight_on,
    NativeHeadRead read, void *ctx) {
    uint64_t target_y, current;
    unsigned candidate = NATIVE_HEAD_NONE;
    if (!targets || characters != targets->characters ||
        !inputs(image_base, targets->head, characters, read) ||
        !span(transform_base, 0x34, 4, &target_y)) return NATIVE_HEAD_NONE;
    if (flashlight_on && target_y == targets->shadow) candidate = NATIVE_HEAD_SHADOW;
    else if (target_y == targets->head + 0x34 || target_y == targets->head + 0x74 ||
             target_y == targets->head + 0xB4 || target_y == targets->head + 0x2B4)
        candidate = NATIVE_HEAD_HEAD;
    else if (characters == (1u << RE7_ORIGINAL_CHAR_PRESENT_MIA) &&
             target_y == targets->hair) candidate = NATIVE_HEAD_HAIR;
    if (!candidate || !current_head(targets, image_base, read, ctx))
        return NATIVE_HEAD_NONE;
    if (candidate == NATIVE_HEAD_SHADOW &&
        (!shadow_target(image_base, read, ctx, &current) || current != targets->shadow))
        return NATIVE_HEAD_NONE;
    if (candidate == NATIVE_HEAD_HAIR &&
        (!hair_target(image_base, read, ctx, &current) || current != targets->hair))
        return NATIVE_HEAD_NONE;
    return candidate;
}
