#ifndef RAWLIFE_EVENT_H
#define RAWLIFE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/person.h"
#include "sim/relation.h"
#include "sim/traits.h"

/* Where world_tick_year should look for a multi-person event's second
 * participant. See the requires_partner block below. */
typedef enum {
    PARTNER_SOURCE_ANY_POPULATION = 0, /* search all living NPCs matching partner_* constraints */
    PARTNER_SOURCE_EXISTING_RELATION,  /* search only the initiator's current relationship edges */
} PartnerSource;

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

    /*
     * Multi-person events (e.g. an affair) need a second participant.
     * If requires_partner is nonzero, world_tick_year searches the living
     * population for a candidate satisfying the partner_* constraints
     * below, picks one uniformly at random, and applies partner_* effects
     * to them in addition to the normal effects applied to the initiator.
     * If no valid candidate exists, the event does not fire for that
     * person this year (falls through, as if it were never eligible).
     */
    uint8_t  requires_partner;
    uint8_t  partner_source;         /* PartnerSource */
    uint8_t  partner_required_status; /* RelationStatus the candidate edge must have,
                                        * only checked when partner_source is
                                        * PARTNER_SOURCE_EXISTING_RELATION */
    uint8_t  partner_exclude_spouse; /* if set, initiator's current spouse (if any) is never chosen --
                                       * meaningful for PARTNER_SOURCE_ANY_POPULATION searches */
    uint8_t  partner_min_age;
    uint8_t  partner_max_age;
    uint32_t partner_required_trait_mask;
    uint32_t partner_forbidden_trait_mask;
    int8_t   partner_trait_deltas[TRAIT_COUNT];
    uint32_t partner_flag_set;
    uint32_t partner_flag_clear;

    /*
     * How this event changes the relationship between initiator and
     * partner, if requires_partner is set. sets_relation_status of
     * REL_STATUS_KEEP leaves status unchanged (e.g. an event that nudges
     * lust between existing FWBs without changing their status); any
     * other value overwrites it. Applied via relation_upsert, which
     * creates the edge if one doesn't exist yet (e.g. a brand new hookup
     * found via PARTNER_SOURCE_ANY_POPULATION).
     */
    uint8_t  sets_relation_status;     /* RelationStatus, or REL_STATUS_KEEP for no change */
    int8_t   relation_friendship_delta;
    int8_t   relation_romance_delta;
    int8_t   relation_lust_delta;
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
 * Same as event_apply, but for the second participant in a multi-person
 * event -- uses partner_trait_deltas/partner_flag_set/partner_flag_clear
 * instead of the initiator's fields. Only meaningful when
 * event->requires_partner is set.
 */
void event_apply_partner(const EventDef* event, PersonPool* pool, uint32_t partner_id);

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
