#ifndef RE7_OTS_ORIGINAL_INTERACTION_H
#define RE7_OTS_ORIGINAL_INTERACTION_H

#include <stddef.h>
#include <stdint.h>

/* cheezeit / goldedtoad RE7 RT 8.02 CT, primarily lines 1459-1948.
 * See diagnostics/NOTICE.md. Pure caller-owned state; no installed hooks,
 * direct process-memory access, game writes, visibility edits, or coordinates.
 * The caller serializes shared state and passes the pre-instruction registers.
 * read returns nonzero only after copying ALL requested bytes. All addresses
 * are game virtual addresses; even intermediate pointers use this callback.
 *
 * Intentional fail-closed difference from CT {$try}/{$except}: failed/overflowed
 * reads and rejected paper/main pointers cannot match a residual register.
 * Cache/latch timing, cached-target refresh, sentinel 0x69, and unsigned
 * comparisons otherwise follow the source. No additional nonzero-secondary
 * restriction or invented target generation/reset rule is applied.
 */
typedef int (*Re7OriginalInteractionRead)(void *context, uint64_t address,
                                         void *output, size_t bytes);

typedef struct {
    uint8_t tpinteractables, firstpersonflag, fixpaperscan, fixwormbox;
    uint8_t fixfprompt, fixmain, interactableflag, scrapexamine, lockboxflag;
    uint32_t loadarea, frametrack;
    float cplayerz;
    uint64_t invflagadd, secintadd;
} Re7OriginalInteractionInput;

typedef struct {
    uint8_t paperscanpickuppop, readingnote, herbinwormbox, itempickuppop;
    uint32_t oldfpromptframe, activefprompt;
    uint64_t pvmainadd, ivmainadd;
} Re7OriginalInteractionState;

enum {
    RE7_ORIGINAL_INTERACTION_NONE,
    RE7_ORIGINAL_INTERACTION_PAPER,
    RE7_ORIGINAL_INTERACTION_SECONDARY,
    RE7_ORIGINAL_INTERACTION_FPROMPT
};

/* TP_INTERACTABLE_COORDS: rbx+0x30 is the coordinate destination; r13 is
 * compared verbatim with the secondary producer. Nonzero result authorizes
 * the original interaction XYZ formulas only for THIS invocation/target.
 * Readingnote is updated only on the matching fprompt path, as in the CT.
 * Defaults: tpinteractables,fixpaperscan,fixwormbox,fixfprompt,fixmain=1;
 * all state=0. firstpersonflag and interactableflag are producer-owned. */
int re7_original_interaction_select(const Re7OriginalInteractionInput *in,
    Re7OriginalInteractionState *state, uint64_t module_base, uint64_t rbx,
    uint64_t r13, Re7OriginalInteractionRead read, void *context);

/* HEAD_Y_WRITE item-view branch 1459-1519, independent of face/head hiding.
 * Updates ivmainadd for the prompt note guard, and returns whether rbx+0x34
 * matches the main item transform. Does not implement other HEAD_Y branches. */
int re7_original_interaction_track_main(const Re7OriginalInteractionInput *in,
    Re7OriginalInteractionState *state, uint64_t module_base, uint64_t rbx,
    Re7OriginalInteractionRead read, void *context);

void re7_original_interaction_prompt_seen(Re7OriginalInteractionState *state,
                                          uint8_t interactableflag);
void re7_original_interaction_loadscreen(Re7OriginalInteractionState *state,
                                         uint8_t loadscreenflag);
uint32_t re7_original_interaction_timer(uint32_t frametrack);
int re7_original_interaction_secondary(uint64_t rax,
    Re7OriginalInteractionRead read, void *context, uint64_t *address);
int re7_original_interaction_inventory(uint64_t module_base,
    Re7OriginalInteractionRead read, void *context, uint64_t *address);
uint8_t re7_original_interaction_scrap(uint64_t rbx,
    Re7OriginalInteractionRead read, void *context);
uint8_t re7_original_interaction_lockbox(uint32_t loadarea,
                                        float player_x, float player_y, float player_z);

/* Runtime producer contract, bytes from diagnostics/data/patch_manifest.json.
 * Install/restore the COMPLETE expected spans, not the CT jump+nop counts.
 * Offsets are relative to each manifest AOB result. Preserve flags/registers
 * and replay the original instructions after capturing these values.
 *
 * INTERACTABLE_FLAG_ON +1, 7: C6 82 88 00 00 00 01
 *   flag=1; original writes [rdx+0x88]=1.
 * INTERACTABLE_FLAG_OFF +0, 7: C6 80 88 00 00 00 00
 *   flag=0; original writes [rax+0x88]=0; does NOT clear pickup latches.
 * GET_SECONDARY_INTERACTABLE_R13_ADDRESS +0, 8:
 *   48 8B 48 18 48 8B 43 50; secintadd=[rax+0x18] BEFORE rax changes.
 * FPROMPT_CMP +0, 7: 80 BA 90 00 00 00 00
 *   prompt_seen(flag); independent of [rdx+0x90]'s value, preserve original CMP.
 * TIMER_AOB +0, 8: F3 0F 10 83 D4 02 00 00
 *   frametrack=timer(frametrack), once per original invocation (not wall time).
 * HEAD_Y_WRITE +1, 21:
 *   F3 0F 11 43 34 F3 0F 11 4B 38 F3 0F 11 53 3C F3 44 0F 11 4B 30
 *   track_main(...) using rbx; the face/hair-hiding branches are unnecessary.
 * CUTSCENE_AUTOLOOK +1, 8: C6 42 1C 01 48 8B 41 50
 *   inventory(...) resolves invflagadd, source 1202-1210; no register input.
 * INVENTORY_HIGHLIGHTED_ITEM_ID +0, 7: 48 8B 53 70 48 85 D2
 *   scrap(rbx): [rbx+0x78] UTF-16 words +0x14='N', +0x26='0', +0x28='9'.
 * LOADED_AREA +0, 6: 8B 48 54 83 F9 03
 *   loadarea=uint32 [rax+0x54], before original MOV ECX/CMP.
 * TIMESCALE_MULTIPLIER +0, 8: F3 0F 59 83 48 03 00 00
 *   lockbox(loadarea,cplayerx,cplayery,cplayerz), source 3281-3420;
 *   loadscreen(state,loadscreenflag), source 3593-3597. The player snapshot
 *   and loadscreenflag come from the existing original camera/timer producers.
 * TP_INTERACTABLE_COORDS -5, 25:
 *   F3 0F 11 43 30 F3 0F 10 45 30 F3 0F 11 43 3C F3 0F 11 4B 34 F3 0F 11 53 38
 *   select(...rbx,r13); XYZ bases are pre-original xmm0/1/2; W=[rbp+0x30]
 *   must retain its original store to [rbx+0x3C]. Use original_scalar XYZ.
 */
#endif
