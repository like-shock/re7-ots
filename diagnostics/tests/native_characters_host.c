/* Characterization of the real RT character producers and camera policy.
 * The only substitute is the process-memory boundary. Character flags, marker
 * selection, readiness and policy decisions run through production functions.
 * Expected IDs/masks/areas are derived from CT 2129-2235 and 7577-7756. */
#include "original_inputs.h"
#include "native_sites.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "%s:%d: %s (%s)\n", __func__, __LINE__, #value, example); \
    exit(1); \
} } while (0)

static const uint64_t image = UINT64_C(0x140000000);
static const uint64_t player = UINT64_C(0x200000000);
static const uint64_t owner = UINT64_C(0x300000000);
static const uint64_t head = UINT64_C(0x400000000);
static const uint64_t aim = UINT64_C(0x500000000);
static const uint64_t inventory = UINT64_C(0x600000000);
static Re7OriginalInputs inputs;
static Re7OriginalStateConfig config;
static NativeFrame frame;
static uint32_t raw_character;
static float player_z, regular_marker, clancy_marker;
static uint64_t unreadable;
static const char *example = "fixture";

/* Required by other input-producer branches, which these tests do not invoke. */
float native_get_xmm_scalar(const void *saved, unsigned reg) {
    return ((const float *)saved)[reg];
}

static int read_memory(void *context, uint64_t at, void *out, size_t size) {
    (void)context;
    if (at == unreadable) return 0;
    if (at == owner + 0x1E8 && size == 4) memcpy(out, &raw_character, 4);
    else if (at == player + 0x38 && size == 4) memcpy(out, &player_z, 4);
    else if (at == head + 0x53C && size == 4) memcpy(out, &regular_marker, 4);
    else if (at == head + 0x4BC && size == 4) memcpy(out, &clancy_marker, 4);
    else if ((at == aim || at == inventory) && size == 1) memset(out, 0, 1);
    else if (at == image + 0x8FB17F0 && size == 8) memset(out, 0, 8);
    else return 0;
    return 1;
}

static void capture(unsigned site) {
    float saved[16] = {0};
    re7_original_inputs_capture(&inputs, site, &frame, saved, image,
                               read_memory, NULL, &config);
}

static void fixture(void) {
    re7_original_inputs_init(&inputs);
    re7_original_state_defaults(&config);
    frame = (NativeFrame){0};
    frame.rdi = owner;
    raw_character = 0;
    player_z = 100;
    regular_marker = clancy_marker = 1;
    unreadable = 0;
    /* Basic pointers/scene facts were already captured. Pointer-chain discovery
     * is exercised in test_original_inputs.py; this fixture isolates the actual
     * character event and recurring marker/policy inputs. */
    inputs.player_base = player;
    inputs.head_address = head;
    inputs.aim_address = aim;
    inputs.inventory_address = inventory;
    inputs.valid |= RE7_INPUT_REQUIRED;
    inputs.state.player_available = 1;
    inputs.state.loadarea = 5;
    inputs.state.autooldcamfov = 75;
    inputs.state.player = (Re7OriginalVec){100, 100, 100};
    inputs.state.camera = inputs.state.player;
}

static void classify(uint32_t id, float z, uint32_t area) {
    raw_character = id;
    player_z = z;
    inputs.state.loadarea = area;
    capture(NS_CHARACTER_CMP);
}

static Re7OriginalState policy(void) {
    Re7OriginalState state = {0};
    re7_original_state_policy(&config, &inputs.state, &state);
    return state;
}

typedef struct {
    const char *name;
    uint32_t id, classify_area, play_area, mask, marker_offset;
    unsigned character;
} Character;

static const Character characters[] = {
    {"Ethan", 0, 5, 5, 0x01, 0x53C, RE7_ORIGINAL_CHAR_ETHAN},
    {"main Clancy", 4, 11, 11, 0x02, 0x4BC, RE7_ORIGINAL_CHAR_MAIN_CLANCY},
    {"present Mia", 2, 6, 6, 0x04, 0x53C, RE7_ORIGINAL_CHAR_PRESENT_MIA},
    {"past Mia", 5, 12, 12, 0x08, 0x53C, RE7_ORIGINAL_CHAR_PAST_MIA},
    {"DLC Clancy", 8, 31, 31, 0x10, 0x4BC, RE7_ORIGINAL_CHAR_DLC_CLANCY},
    {"Chris", 1, 35, 35, 0x20, 0x53C, RE7_ORIGINAL_CHAR_CHRIS},
    {"Joe", 0, 3, 37, 0x40, 0x53C, RE7_ORIGINAL_CHAR_JOE},
    {"Zoe", 9, 32, 32, 0x80, 0x53C, RE7_ORIGINAL_CHAR_ZOE}
};

static void choose(const Character *character) {
    example = character->name;
    fixture();
    classify(character->id, 100, character->classify_area);
    CHECK(inputs.state.characters == character->mask);
    inputs.state.loadarea = character->play_area;
}

/* Catches a missing ID branch, a stale second character bit, or truncating the
 * game's DWORD ID to a byte. The unmatched-ID behavior is the CT fallback. */
static void all_character_ids(void) {
    static const struct { uint32_t id, mask; } cases[] = {
        {0, 0x01}, {1, 0x20}, {2, 0x04}, {4, 0x02}, {5, 0x08},
        {6, 0x08}, {8, 0x10}, {9, 0x80}, {10, 0x01},
        {7, 0x01}, {0x102, 0x01}, {UINT32_MAX, 0x01}
    };
    fixture();
    for (unsigned n = 0; n < sizeof cases / sizeof cases[0]; ++n) {
        classify(cases[n].id, 100, 5);
        CHECK(inputs.state.characters == cases[n].mask);
        CHECK(inputs.pcharcflag == cases[n].id);
        CHECK(inputs.ready);
    }
}

/* Catches reversing the ID-3 branch, changing its boundary, or using unsigned
 * comparisons for negative float storage. */
static void shared_id_boundary(void) {
    static const struct { float z; uint32_t mask; } cases[] = {
        {-5, 0x02}, {0, 0x02}, {20, 0x02}, {34.999f, 0x02},
        {35, 0x04}, {35.001f, 0x04}, {50, 0x04}
    };
    fixture();
    for (unsigned n = 0; n < sizeof cases / sizeof cases[0]; ++n) {
        classify(3, cases[n].z, 7);
        CHECK(inputs.state.characters == cases[n].mask);
        CHECK(inputs.ready);
    }
}

/* Catches treating an unreadable/non-finite player Z as safe evidence for a
 * character, or losing recovery through the existing recurring policy hook. */
static void invalid_shared_id_recovers(void) {
    fixture();
    classify(1, 100, 5);
    unreadable = player + 0x38;
    classify(3, 20, 7);
    CHECK(inputs.state.characters == 0x20 && inputs.pcharcflag == 1);
    CHECK(!inputs.ready && (inputs.failed & RE7_INPUT_CHARACTER));
    unreadable = 0;
    player_z = NAN;
    capture(NS_AFTER_ADAPTIVE_DIFF);
    CHECK(inputs.state.characters == 0x20 && !inputs.ready);
    player_z = 20;
    capture(NS_AFTER_ADAPTIVE_DIFF);
    CHECK(inputs.state.characters == 0x02 && inputs.pcharcflag == 3);
    CHECK(inputs.ready && !(inputs.failed & RE7_INPUT_CHARACTER));
    unreadable = owner + 0x1E8;
    classify(9, 100, 32);
    CHECK(inputs.state.characters == 0x02 && !inputs.ready);
}

/* Catches requiring a character hook in EOZ, assuming Ethan at initialization,
 * or letting the menu fallback override an explicit character ID. */
static void joe_without_character_event(void) {
    fixture();
    inputs.state.loadarea = 37;
    capture(NS_AFTER_ADAPTIVE_DIFF);
    CHECK(!(inputs.observed & (UINT64_C(1) << NS_CHARACTER_CMP)));
    CHECK(inputs.state.characters == 0x40 && inputs.ready);
    CHECK(policy().stopflag == 0);
    classify(0, 100, 3);
    CHECK(inputs.state.characters == 0x40);
    classify(9, 100, 3);
    CHECK(inputs.state.characters == 0x80);
    classify(0, 100, 5);
    CHECK(inputs.state.characters == 0x01);
}

/* Catches applying the ordinary marker offset to either Clancy variant, or
 * ignoring a readable headless marker for a non-Ethan character. */
static void marker_offsets_gate_each_character(void) {
    for (unsigned n = 0; n < sizeof characters / sizeof characters[0]; ++n) {
        const Character *character = &characters[n];
        choose(character);
        for (unsigned phase = 0; phase < 2; ++phase) {
            regular_marker = phase ? 1 : .5f;
            clancy_marker = phase ? .5f : 1;
            float expected = character->marker_offset == 0x4BC ? clancy_marker : regular_marker;
            capture(NS_AFTER_ADAPTIVE_DIFF);
            CHECK(inputs.state.model_marker_available && inputs.ready);
            CHECK(inputs.state.model_marker == expected);
            Re7OriginalState state = policy();
            CHECK(state.stopflag == (expected != 1));
            CHECK(state.assumeheadless == (expected != 1));
        }
    }
}

/* Catches promoting the optional model read to a readiness prerequisite or
 * retaining the last headless marker after its current read becomes invalid. */
static void unavailable_marker_is_optional(void) {
    for (unsigned n = 0; n < sizeof characters / sizeof characters[0]; ++n) {
        choose(&characters[n]);
        regular_marker = clancy_marker = .5f;
        capture(NS_AFTER_ADAPTIVE_DIFF);
        CHECK(policy().assumeheadless == 1);
        unreadable = head + characters[n].marker_offset;
        capture(NS_AFTER_ADAPTIVE_DIFF);
        CHECK(!inputs.state.model_marker_available && inputs.ready);
        CHECK(inputs.failed & RE7_INPUT_MODEL);
        CHECK(policy().stopflag == 0 && policy().assumeheadless == 0);
        unreadable = 0;
        regular_marker = clancy_marker = NAN;
        capture(NS_AFTER_ADAPTIVE_DIFF);
        CHECK(!inputs.state.model_marker_available && inputs.ready);
        CHECK(policy().stopflag == 0);
        config.autodetectmodel = 0;
        regular_marker = clancy_marker = .5f;
        capture(NS_AFTER_ADAPTIVE_DIFF);
        CHECK(!inputs.state.model_marker_available && policy().stopflag == 0);
    }
}

/* Catches a default that silently disables a side character, while preserving
 * the distinct hard character opt-out that also suppresses head handling. */
static void character_gates_are_opt_in(void) {
    for (unsigned n = 0; n < sizeof characters / sizeof characters[0]; ++n) {
        choose(&characters[n]);
        capture(NS_AFTER_ADAPTIVE_DIFF);
        Re7OriginalState state = policy();
        CHECK(state.stopflag == 0 && state.blockedgun == 0 && state.assumeheadless == 0);
        config.autodisable_character[characters[n].character] = 1;
        state = policy();
        CHECK(state.stopflag == 1 && state.assumeheadless == 1);
    }
}

/* Catches accidentally shipping a tape/DLC location forced to FP by default,
 * or turning a soft location opt-out into a headless-character decision. */
static void location_gates_are_opt_in(void) {
    static const struct { uint32_t mask, area; unsigned option; } cases[] = {
        {0x01, 5, RE7_ORIGINAL_AD_ETHAN_MAIN},
        {0x02, 7, RE7_ORIGINAL_AD_DERELICT_TAPE},
        {0x02, 11, RE7_ORIGINAL_AD_BIRTHDAY_TAPE},
        {0x04, 10, RE7_ORIGINAL_AD_OLDHOUSE_TAPE},
        {0x04, 6, RE7_ORIGINAL_AD_MIA_SHIP},
        {0x08, 12, RE7_ORIGINAL_AD_PAST_SHIP},
        {0x10, 31, RE7_ORIGINAL_AD_NIGHTMARE},
        {0x10, 29, RE7_ORIGINAL_AD_BEDROOM},
        {0x10, 30, RE7_ORIGINAL_AD_21},
        {0x80, 32, RE7_ORIGINAL_AD_DAUGHTERS},
        {0x20, 35, RE7_ORIGINAL_AD_NOT_A_HERO},
        {0x40, 37, RE7_ORIGINAL_AD_END_OF_ZOE},
        {0x04, 22, RE7_ORIGINAL_AD_JACK55},
        {0x04, 23, RE7_ORIGINAL_AD_JACK55},
        {0x04, 24, RE7_ORIGINAL_AD_JACK55},
        {0x01, 34, RE7_ORIGINAL_AD_ETHAN_MUST_DIE}
    };
    for (unsigned n = 0; n < sizeof cases / sizeof cases[0]; ++n) {
        fixture();
        inputs.state.characters = cases[n].mask;
        inputs.state.loadarea = cases[n].area;
        capture(NS_AFTER_ADAPTIVE_DIFF);
        CHECK(policy().stopflag == 0);
        config.autodisable_location[cases[n].option] = 1;
        Re7OriginalState state = policy();
        CHECK(state.stopflag == 1 && state.assumeheadless == 0);
    }
}

/* Catches a side-character bypass of existing inventory/event/melee guards.
 * The user's melee switch must affect melee alone, not these other guards. */
static void scene_guards_remain_character_independent(void) {
    for (unsigned n = 0; n < sizeof characters / sizeof characters[0]; ++n) {
        choose(&characters[n]);
        capture(NS_AFTER_ADAPTIVE_DIFF);
        inputs.state.holdingmelee = re7_original_state_weapon(&config, 300) == RE7_ORIGINAL_WEAPON_MELEE;
        Re7OriginalState state = policy();
        CHECK(state.blockedgun == 1 && state.fakegbflag == 1);
        config.fpmeleeflag = 0;
        CHECK(policy().blockedgun == 0 && policy().stopflag == 0);
        inputs.state.inventory = 1;
        state = policy();
        CHECK(state.blockedgun == 1 && state.fakegbflag == 1);
        inputs.state.inventory = 0;
        inputs.state.telescopeflag = 1;
        state = policy();
        CHECK(state.stopflag == 1 && state.stoppedbloodhide == 1);
    }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    static void (*const tests[])(void) = {
        all_character_ids, shared_id_boundary, invalid_shared_id_recovers,
        joe_without_character_event, marker_offsets_gate_each_character,
        unavailable_marker_is_optional, character_gates_are_opt_in,
        location_gates_are_opt_in, scene_guards_remain_character_independent
    };
    unsigned test = (unsigned)strtoul(argv[1], NULL, 10);
    assert(test < sizeof tests / sizeof tests[0]);
    tests[test]();
    puts("native character contract passed");
    return 0;
}
