#ifndef RAWLIFE_RELATION_H
#define RAWLIFE_RELATION_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/arena.h"
#include "sim/person.h"

/* Max relationships tracked per person. Tunable -- raise if content needs
 * NPCs with larger social graphs; each unit costs MAX_PEOPLE * ~12 bytes. */
#define MAX_RELATIONS_PER_PERSON 16

/*
 * Category: permanent, biological/legal facts about how two people are
 * connected. Independent of how they currently feel about each other or
 * what's currently going on between them -- a parent and child are always
 * REL_CATEGORY_FAMILY_PARENT_CHILD regardless of their relationship status.
 * Deliberately kept separate from status (below) so family ties and
 * romantic/sexual status can coexist on the same edge rather than
 * fighting over one field.
 */
typedef enum {
    REL_CATEGORY_NONE = 0,
    REL_CATEGORY_FAMILY_PARENT_CHILD,
    REL_CATEGORY_FAMILY_SIBLING,
    REL_CATEGORY_FAMILY_EXTENDED, /* cousins, in-laws, etc. -- one bucket for now */
} RelationCategory;

/*
 * Status: the current state of the relationship. Mutable -- events
 * transition people through these over time (e.g. acquaintance -> friend
 * -> best friend, or dating -> exclusive -> fiance -> spouse). A single
 * status field per edge, not a bitmask -- two people are in exactly one
 * relationship state at a time, even if their category (family) and
 * scalars (below) carry independent history alongside it.
 *
 * REL_STATUS_KEEP is a sentinel used only in EventDef (see event.h) to
 * mean "resolve this event without changing status" -- never stored on
 * an actual edge. Deliberately placed first (value 0) so that a
 * hand-written EventDef or a compiled event.def entry that simply omits
 * a status change defaults safely to "no change" rather than silently
 * resetting the relationship to REL_STATUS_NONE.
 */
typedef enum {
    REL_STATUS_KEEP = 0,
    REL_STATUS_NONE,
    REL_STATUS_ACQUAINTANCE,
    REL_STATUS_FRIEND,
    REL_STATUS_BEST_FRIEND,
    REL_STATUS_RIVAL,
    REL_STATUS_ENEMY,
    REL_STATUS_DATING,
    REL_STATUS_EXCLUSIVE,   /* official girlfriend/boyfriend */
    REL_STATUS_FIANCE,
    REL_STATUS_SPOUSE,
    REL_STATUS_FWB,         /* sexual, not romantic -- see the lust/romance split below */
    REL_STATUS_AFFAIR,      /* the third party in an infidelity event, see event.c */
    REL_STATUS_EX,
    REL_STATUS_COUNT,
} RelationStatus;

/*
 * Three independent 0-255 scalars, deliberately kept separate rather than
 * collapsed into one "relationship strength" number:
 *   friendship -- platonic closeness/trust
 *   romance    -- romantic affection/attachment
 *   lust       -- sexual chemistry/attraction
 * This is what lets "close friends, no romance" and "so-so friends, high
 * lust, no romance" (a fuckbuddy situation) both exist as distinct,
 * queryable states rather than being forced onto a single axis.
 */
typedef struct {
    uint32_t other_id;
    uint8_t  category;   /* RelationCategory */
    uint8_t  status;     /* RelationStatus */
    uint8_t  friendship;
    uint8_t  romance;
    uint8_t  lust;
    uint16_t flags;      /* reserved (e.g. REL_FLAG_SECRET for a hidden affair) */
} RelationEdge;

/*
 * Fixed-slot layout: person i's edges live in edges[i][0..edge_count[i]-1].
 * Not a compacting CSR -- simpler to maintain given relationships are
 * added and updated throughout a person's life, at the cost of reserving
 * MAX_RELATIONS_PER_PERSON slots per person whether they're full or not.
 */
typedef struct {
    RelationEdge edges[MAX_PEOPLE][MAX_RELATIONS_PER_PERSON];
    uint8_t      edge_count[MAX_PEOPLE];
} RelationPool;

/* Carves a RelationPool out of `arena`. Returns NULL if the arena doesn't
 * have enough remaining space. */
RelationPool* relation_pool_create(Arena* arena);

/* Returns a pointer to the edge from `person` to `other_id`, or NULL if
 * none exists yet. Pointer is into the pool's own storage -- valid until
 * the next structural change (add/remove) to `person`'s edge list. */
RelationEdge* relation_find_edge(RelationPool* pool, uint32_t person, uint32_t other_id);

/*
 * Creates a new edge between `a` and `b` with the given category, status,
 * and starting scalars, recorded on both sides. Fails (returns false, no
 * change made) if an edge between them already exists -- use
 * relation_upsert for the common "create or update" case instead of
 * calling this directly unless you specifically want create-only
 * semantics (e.g. initial family setup at spawn time).
 */
bool relation_add(RelationPool* pool, uint32_t a, uint32_t b, RelationCategory category,
                   RelationStatus status, uint8_t friendship, uint8_t romance, uint8_t lust);

/*
 * The main entry point for event resolution: updates the edge between
 * `a` and `b`, creating it first (with REL_CATEGORY_NONE) if it doesn't
 * exist yet. `new_status` of REL_STATUS_KEEP leaves status unchanged;
 * any other value overwrites it. The three deltas are added to the
 * current scalars and clamped to 0-255. Applied symmetrically to both
 * sides' copies of the edge.
 */
void relation_upsert(RelationPool* pool, uint32_t a, uint32_t b, RelationStatus new_status,
                      int8_t friendship_delta, int8_t romance_delta, int8_t lust_delta);

/* Removes the edge between `a` and `b` entirely, on both sides. Use
 * relation_upsert with REL_STATUS_EX instead if the relationship should
 * be remembered as having ended rather than erased outright. */
void relation_remove(RelationPool* pool, uint32_t a, uint32_t b);

/* Returns the id of the first person related to `person` with the given
 * status, or UINT32_MAX if none exists. E.g. REL_STATUS_SPOUSE to find a
 * spouse, REL_STATUS_FWB to find a sex partner. */
uint32_t relation_find_first_by_status(const RelationPool* pool, uint32_t person, RelationStatus status);

/* Same as above, but matches on category instead -- e.g.
 * REL_CATEGORY_FAMILY_PARENT_CHILD to find a parent or child. */
uint32_t relation_find_first_by_category(const RelationPool* pool, uint32_t person, RelationCategory category);

#endif /* RAWLIFE_RELATION_H */
