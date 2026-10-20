/* Exercise the real resolver. Only unavailable game-process reads are faked.
 * Literal addresses and offset expectations come from the original RT chains.
 * Broad transforms, a wrong marker branch, stale ownership or ignored read
 * failure must never earn an affirmative match. */
#include "native_head.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); \
} } while (0)
typedef struct { uint64_t address, value; size_t size; } Cell;
typedef struct {
    Cell cells[64]; unsigned count, reads, invalid_reads;
    uint64_t failed_address, changed_address, replacement;
    unsigned changed_reads, change_after;
} Memory;
static const uint64_t image = UINT64_C(0x140000000);
static const uint64_t head = UINT64_C(0x200060000);
static const uint64_t shadow = UINT64_C(0x300050034);
static const uint64_t fallback = UINT64_C(0x400060034);
static const uint64_t hair = UINT64_C(0x5000401B4);

static void put(Memory *m, uint64_t address, uint64_t value, size_t size) {
    for (unsigned i = 0; i < m->count; i++) {
        if (m->cells[i].address == address) {
            m->cells[i] = (Cell){address, value, size}; return;
        }
    }
    CHECK(m->count < sizeof m->cells / sizeof m->cells[0]);
    m->cells[m->count++] = (Cell){address, value, size};
}
static void q(Memory *m, uint64_t a, uint64_t v) { put(m, a, v, 8); }
static void marker(Memory *m, uint64_t a, uint32_t bits) { put(m, a, bits, 4); }
static int read_memory(void *ctx, uint64_t address, void *out, size_t size) {
    Memory *m = ctx; m->reads++;
    if (address < 0x10000 || address > UINT64_C(0x7FFFFFFFFFFF) ||
        !size || size - 1 > UINT64_C(0x7FFFFFFFFFFF) - address) {
        m->invalid_reads++; return 0;
    }
    if (address == m->failed_address) return 0;
    if (address == m->changed_address && size == 8 &&
        m->changed_reads++ >= m->change_after) {
        memcpy(out, &m->replacement, 8); return 1;
    }
    for (unsigned i = 0; i < m->count; i++) {
        if (m->cells[i].address == address && m->cells[i].size == size) {
            memcpy(out, &m->cells[i].value, size); return 1;
        }
    }
    return 0;
}
static void fixture(Memory *m) {
    memset(m, 0, sizeof *m);
    q(m, 0x148FBB610, 0x200000000);
    q(m, 0x2000000A0, 0x200010000);
    q(m, 0x2000100E0, 0x200020000);
    q(m, 0x200020078, 0x200030000);
    q(m, 0x200030010, 0x200040000);
    q(m, 0x200040018, 0x200050000);
    q(m, 0x2000500C0, 0x200060000);
    marker(m, 0x20006053C, 0x3F800000);
    marker(m, 0x2000604BC, 0x3F800000);
}
static void optional_fixture(Memory *m) {
    q(m, 0x200010068, 0x300010000);
    q(m, 0x300010088, 0x300020000);
    q(m, 0x300020010, 0x300030000);
    q(m, 0x3000300C0, 0x300040000);
    q(m, 0x3000400C0, 0x300050000);
    q(m, 0x200000010, 0x400010000);
    q(m, 0x400010068, 0x400020000);
    q(m, 0x400020028, 0x400030000);
    q(m, 0x400030018, 0x400040000);
    q(m, 0x400040088, 0x400050000);
    q(m, 0x4000500C0, 0x400060000);
    q(m, 0x200020080, 0x500010000);
    q(m, 0x500010010, 0x500020000);
    q(m, 0x500020018, 0x500030000);
    q(m, 0x5000300C0, 0x500040000);
}
static void refresh(NativeHeadTargets *t, Memory *m, uint32_t characters) {
    native_head_refresh(t, image, head, characters, read_memory, m);
}
static unsigned match(const NativeHeadTargets *t, Memory *m,
                      uint64_t transform, uint32_t characters, int light) {
    return native_head_match(t, image, transform, characters, light, read_memory, m);
}

/* Would fail if refresh or clear retained a previous owner's targets. */
static void cache_clear(void) {
    Memory m; NativeHeadTargets t = {9, 8, 7, 6}; fixture(&m);
    refresh(&t, &m, 1);
    CHECK(t.head == head && t.characters == 1 && !t.shadow && !t.hair);
    native_head_clear(&t);
    CHECK(!t.head && !t.shadow && !t.hair && !t.characters);
    CHECK(match(&t, &m, head, 1, 1) == NATIVE_HEAD_NONE);
    native_head_clear(NULL);
}
/* Would fail for Ethan-only matching or using the wrong Clancy marker offset. */
static void every_playable_character(void) {
    const uint32_t characters[] = {1, 2, 4, 8, 16, 32, 64, 128};
    for (unsigned i = 0; i < 8; i++) {
        Memory m; NativeHeadTargets t; fixture(&m);
        marker(&m, 0x20006053C, i == 1 || i == 4 ? 0x40000000 : 0x3F800000);
        marker(&m, 0x2000604BC, i == 1 || i == 4 ? 0x3F800000 : 0x40000000);
        refresh(&t, &m, characters[i]);
        CHECK(match(&t, &m, 0x200060000, characters[i], 0) == NATIVE_HEAD_HEAD);
    }
}
/* Would fail for one omitted neck/helmet or an overbroad range comparison. */
static void exact_head_transforms(void) {
    Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
    const uint64_t targets[] = {0x200060000, 0x200060040, 0x200060080, 0x200060280};
    for (unsigned i = 0; i < 4; i++) {
        CHECK(match(&t, &m, targets[i], 1, 0) == NATIVE_HEAD_HEAD);
        m.reads = 0;
        CHECK(match(&t, &m, targets[i] + 1, 1, 0) == NATIVE_HEAD_NONE);
        CHECK(m.reads == 0);
        CHECK(match(&t, &m, targets[i] - 1, 1, 0) == NATIVE_HEAD_NONE);
        CHECK(m.reads == 0);
    }
    CHECK(match(&t, &m, 0x2000600C0, 1, 1) == NATIVE_HEAD_NONE);
    CHECK(m.reads == 0);
}

/* Wrong/ambiguous characters must not authorize a previous character's head. */
static void character_change(void) {
    Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
    const uint32_t invalid[] = {0, 2, 3, 4, 128, 256, 0x80000000, UINT32_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        m.reads = 0;
        CHECK(match(&t, &m, head, invalid[i], 1) == NATIVE_HEAD_NONE);
        CHECK(m.reads == 0);
    }
    t.characters = 3;
    CHECK(match(&t, &m, head, 3, 1) == NATIVE_HEAD_NONE && m.reads == 0);
}
/* A readable non-one or NaN marker must not count as a visible head model. */
static void exact_marker(void) {
    const uint32_t rejected[] = {0, 0x80000000, 0x3F7FFFFF, 0x3F800001,
        0x40000000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F800001};
    for (unsigned c = 0; c < 8; c++) {
        Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1u << c);
        uint64_t address = c == 1 || c == 4 ? 0x2000604BC : 0x20006053C;
        CHECK(match(&t, &m, head, 1u << c, 0) == NATIVE_HEAD_HEAD);
        for (unsigned i = 0; i < sizeof rejected / sizeof rejected[0]; i++) {
            marker(&m, address, rejected[i]);
            CHECK(match(&t, &m, head, 1u << c, 0) == NATIVE_HEAD_NONE);
        }
        marker(&m, address, 0x3F800000); m.failed_address = address;
        CHECK(match(&t, &m, head, 1u << c, 0) == NATIVE_HEAD_NONE);
    }
}
static const uint64_t head_links[] = {0x148FBB610, 0x2000000A0,
    0x2000100E0, 0x200020078, 0x200030010, 0x200040018, 0x2000500C0};
/* Every individual missing pointer must prevent stale head writes. */
static void unreadable_head_chain(void) {
    Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
    for (unsigned i = 0; i < sizeof head_links / sizeof head_links[0]; i++) {
        m.failed_address = head_links[i];
        CHECK(match(&t, &m, head, 1, 1) == NATIVE_HEAD_NONE);
    }
    m.failed_address = 0;
    CHECK(match(&t, &m, head, 1, 1) == NATIVE_HEAD_HEAD);
}
/* Re-reading only the final address misses replacements higher in the chain. */
static void changing_head_chain(void) {
    for (unsigned i = 0; i < sizeof head_links / sizeof head_links[0]; i++) {
        Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
        m.changed_address = head_links[i]; m.change_after = 1;
        m.replacement = 0x700000000;
        CHECK(match(&t, &m, head, 1, 1) == NATIVE_HEAD_NONE);
        CHECK(!m.invalid_reads);
    }
}
/* Reload must stop an old target before reacquiring the current owner. */
static void reloaded_head(void) {
    Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
    q(&m, 0x148FBB610, 0x700000000);
    CHECK(match(&t, &m, head, 1, 0) == NATIVE_HEAD_NONE);
    q(&m, 0x7000000A0, 0x700010000);
    q(&m, 0x7000100E0, 0x700020000);
    q(&m, 0x700020078, 0x700030000);
    q(&m, 0x700030010, 0x700040000);
    q(&m, 0x700040018, 0x700050000);
    q(&m, 0x7000500C0, 0x700060000);
    marker(&m, 0x70006053C, 0x3F800000);
    CHECK(match(&t, &m, head, 1, 0) == NATIVE_HEAD_NONE);
    native_head_refresh(&t, image, 0x700060000, 1, read_memory, &m);
    CHECK(match(&t, &m, 0x700060000, 1, 0) == NATIVE_HEAD_HEAD);
    m.reads = 0;
    CHECK(match(&t, &m, head, 1, 0) == NATIVE_HEAD_NONE && !m.reads);
}
/* Invalid inputs must erase every cached branch, never retain yesterday's head. */
static void invalid_refresh(void) {
    const uint64_t invalid[] = {0, 0xFFFF, 0x7FFFFFFFFFCC, 0x7FFFFFFFFFFF, UINT64_MAX};
    Memory m; NativeHeadTargets t; fixture(&m);
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        t = (NativeHeadTargets){head, shadow, hair, 4}; m.reads = 0;
        native_head_refresh(&t, image, invalid[i], 4, read_memory, &m);
        CHECK(!t.head && !t.shadow && !t.hair && !t.characters && !m.reads);
        t = (NativeHeadTargets){head, shadow, hair, 4};
        native_head_refresh(&t, invalid[i], head, 4, read_memory, &m);
        CHECK(!t.head && !t.shadow && !t.hair && !t.characters && !m.reads);
    }
    const uint32_t ambiguous[] = {0, 3, 255, 256, UINT32_MAX};
    for (unsigned i = 0; i < sizeof ambiguous / sizeof ambiguous[0]; i++) {
        t = (NativeHeadTargets){head, shadow, hair, 4};
        native_head_refresh(&t, image, head, ambiguous[i], read_memory, &m);
        CHECK(!t.head && !t.shadow && !t.hair && !t.characters && !m.reads);
    }
    t = (NativeHeadTargets){head, shadow, hair, 4};
    native_head_refresh(&t, image, head, 4, NULL, &m);
    CHECK(!t.head && !t.shadow && !t.hair && !t.characters && !m.reads);
    native_head_refresh(NULL, image, head, 4, read_memory, &m);
    CHECK(!m.reads);
}
/* Bad pointer arithmetic must be rejected before reaching a memory callback. */
static void invalid_spans(void) {
    const uint64_t invalid[] = {0, 0xFFFF, 0x7FFFFFFFFFCC, 0x7FFFFFFFFFFF, UINT64_MAX};
    for (unsigned j = 0; j < sizeof head_links / sizeof head_links[0]; j++) {
        for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
            Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
            q(&m, head_links[j], invalid[i]);
            CHECK(match(&t, &m, head, 1, 1) == NATIVE_HEAD_NONE);
            CHECK(!m.invalid_reads);
        }
    }
    Memory m; NativeHeadTargets t; fixture(&m); refresh(&t, &m, 1);
    m.reads = 0;
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        CHECK(match(&t, &m, invalid[i], 1, 1) == NATIVE_HEAD_NONE && !m.reads);
        CHECK(native_head_match(&t, invalid[i], head, 1, 1, read_memory, &m)
              == NATIVE_HEAD_NONE && !m.reads);
    }
    CHECK(native_head_match(&t, image, head, 1, 1, NULL, &m) == NATIVE_HEAD_NONE);
    CHECK(native_head_match(NULL, image, head, 1, 1, read_memory, &m) == NATIVE_HEAD_NONE);
    CHECK(!m.reads);
}

/* The source's shadow has a primary owner, then an independent fallback. */
static void shadow_targets(void) {
    for (unsigned c = 0; c < 8; c++) {
        Memory m; NativeHeadTargets t; fixture(&m); optional_fixture(&m);
        refresh(&t, &m, 1u << c);
        CHECK(t.head == head && t.shadow == shadow);
        CHECK(match(&t, &m, 0x300050000, 1u << c, 1) == NATIVE_HEAD_SHADOW);
        m.reads = 0;
        CHECK(match(&t, &m, 0x300050000, 1u << c, 0) == NATIVE_HEAD_NONE);
        CHECK(!m.reads);
        CHECK(match(&t, &m, 0x300050001, 1u << c, 1) == NATIVE_HEAD_NONE);
        CHECK(!m.reads);
        q(&m, 0x200010068, 0);
        refresh(&t, &m, 1u << c);
        CHECK(t.head == head && t.shadow == fallback);
        CHECK(match(&t, &m, 0x400060000, 1u << c, 1) == NATIVE_HEAD_SHADOW);
    }
}
/* Past Mia and other characters must never hide present Mia's extra hair. */
static void present_mia_hair(void) {
    for (unsigned c = 0; c < 8; c++) {
        Memory m; NativeHeadTargets t; fixture(&m); optional_fixture(&m);
        refresh(&t, &m, 1u << c); m.reads = 0;
        if (c == 2) {
            CHECK(t.hair == hair);
            CHECK(match(&t, &m, 0x500040180, 4, 0) == NATIVE_HEAD_HAIR);
            m.reads = 0;
            CHECK(match(&t, &m, 0x500040181, 4, 0) == NATIVE_HEAD_NONE);
            CHECK(!m.reads);
        } else {
            CHECK(!t.hair);
            CHECK(match(&t, &m, 0x500040180, 1u << c, 1) == NATIVE_HEAD_NONE);
            CHECK(!m.reads);
            t.hair = hair;
            CHECK(match(&t, &m, 0x500040180, 1u << c, 1) == NATIVE_HEAD_NONE);
            CHECK(!m.reads);
        }
    }
}
/* Optional absence must clear old targets without disabling a valid head. */
static void optional_cache_reset(void) {
    Memory m; NativeHeadTargets t; fixture(&m); optional_fixture(&m);
    refresh(&t, &m, 4); CHECK(t.shadow == shadow && t.hair == hair);
    q(&m, 0x200010068, 0); q(&m, 0x200000010, 0); q(&m, 0x200020080, 0);
    refresh(&t, &m, 4);
    CHECK(t.head == head && t.characters == 4 && !t.shadow && !t.hair);
    CHECK(match(&t, &m, head, 4, 0) == NATIVE_HEAD_HEAD);
    optional_fixture(&m); refresh(&t, &m, 4);
    CHECK(t.shadow == shadow && t.hair == hair);
    refresh(&t, &m, 8);
    CHECK(t.head == head && t.characters == 8 && t.shadow == shadow && !t.hair);
    native_head_clear(&t);
    CHECK(!t.head && !t.shadow && !t.hair && !t.characters);
}
static const uint64_t primary_links[] = {0x148FBB610, 0x2000000A0,
    0x200010068, 0x300010088, 0x300020010, 0x3000300C0, 0x3000400C0};
static const uint64_t fallback_links[] = {0x148FBB610, 0x200000010,
    0x400010068, 0x400020028, 0x400030018, 0x400040088, 0x4000500C0};
static const uint64_t hair_links[] = {0x148FBB610, 0x2000000A0,
    0x2000100E0, 0x200020080, 0x500010010, 0x500020018, 0x5000300C0};
typedef struct {
    const uint64_t *links; uint64_t transform, replacement_transform;
    unsigned result;
} Branch;
static const Branch branches[] = {
    {primary_links, 0x300050000, 0x300090000, NATIVE_HEAD_SHADOW},
    {fallback_links, 0x400060000, 0x400090000, NATIVE_HEAD_SHADOW},
    {hair_links, 0x500040180, 0x500090180, NATIVE_HEAD_HAIR}
};
static void branch_fixture(Memory *m, NativeHeadTargets *t, unsigned branch) {
    fixture(m); optional_fixture(m);
    if (branch == 1) q(m, 0x200010068, 0);
    refresh(t, m, 4);
    CHECK(match(t, m, branches[branch].transform, 4, 1) == branches[branch].result);
}
/* An optional cached target cannot bypass its live chain, generic head or marker. */
static void unreadable_optional_chains(void) {
    for (unsigned b = 0; b < 3; b++) {
        Memory m; NativeHeadTargets t; branch_fixture(&m, &t, b);
        m.failed_address = 0x20006053C;
        CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
        for (unsigned i = 0; i < 7; i++) {
            m.failed_address = head_links[i];
            CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
        }
        for (unsigned i = 0; i < 7; i++) {
            m.failed_address = branches[b].links[i];
            CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
        }
        m.failed_address = 0;
        CHECK(match(&t, &m, branches[b].transform, 4, 1) == branches[b].result);
    }
}
/* Leaf reuse at a new owner must stop old shadow/hair matches before refresh. */
static void reloaded_optional_targets(void) {
    for (unsigned b = 0; b < 3; b++) {
        Memory m; NativeHeadTargets t; branch_fixture(&m, &t, b);
        q(&m, branches[b].links[6], b == 2 ? 0x500090000 : branches[b].replacement_transform);
        CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
        refresh(&t, &m, 4);
        CHECK(match(&t, &m, branches[b].replacement_transform, 4, 1) == branches[b].result);
        m.reads = 0;
        CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
        CHECK(!m.reads);
    }
}
/* A match needs reverse checks on every optional chain link, too. */
static void changing_optional_chains(void) {
    for (unsigned b = 0; b < 3; b++) {
        for (unsigned i = 0; i < 7; i++) {
            Memory m; NativeHeadTargets t; branch_fixture(&m, &t, b);
            m.changed_address = branches[b].links[i]; m.change_after = 1;
            m.replacement = 0x700000000;
            CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
            CHECK(!m.invalid_reads);
        }
    }
}
/* Even a valid pointer must leave room for the entire DWORD at the leaf. */
static void invalid_optional_spans(void) {
    const uint64_t invalid[] = {0, 0xFFFF, 0x7FFFFFFFFFCC, 0x7FFFFFFFFFFF, UINT64_MAX};
    for (unsigned b = 0; b < 3; b++) {
        for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
            Memory m; NativeHeadTargets t; branch_fixture(&m, &t, b);
            if (b == 0) q(&m, 0x200000010, 0); /* no fallback available */
            q(&m, branches[b].links[6], invalid[i]);
            CHECK(match(&t, &m, branches[b].transform, 4, 1) == NATIVE_HEAD_NONE);
            refresh(&t, &m, 4);
            CHECK(b == 2 ? !t.hair : !t.shadow);
            CHECK(match(&t, &m, head, 4, 0) == NATIVE_HEAD_HEAD);
            CHECK(!m.invalid_reads);
        }
    }
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (!strcmp(argv[1], "cache_clear")) cache_clear();
    else if (!strcmp(argv[1], "every_playable_character")) every_playable_character();
    else if (!strcmp(argv[1], "exact_head_transforms")) exact_head_transforms();
    else if (!strcmp(argv[1], "character_change")) character_change();
    else if (!strcmp(argv[1], "exact_marker")) exact_marker();
    else if (!strcmp(argv[1], "unreadable_head_chain")) unreadable_head_chain();
    else if (!strcmp(argv[1], "changing_head_chain")) changing_head_chain();
    else if (!strcmp(argv[1], "reloaded_head")) reloaded_head();
    else if (!strcmp(argv[1], "invalid_refresh")) invalid_refresh();
    else if (!strcmp(argv[1], "invalid_spans")) invalid_spans();
    else if (!strcmp(argv[1], "shadow_targets")) shadow_targets();
    else if (!strcmp(argv[1], "present_mia_hair")) present_mia_hair();
    else if (!strcmp(argv[1], "optional_cache_reset")) optional_cache_reset();
    else if (!strcmp(argv[1], "unreadable_optional_chains")) unreadable_optional_chains();
    else if (!strcmp(argv[1], "reloaded_optional_targets")) reloaded_optional_targets();
    else if (!strcmp(argv[1], "changing_optional_chains")) changing_optional_chains();
    else if (!strcmp(argv[1], "invalid_optional_spans")) invalid_optional_spans();
    else CHECK(0);
    puts("native head contract passed"); return 0;
}
