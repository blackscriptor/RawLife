#ifndef RAWLIFE_EVENT_H
#define RAWLIFE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/person.h"
#include "sim/traits.h"

/*
 * A life event. Fully data-describable -- once src/tools/event_compiler
 * exists, these will be authored in a human-readable DSL and compiled to
 * a flat binary loaded at startup, rather than hardcoded C arrays like
 * g_example_events below. The struct layout is the actual binary format,
 * so it stays a plain POD struct: no pointers except the debug-only name.
 */
typedef struct {
    uint16_t event_id;
    const char* name;              /* debug/placeholder only -- real content
                                     * will reference a string table, not an
                                     * embedded pointer, once compiled from data */
    uint8_t  min_age;
    uint8_t  max_age;
    uint32_t required_trait_mask;  /* PersonHot.trait_flags bits that must ALL be set */
    uint32_t forbidden_trait_mask; /* PersonHot.trait_flags bits that must ALL be clear */
    uint16_t weight_base;          /* relative selection weight among eligible events */
    int8_t   trait_deltas[TRAIT_COUNT]; /* indexed by WarmTraitId, applied on resolution */
    uint32_t flag_set;             /* trait_flags bits to set on resolution */
    uint32_t flag_clear;           /* trait_flags bits to clear on resolution */
} EventDef;

/*
 * Returns true if `event` is a valid outcome for someone of the given age
 * with the given boolean trait flags. Pure range/bitmask checks -- this is
 * the single choke point that both age-gating and trait-gating (e.g. a
 * loyal person being excluded from infidelity-type events) flow through.
 * See docs/lifesim_design_doc.md section 5.4.
 */
bool event_eligible(const EventDef* event, uint8_t age, uint32_t trait_flags);

/*
 * Applies an event's consequences to a person: sets/clears boolean trait
 * flags and nudges their warm scalar traits by trait_deltas, clamped to
 * the 0-255 fixed-point range.
 */
void event_apply(const EventDef* event, PersonPool* pool, uint32_t person_id);

/*
 * Placeholder hardcoded event table standing in for the real data-driven
 * table until the offline DSL compiler exists (see roadmap). Deliberately
 * includes an all-ages event, an 18+ gated event, and a loyalty-gated
 * event, to prove the eligibility mechanism end to end before real
 * content gets authored.
 */
extern const EventDef g_example_events[];
extern const uint32_t g_example_event_count;

#endif /* RAWLIFE_EVENT_H */
