#ifndef RAWLIFE_PERSON_H
#define RAWLIFE_PERSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sim/arena.h"

#define MAX_PEOPLE        8192
#define TRAIT_COUNT       24
#define FIRST_NAME_MAX_LEN 20
#define LAST_NAME_MAX_LEN  20

#define SEX_FEMALE 0
#define SEX_MALE   1

/*
 * Fields are grouped by how often the simulation touches them, not by
 * logical category -- this is what keeps a full-population pass cache
 * friendly. See docs/lifesim_design_doc.md section 3.
 */

/* Hot: read/written for every alive person, every simulation tick. */
typedef struct {
    uint8_t  age[MAX_PEOPLE];
    uint8_t  sex[MAX_PEOPLE];
    uint8_t  alive_bits[(MAX_PEOPLE + 7) / 8]; /* packed bitset */
    uint32_t trait_flags[MAX_PEOPLE];          /* boolean traits, e.g. loyal, criminal */
} PersonHot;

/* Warm: read/written only when an event actually resolves for this person. */
typedef struct {
    uint8_t  loyalty[MAX_PEOPLE];      /* 0-255 fixed point */
    uint8_t  charisma[MAX_PEOPLE];
    uint8_t  strength[MAX_PEOPLE];
    uint8_t  beauty[MAX_PEOPLE];
    uint8_t  intelligence[MAX_PEOPLE];
    uint8_t  libido[MAX_PEOPLE];
    uint16_t kink_mask[MAX_PEOPLE];    /* bitmask into a kink ID table (added later) */
} PersonWarm;

/*
 * Cold: UI/flavor data, rarely touched by simulation logic.
 *
 * first_name and last_name are separate fields, not one combined "name"
 * string: last_name is inherited (a child's matches their parents'; see
 * person_spawn) and will eventually be mutable at marriage/divorce time
 * (spouses may take one partner's name, combine both, or a name may
 * revert on divorce -- not implemented yet, but keeping the fields
 * separate now is what makes that possible later without restructuring
 * person data again). first_name is chosen -- by the player for their
 * own character, or however NPC name generation works once that exists
 * (see docs/lifesim_design_doc.md section 17 on regional name pools).
 */
typedef struct {
    char     first_name[MAX_PEOPLE][FIRST_NAME_MAX_LEN];
    char     last_name[MAX_PEOPLE][LAST_NAME_MAX_LEN];
    uint32_t family_id[MAX_PEOPLE];
    uint32_t birth_year[MAX_PEOPLE];
} PersonCold;

typedef struct {
    PersonHot  hot;
    PersonWarm warm;
    PersonCold cold;

    uint32_t active_count;        /* high-water mark of slots ever used, not a live count */
    uint32_t free_list[MAX_PEOPLE];
    uint32_t free_count;
} PersonPool;

/* Carves a PersonPool out of `arena` and initializes it (all slots dead,
 * free list empty -- fresh slots come from active_count until it hits
 * MAX_PEOPLE, then only recycled slots are available).
 * Returns NULL if the arena doesn't have enough remaining space. */
PersonPool* person_pool_create(Arena* arena);

bool person_is_alive(const PersonPool* pool, uint32_t id);

/* Allocates a person slot (recycled from the free list if one exists,
 * otherwise a fresh slot) and initializes their basic fields.
 * Returns UINT32_MAX if the pool is completely full (no free slots and
 * active_count has reached MAX_PEOPLE). */
uint32_t person_spawn(PersonPool* pool, const char* first_name, const char* last_name,
                       uint8_t sex, uint32_t birth_year);

/* Formats "First Last" into `out` (a caller-provided buffer of at least
 * out_size bytes), truncating if necessary. Convenience for UI/logging
 * code that wants a single display string rather than the two separate
 * fields. */
void person_get_full_name(const PersonPool* pool, uint32_t id, char* out, size_t out_size);

/* Marks a person dead and returns their slot to the free list for reuse
 * by a future person_spawn call. */
void person_kill(PersonPool* pool, uint32_t id);

#endif /* RAWLIFE_PERSON_H */
