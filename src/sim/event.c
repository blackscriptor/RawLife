#include "sim/event.h"

static uint8_t clamp_add_i8(uint8_t base, int8_t delta) {
    int32_t v = (int32_t)base + delta;
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

bool event_eligible(const EventDef* event, uint8_t age, uint32_t trait_flags) {
    if (age < event->min_age || age > event->max_age) {
        return false;
    }
    if ((trait_flags & event->required_trait_mask) != event->required_trait_mask) {
        return false;
    }
    if (trait_flags & event->forbidden_trait_mask) {
        return false;
    }
    return true;
}

void event_apply(const EventDef* event, PersonPool* pool, uint32_t person_id) {
    pool->hot.trait_flags[person_id] |= event->flag_set;
    pool->hot.trait_flags[person_id] &= ~event->flag_clear;

    pool->warm.loyalty[person_id] =
        clamp_add_i8(pool->warm.loyalty[person_id], event->trait_deltas[WARM_TRAIT_LOYALTY]);
    pool->warm.charisma[person_id] =
        clamp_add_i8(pool->warm.charisma[person_id], event->trait_deltas[WARM_TRAIT_CHARISMA]);
    pool->warm.strength[person_id] =
        clamp_add_i8(pool->warm.strength[person_id], event->trait_deltas[WARM_TRAIT_STRENGTH]);
    pool->warm.beauty[person_id] =
        clamp_add_i8(pool->warm.beauty[person_id], event->trait_deltas[WARM_TRAIT_BEAUTY]);
    pool->warm.intelligence[person_id] =
        clamp_add_i8(pool->warm.intelligence[person_id], event->trait_deltas[WARM_TRAIT_INTELLIGENCE]);
    pool->warm.libido[person_id] =
        clamp_add_i8(pool->warm.libido[person_id], event->trait_deltas[WARM_TRAIT_LIBIDO]);
}

/*
 * Placeholder content. Real event catalogs get authored in the data/ DSL
 * and compiled to binary once src/tools/event_compiler exists (roadmap
 * item, not yet built) -- these four exist only to prove event_eligible
 * and event_apply work correctly across the cases that matter:
 *   - an ordinary all-ages event
 *   - an event gated at 18+ (min_age enforces the maturity boundary)
 *   - an event a LOYAL person is structurally excluded from
 *   - a crime event that sets flags for later events to react to
 */
const EventDef g_example_events[] = {
    {
        .event_id = 1,
        .name = "Made a new friend",
        .min_age = 5,
        .max_age = 255,
        .required_trait_mask = 0,
        .forbidden_trait_mask = 0,
        .weight_base = 100,
        .trait_deltas = { [WARM_TRAIT_CHARISMA] = 3 },
        .flag_set = 0,
        .flag_clear = 0,
    },
    {
        .event_id = 2,
        .name = "Caught shoplifting",
        .min_age = 10,
        .max_age = 17,
        .required_trait_mask = 0,
        .forbidden_trait_mask = TRAIT_FLAG_CRIMINAL_RECORD, /* first offense only, for now */
        .weight_base = 15,
        .trait_deltas = { [WARM_TRAIT_INTELLIGENCE] = -2 },
        .flag_set = TRAIT_FLAG_CRIMINAL_RECORD,
        .flag_clear = 0,
    },
    {
        .event_id = 3,
        .name = "Committed robbery",
        .min_age = 18,
        .max_age = 255,
        .required_trait_mask = 0,
        .forbidden_trait_mask = TRAIT_FLAG_EMPLOYED, /* placeholder gate -- real
                                                        * version will weigh this
                                                        * against other traits too */
        .weight_base = 10,
        .trait_deltas = { [WARM_TRAIT_STRENGTH] = 1 },
        .flag_set = TRAIT_FLAG_CRIMINAL_RECORD | TRAIT_FLAG_WANTED,
        .flag_clear = 0,
    },
    {
        .event_id = 4,
        .name = "Cheated on partner",
        .min_age = 18,
        .max_age = 255,
        .required_trait_mask = TRAIT_FLAG_MARRIED,
        .forbidden_trait_mask = TRAIT_FLAG_LOYAL, /* the mechanism the whole
                                                     * design hinges on: a loyal
                                                     * person is never even in
                                                     * the eligible pool for this */
        .weight_base = 8,
        .trait_deltas = { [WARM_TRAIT_LOYALTY] = -20 },
        .flag_set = 0,
        .flag_clear = 0,
    },
};

const uint32_t g_example_event_count = sizeof(g_example_events) / sizeof(g_example_events[0]);
