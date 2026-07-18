#ifndef RAWLIFE_PERSON_H
#define RAWLIFE_PERSON_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/arena.h"

#define MAX_PEOPLE   8192
#define TRAIT_COUNT  24
#define MAX_NAME_LEN 32

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

/* Cold: UI/flavor data, rarely touched by simulation logic. */
typedef struct {
    char     name[MAX_PEOPLE][MAX_NAME_LEN];
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
uint32_t person_spawn(PersonPool* pool, const char* name, uint8_t sex, uint32_t birth_year);

/* Marks a person dead and returns their slot to the free list for reuse
 * by a future person_spawn call. */
void person_kill(PersonPool* pool, uint32_t id);

#endif /* RAWLIFE_PERSON_H */
