#ifndef RAWLIFE_EVENT_H
#define RAWLIFE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/arena.h"
#include "sim/person.h"
#include "sim/relation.h"
#include "sim/traits.h"

/* Where world_tick_year should look for a multi-person event's second
 * participant. See the requires_partner block below. */
typedef enum {
    PARTNER_SOURCE_ANY_POPULATION = 0, /* search all living NPCs matching partner_* constraints */
    PARTNER_SOURCE_EXISTING_RELATION,  /* search only the initiator's current relationship edges */
} PartnerSource;

#define EVENT_NAME_MAX_LEN 32

/*
 * A life event. Fully data-describable: authored in a human-readable DSL
 * (see src/data/events.def) and compiled by src/tools/event_compiler into
 * a flat binary loaded at startup via event_table_load(). This struct's
 * layout IS the binary format, so it must stay a plain POD struct -- no
 * pointers. `name` is a fixed-size array rather than `const char*` for
 * exactly this reason: a pointer written to a save/data file is garbage
 * the moment it's read back in a different process.
 */
typedef struct {
    uint16_t event_id;
    char     name[EVENT_NAME_MAX_LEN];
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
 * Placeholder hardcoded event table standing in for real content. Kept
 * around as a reference/test fixture even now that event_table_load()
 * exists -- see src/data/events.def for the actual authorable content,
 * compiled to events.bin by src/tools/event_compiler.
 */
extern const EventDef g_example_events[];
extern const uint32_t g_example_event_count;

/* Binary event file format: a small header, then a flat array of
 * EventDef. Written by src/tools/event_compiler, read by
 * event_table_load below. */
#define EVENT_FILE_MAGIC   0x56454C52u /* 'RLEV' */
#define EVENT_FILE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t event_count;
} EventFileHeader;

/*
 * Loads a compiled event table from `path` (as produced by
 * src/tools/event_compiler) into memory carved from `arena`. On success,
 * *out_table points at an array of *out_count EventDefs and returns true.
 * Returns false (leaving out_table and out_count untouched) if the file
 * can't be opened, the header magic/version doesn't match, the arena
 * doesn't have enough space, or a read fails partway through.
 */
bool event_table_load(Arena* arena, const char* path, const EventDef** out_table, uint32_t* out_count);

#endif /* RAWLIFE_EVENT_H */
