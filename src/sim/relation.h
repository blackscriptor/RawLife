#ifndef RAWLIFE_RELATION_H
#define RAWLIFE_RELATION_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/arena.h"
#include "sim/person.h"

/* Max relationships tracked per person. Tunable -- raise if content needs
 * NPCs with larger social graphs; each unit costs MAX_PEOPLE * 8 bytes. */
#define MAX_RELATIONS_PER_PERSON 16

/*
 * Relationship type. Currently stored symmetrically (the same type is
 * recorded on both sides of the edge) -- e.g. RELATION_PARENT is used for
 * both the parent's and the child's copy of the edge. Directional pairs
 * (RELATION_PARENT_OF / RELATION_CHILD_OF) can be split out later once
 * birth/family mechanics need to distinguish "my parent" from "my child"
 * rather than just "we're related this way."
 */
typedef enum {
    RELATION_NONE = 0,
    RELATION_FAMILY_PARENT_CHILD,
    RELATION_FAMILY_SIBLING,
    RELATION_SPOUSE,
    RELATION_FRIEND,
    RELATION_RIVAL,
    RELATION_AFFAIR, /* the "other party" in an infidelity event -- see event.c */
} RelationType;

typedef struct {
    uint32_t other_id;
    uint8_t  type;     /* RelationType */
    uint8_t  strength; /* 0-255 fixed point */
    uint16_t flags;    /* reserved for future use (e.g. "secret") */
} RelationEdge;

/*
 * Fixed-slot layout: person i's edges live in edges[i][0..edge_count[i]-1].
 * Not a compacting CSR -- simpler to maintain given relationships are
 * added and removed throughout a person's life, at the cost of reserving
 * MAX_RELATIONS_PER_PERSON slots per person whether they're full or not.
 */
typedef struct {
    RelationEdge edges[MAX_PEOPLE][MAX_RELATIONS_PER_PERSON];
    uint8_t      edge_count[MAX_PEOPLE];
} RelationPool;

/* Carves a RelationPool out of `arena`. Returns NULL if the arena doesn't
 * have enough remaining space. */
RelationPool* relation_pool_create(Arena* arena);

/*
 * Adds a relationship between `a` and `b`, recording it in both people's
 * edge lists (so lookups from either side are O(1), no traversal needed).
 * Silently does nothing if either person's edge list is already full --
 * check the return value if that matters to calling code.
 * Returns true if the edge was added on both sides.
 */
bool relation_add(RelationPool* pool, uint32_t a, uint32_t b, RelationType type, uint8_t strength);

/* Removes the first edge from `person` to `other_id` of the given type,
 * if present. Only removes `person`'s copy -- call twice (with a/b
 * swapped) to fully sever a relationship on both sides. */
void relation_remove(RelationPool* pool, uint32_t person, uint32_t other_id, RelationType type);

/*
 * Returns the id of the first person related to `person` by `type`, or
 * UINT32_MAX if none exists. Useful for simple queries like "does this
 * person have a spouse."
 */
uint32_t relation_find_first(const RelationPool* pool, uint32_t person, RelationType type);

#endif /* RAWLIFE_RELATION_H */
