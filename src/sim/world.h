#ifndef RAWLIFE_WORLD_H
#define RAWLIFE_WORLD_H

#include <stdint.h>

#include "sim/arena.h"
#include "sim/event.h"
#include "sim/person.h"
#include "sim/relation.h"
#include "sim/rng.h"

#define MAX_TICK_LOG_ENTRIES 64

/*
 * Records one event actually firing during a tick -- not every eligible
 * event, only ones that resolved (had a partner if one was required,
 * etc.). partner_id is UINT32_MAX for solo events. Purely descriptive
 * data; interpreting it into human-readable text (looking up names and
 * the event's display name) is a UI concern, done by the caller.
 */
typedef struct {
    uint32_t person_id;
    uint32_t partner_id;
    uint16_t event_id;
} EventLogEntry;

typedef struct {
    PersonPool*   people;
    RelationPool* relations;
    Arena         frame_arena; /* scratch memory, rebuilt every tick, reset at tick end */
    Rng           rng;
    uint32_t      year;

    /* Events that fired during the most recent world_tick_year call.
     * Overwritten (not accumulated) each tick -- if history across
     * multiple years is ever needed, this would become a ring buffer
     * instead of a single-tick snapshot. */
    EventLogEntry tick_log[MAX_TICK_LOG_ENTRIES];
    uint32_t      tick_log_count;
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
 *      (via event_apply) -- each event that actually fires is recorded
 *      in tick_log (overwriting the previous tick's entries)
 *   3. reclaims the scratch memory used while gathering
 *
 * This is deliberately a flat linear pass over the person pool rather
 * than per-entity dispatch -- see docs/lifesim_design_doc.md section 4.
 */
void world_tick_year(WorldState* world, const EventDef* events, uint32_t event_count);

#endif /* RAWLIFE_WORLD_H */
