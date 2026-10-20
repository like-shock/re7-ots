#include "original_model.h"

/* Windows x64 user-space bounds. The callback still controls readability. */
static int address(uint64_t base, uint64_t offset, size_t size, uint64_t *out) {
    const uint64_t maximum = UINT64_C(0x7FFFFFFFFFFF);
    if (base < 0x10000 || base > maximum || offset > maximum - base || !size)
        return 0;
    *out = base + offset;
    return size - 1 <= maximum - *out;
}

static int enabled(Re7OriginalModelRead read, void *ctx,
                   const Re7OriginalModelInput *input, size_t leaf_size,
                   uint64_t *leaf_address) {
    uint64_t interact_address;
    uint8_t interact[4], leaf[8];
    if (!read || !input || input->validated_hook != 1 || input->culflag == 0 ||
        !address(input->image_base, 0x8FB17F0, sizeof interact, &interact_address) ||
        !address(input->owner, 0x3D0, leaf_size, leaf_address))
        return 0;
    /* The original reads a DWORD here, NOT the full pointer-sized slot. */
    if (!read(ctx, interact_address, interact, sizeof interact) ||
        (interact[0] | interact[1] | interact[2] | interact[3]) != 0)
        return 0;
    return read(ctx, *leaf_address, leaf, leaf_size) != 0;
}

void re7_original_model_player_read(Re7OriginalModelRead read, void *ctx,
                                    const Re7OriginalModelInput *input,
                                    Re7OriginalModelDecision *out) {
    uint64_t leaf_address;
    if (!out) return;
    *out = (Re7OriginalModelDecision){0};
    if (enabled(read, ctx, input, 4, &leaf_address)) {
        out->byte_write_address = leaf_address + 1;
        out->byte_write_value = 6;
    }
}

void re7_original_model_gun_write(Re7OriginalModelRead read, void *ctx,
                                  const Re7OriginalModelInput *input,
                                  Re7OriginalModelDecision *out) {
    uint64_t leaf_address;
    if (!out) return;
    *out = (Re7OriginalModelDecision){0};
    if (enabled(read, ctx, input, 8, &leaf_address))
        out->suppress_original_write = 1;
}
