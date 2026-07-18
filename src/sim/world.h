#ifndef RAWLIFE_WORLD_H
#define RAWLIFE_WORLD_H

#include <stdint.h>

#include "sim/arena.h"
#include "sim/event.h"
#include "sim/person.h"
#include "sim/relation.h"
#include "sim/rng.h"

typedef struct {
    PersonPool*   people;
    RelationPool* relations;
    Arena         frame_arena; /* scratch memory, rebuilt every tick, reset at tick end */
    Rng           rng;
    uint32_t      year;
} WorldState;

/*
 * Carves a WorldState (including its PersonPool and per-tick scratch
 * space) out of `arena`. Returns NULL if the arena doesn't have enough
 * remaining space.
 */
WorldState* world_create(Arena* arena, uint64_t seed);

/*
 * Advances the world by one year:
 *   1. ages every alive person up
 *   2. for each alive person, gathers their eligible events (via
 *      event_eligible against the supplied table) and resolves exactly
 *      one via weighted random selection, applying its consequences
 *      (via event_apply)
 *   3. reclaims the scratch memory used while gathering
 *
 * This is deliberately a flat linear pass over the person pool rather
 * than per-entity dispatch -- see docs/lifesim_design_doc.md section 4.
 */
void world_tick_year(WorldState* world, const EventDef* events, uint32_t event_count);

#endif /* RAWLIFE_WORLD_H */
