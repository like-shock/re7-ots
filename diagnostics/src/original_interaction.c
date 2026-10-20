#include "original_interaction.h"
#include <string.h>

#define INTERACTABLE_RVA UINT64_C(0x08FB17F0)
#define FPROMPT_RVA UINT64_C(0x08FB2BA0)
#define POPUP_RVA UINT64_C(0x08FB16E0)
#define INVENTORY_RVA UINT64_C(0x08FAC380)
#define BAD_TARGET UINT64_C(0x69)
#define MIN_MAIN_TARGET UINT64_C(0x1000000000)

static int add(uint64_t base, uint64_t offset, uint64_t *address) {
    if (base > UINT64_MAX - offset) return 0;
    *address = base + offset;
    return 1;
}

static int read_at(Re7OriginalInteractionRead read, void *context,
                    uint64_t base, uint64_t offset, void *output, size_t count) {
    uint64_t address;
    return read && add(base, offset, &address) &&
        address <= UINT64_MAX - (count - 1) && read(context, address, output, count);
}

/* Offsets dereference every stage; final_offset is LEA, not another read. */
static int chain(uint64_t base, uint64_t root, const uint16_t *offsets,
                 size_t count, uint16_t final_offset,
                 Re7OriginalInteractionRead read, void *context, uint64_t *out) {
    uint64_t current;
    *out = 0;
    if (!read_at(read, context, base, root, &current, sizeof(current))) return 0;
    for (size_t i = 0; i < count; ++i) {
        uint64_t next;
        if (!read_at(read, context, current, offsets[i], &next, sizeof(next))) return 0;
        current = next;
    }
    return add(current, final_offset, out);
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static int latch(const Re7OriginalInteractionInput *in, uint8_t *pickup,
                  uint64_t module, Re7OriginalInteractionRead read, void *context) {
    uint32_t static_low;
    if (in->tpinteractables != 1 || in->firstpersonflag == 1) return 0;
    if (!read_at(read, context, module, INTERACTABLE_RVA, &static_low, sizeof(static_low)))
        return 0;
    if (static_low == 0) {
        *pickup = 0;
        return 0;
    }
    if (*pickup != 1) {
        if (in->interactableflag != 1) return 0;
        *pickup = 1;
    }
    return 1;
}

static int main_target(uint64_t module, uint64_t candidate, uint64_t *cached,
                       const uint16_t *offsets, size_t count, uint16_t final_offset,
                       Re7OriginalInteractionRead read, void *context) {
    uint64_t resolved;
    if (*cached == BAD_TARGET || (*cached != 0 && *cached != candidate)) return 0;
    if (!chain(module, INTERACTABLE_RVA, offsets, count, final_offset,
               read, context, &resolved) || resolved < MIN_MAIN_TARGET) {
        if (*cached == 0) *cached = BAD_TARGET;
        return 0;
    }
    *cached = resolved;
    return resolved == candidate;
}

int re7_original_interaction_track_main(const Re7OriginalInteractionInput *in,
    Re7OriginalInteractionState *s, uint64_t module, uint64_t rbx,
    Re7OriginalInteractionRead read, void *context) {
    static const uint16_t offsets[] = {0x348, 0x10, 0xC0};
    uint64_t candidate;
    if (s->itempickuppop != 1) s->ivmainadd = 0;
    if (!latch(in, &s->itempickuppop, module, read, context) || in->fixmain != 1 ||
        !add(rbx, 0x34, &candidate)) return 0;
    return main_target(module, candidate, &s->ivmainadd, offsets, 3, 0x74, read, context);
}

static int popup(uint64_t module, Re7OriginalInteractionRead read, void *context,
                  uint16_t *value) {
    static const uint16_t offsets[] = {0x110, 0x70};
    uint64_t target;
    return chain(module, POPUP_RVA, offsets, 2, 0xA0E, read, context, &target) &&
        read_at(read, context, target, 0, value, sizeof(*value));
}

int re7_original_interaction_select(const Re7OriginalInteractionInput *in,
    Re7OriginalInteractionState *s, uint64_t module, uint64_t rbx, uint64_t r13,
    Re7OriginalInteractionRead read, void *context) {
    static const uint16_t paper_offsets[] = {0x98, 0x18};
    static const uint16_t prompt_offsets[] = {0x18, 0x60, 0x68, 0x18, 0xC0, 0x238, 0x18};
    uint64_t candidate, prompt_target;
    uint16_t popup_status;
    int paper_allowed = in->fixpaperscan == 1;

    /* Original reset occurs before the feature/first-person checks. */
    if (s->paperscanpickuppop != 1) s->pvmainadd = 0;
    if (!latch(in, &s->paperscanpickuppop, module, read, context) ||
        !add(rbx, 0x30, &candidate)) return RE7_ORIGINAL_INTERACTION_NONE;

    if (paper_allowed && in->scrapexamine == 1 && in->loadarea == 37) {
        uint8_t inventory_flag;
        /* Unreadable inventory state cannot authorize the paper branch. */
        if (!read_at(read, context, in->invflagadd, 0, &inventory_flag, 1) ||
            inventory_flag == 1) paper_allowed = 0;
    }
    if (paper_allowed && main_target(module, candidate, &s->pvmainadd,
            paper_offsets, 2, 0xB0, read, context)) return RE7_ORIGINAL_INTERACTION_PAPER;

    if (in->fixwormbox == 1 && (in->lockboxflag == 1 || in->lockboxflag == 5) &&
        r13 == in->secintadd) return RE7_ORIGINAL_INTERACTION_SECONDARY;

    /* This frame aging is skipped by successful paper/secondary paths. */
    if (s->oldfpromptframe != in->frametrack) {
        s->oldfpromptframe = in->frametrack;
        if (s->activefprompt != 0) --s->activefprompt;
    }
    if (in->fixfprompt != 1 || s->activefprompt == 0 ||
        (in->loadarea == 29 && bits(in->cplayerz) >= bits(7.7f)))
        return RE7_ORIGINAL_INTERACTION_NONE;

    if (!chain(module, FPROMPT_RVA, prompt_offsets, 7, 0xB0, read, context, &prompt_target) ||
        prompt_target != candidate) return RE7_ORIGINAL_INTERACTION_NONE;

    /* CT1892 lacks qword: CE's immediate-memory CMP defaults to DWORD. */
    s->readingnote = (uint32_t)s->ivmainadd < UINT32_C(0x100);
    if (s->readingnote == 1 || in->lockboxflag == 1) return RE7_ORIGINAL_INTERACTION_NONE;
    if (in->lockboxflag == 2) {
        if (!popup(module, read, context, &popup_status) || popup_status == 0)
            return RE7_ORIGINAL_INTERACTION_NONE;
    } else if (in->lockboxflag == 5) {
        if (s->herbinwormbox == 0) {
            if (!popup(module, read, context, &popup_status)) return RE7_ORIGINAL_INTERACTION_NONE;
            if (popup_status == 0) s->herbinwormbox = 1;
        }
        if (s->herbinwormbox != 1 &&
            (!popup(module, read, context, &popup_status) || popup_status != 0))
            return RE7_ORIGINAL_INTERACTION_NONE;
    }
    return RE7_ORIGINAL_INTERACTION_FPROMPT;
}

void re7_original_interaction_prompt_seen(Re7OriginalInteractionState *s, uint8_t flag) {
    if (flag == 1) s->activefprompt = 3;
}

void re7_original_interaction_loadscreen(Re7OriginalInteractionState *s, uint8_t flag) {
    if (flag == 1) s->herbinwormbox = 0;
}

uint32_t re7_original_interaction_timer(uint32_t frame) {
    return (frame > 1000 ? 0 : frame) + 1;
}

int re7_original_interaction_secondary(uint64_t rax, Re7OriginalInteractionRead read,
    void *context, uint64_t *address) {
    uint64_t result;
    *address = 0;
    if (!read_at(read, context, rax, 0x18, &result, sizeof(result))) return 0;
    *address = result;
    return 1;
}

int re7_original_interaction_inventory(uint64_t module, Re7OriginalInteractionRead read,
    void *context, uint64_t *address) {
    static const uint16_t offsets[] = {0x208, 0x58, 0x148, 0x30, 0x48};
    return chain(module, INVENTORY_RVA, offsets, 5, 0x10, read, context, address);
}

void re7_original_interaction_items(uint64_t rbx, Re7OriginalInteractionRead read,
    void *context, uint8_t *scrap, uint8_t *critter) {
    uint64_t name, current;
    uint16_t n, tens, ones;
    *scrap = *critter = 0;
    if (!read_at(read, context, rbx, 0x78, &name, sizeof(name)) ||
        !read_at(read, context, name, 0x14, &n, sizeof(n)) || n != 'N' ||
        !read_at(read, context, name, 0x26, &tens, sizeof(tens)) ||
        !read_at(read, context, name, 0x28, &ones, sizeof(ones)) ||
        !read_at(read, context, rbx, 0x78, &current, sizeof(current)) || current != name)
        return;
    *scrap = tens == '0' && ones == '9';
    *critter = tens == '2' && ones == '2';
}

uint8_t re7_original_interaction_scrap(uint64_t rbx, Re7OriginalInteractionRead read,
    void *context) {
    uint8_t scrap, critter;
    re7_original_interaction_items(rbx, read, context, &scrap, &critter);
    return scrap;
}

uint8_t re7_original_interaction_lockbox(uint32_t area, float px, float py, float pz) {
    /* CT 3281-3420 deliberately compares IEEE bits with unsigned JB/JA.
     * Do not replace with numeric float ranges: negative sides are reversed,
     * and (float)80000000 differs from the bare hex literal 80000000. */
    uint32_t x = bits(px), y = bits(py), z = bits(pz);
    uint8_t flag = 0;
    if (area == 5) {
        if (x >= bits(27) && x <= bits(30) && z >= bits(11) && z <= bits(13)) flag = 1;
        if (x >= bits(-10) && x <= bits(-12) &&
            !(y >= bits(2) && y < bits(80000000)) &&
            !(z >= bits(1) && z < bits(80000000)) && z <= bits(-2)) flag = 1;
        if (x >= bits(-25) && x <= bits(-28) && z >= bits(83) && z <= bits(87)) flag = 5;
        if (x >= bits(-49) && x <= bits(-51) && z >= bits(85) && z <= bits(87)) flag = 1;
        if (x >= bits(-8) && x <= bits(-11) && z >= bits(2) && z <= bits(5)) flag = 1;
        if (x >= bits(-15) && x <= bits(-18) && z >= bits(7) && z <= bits(9)) flag = 1;
        if (x >= bits(20) && x <= bits(23) && z >= bits(2) && z <= bits(4)) flag = 2;
        if (x >= bits(-10) && x <= bits(-14) && y >= bits(3) && y <= UINT32_C(0x80000000) &&
            !(z >= bits(2.01f) && z < bits(80000000)) && z <= bits(-1)) flag = 2;
        if (x >= bits(-14) && x <= bits(-20) && y <= UINT32_C(0x80000000) &&
            z >= bits(83) && z <= bits(94)) flag = 3;
        if (x >= bits(29) && x <= bits(33) &&
            (z < bits(1) || (z >= UINT32_C(0x80000000) && z <= bits(-2)))) flag = 2;
    }
    if (area == 6 && x >= bits(60) && x <= bits(62) && y >= bits(27) &&
        y <= UINT32_C(0x80000000) && z >= bits(28) && z <= bits(30)) flag = 1;
    return flag;
}
