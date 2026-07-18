#ifndef RAWLIFE_TRAITS_H
#define RAWLIFE_TRAITS_H

#include <stdint.h>

/*
 * Boolean status/trait flags, packed into PersonHot.trait_flags (see
 * person.h). Bit-based so eligibility filtering in event.c is pure
 * bitmask AND/OR -- no per-trait branching, no string comparison.
 *
 * Add new flags here as content needs them. 32 bits available; if that
 * ever isn't enough, trait_flags can grow to a small fixed array without
 * changing the filtering approach.
 */
enum {
    TRAIT_FLAG_LOYAL           = 1u << 0,
    TRAIT_FLAG_CRIMINAL_RECORD = 1u << 1,
    TRAIT_FLAG_WANTED          = 1u << 2,
    TRAIT_FLAG_MARRIED         = 1u << 3,
    TRAIT_FLAG_EMPLOYED        = 1u << 4,
};

/*
 * Stable numeric indices for PersonWarm's scalar traits, so EventDef can
 * reference "which trait" via trait_deltas[index] instead of hardcoding
 * struct field names. person.h's PersonWarm fields and this enum are
 * extended together -- add a field there, add a matching index here.
 *
 * trait_deltas arrays are sized to TRAIT_COUNT (person.h), which is
 * larger than WARM_TRAIT_COUNT on purpose: room to add traits later
 * without changing EventDef's binary layout.
 */
typedef enum {
    WARM_TRAIT_LOYALTY = 0,
    WARM_TRAIT_CHARISMA,
    WARM_TRAIT_STRENGTH,
    WARM_TRAIT_BEAUTY,
    WARM_TRAIT_INTELLIGENCE,
    WARM_TRAIT_LIBIDO,
    WARM_TRAIT_COUNT
} WarmTraitId;

#endif /* RAWLIFE_TRAITS_H */
