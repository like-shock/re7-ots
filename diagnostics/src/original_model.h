#ifndef RE7_OTS_ORIGINAL_MODEL_H
#define RE7_OTS_ORIGINAL_MODEL_H

#include <stddef.h>
#include <stdint.h>

/* Original RT 8.02 CT PLAYER_TRANS_READ / TRANS_GUN_WRITE, L1256-1306.
 * See diagnostics/NOTICE.md. No SDK/Lua dependency or retained game pointers.
 * culflag defaults to 1 in the CT (L607-608); it is not an FP/TP mode flag.
 *
 * Caller must prove the supported executable and exact original hook site,
 * then set validated_hook to 1 only for that current hook invocation. Owner
 * is [rbp+0x10] after the first original PLAYER_TRANS_READ instruction, or
 * rcx for TRANS_GUN_WRITE. These CT sites have no player identity comparison:
 * this adapter preserves their site-wide scope, not an Ethan-only guarantee.
 *
 * Caller owns original instruction emulation and all writes. On an empty
 * decision, run the original instruction(s) without alteration. On a read
 * decision, safely write the one byte before executing the original DWORD
 * read into EAX (which zero-extends RAX). On a gun decision, omit only the
 * original QWORD store; preserve all registers and flags. Current-readable
 * leaf checks are an added fail-closed guard, not proof of object lifetime.
 */
typedef int (*Re7OriginalModelRead)(void *ctx, uint64_t address,
                                   void *destination, size_t size);

typedef struct {
    uint64_t image_base, owner;
    uint8_t validated_hook, culflag;
} Re7OriginalModelInput;

typedef struct {
    uint64_t byte_write_address;
    uint8_t byte_write_value, suppress_original_write;
} Re7OriginalModelDecision;

void re7_original_model_player_read(Re7OriginalModelRead read, void *ctx,
                                    const Re7OriginalModelInput *input,
                                    Re7OriginalModelDecision *out);
void re7_original_model_gun_write(Re7OriginalModelRead read, void *ctx,
                                  const Re7OriginalModelInput *input,
                                  Re7OriginalModelDecision *out);

/* HEAD_Y_WRITE's full-body branch (L1641-1652) only hides the entire model
 * when hidemodel == 1. Its default is zero (L719-722). Thus showing the body
 * requires no fullmodel chain, model-Y rewrite, head/shadow offset, or mesh
 * scan here. The installed model pack still supplies the actual geometry.
 */
#endif
