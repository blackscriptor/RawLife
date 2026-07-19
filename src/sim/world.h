#ifndef RAWLIFE_WORLD_H
#define RAWLIFE_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "sim/arena.h"
#include "sim/event.h"
#include "sim/person.h"
#include "sim/relation.h"
#include "sim/rng.h"

#define MAX_TICK_LOG_ENTRIES 64
#define MAX_PLAYER_CHOICE_OPTIONS 4

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

    /*
     * If player_id != UINT32_MAX, that person is exempted from automatic
     * event resolution during world_tick_year: instead of auto-picking a
     * weighted-random eligible event the way every NPC does, their
     * eligible events (up to MAX_PLAYER_CHOICE_OPTIONS of them) are
     * staged here and world_tick_year returns without resolving them.
     * The caller must then call world_apply_player_choice to pick one
     * (or world_skip_player_choice to let the year pass with no action)
     * before advancing further. Every other person in the world still
     * resolves normally within the same tick -- only the player's own
     * turn pauses for input.
     */
    uint32_t player_id;
    bool     player_has_pending_choice;
    uint32_t player_choice_options[MAX_PLAYER_CHOICE_OPTIONS]; /* indices into the events[] table
                                                                  passed to world_tick_year */
    uint32_t player_choice_option_count;
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
 *   2. for each alive person other than world->player_id, gathers their
 *      eligible events (via event_eligible against the supplied table)
 *      and resolves exactly one via weighted random selection, applying
 *      its consequences (via event_apply) -- each event that actually
 *      fires is recorded in tick_log (overwriting the previous tick's
 *      entries)
 *   3. if world->player_id has any eligible events this year, stages up
 *      to MAX_PLAYER_CHOICE_OPTIONS of them in player_choice_options and
 *      sets player_has_pending_choice -- these are NOT applied; the
 *      caller must resolve them via world_apply_player_choice or
 *      world_skip_player_choice
 *   4. reclaims the scratch memory used while gathering
 *
 * This is deliberately a flat linear pass over the person pool rather
 * than per-entity dispatch -- see docs/lifesim_design_doc.md section 4.
 */
void world_tick_year(WorldState* world, const EventDef* events, uint32_t event_count);

/* Designates `player_id` as the person whose events pause for a choice
 * instead of auto-resolving. Pass UINT32_MAX to remove any player
 * designation (restores fully-automatic resolution for everyone, the
 * original behavior). */
void world_set_player(WorldState* world, uint32_t player_id);

/*
 * Resolves the player's pending choice by applying
 * player_choice_options[option_index] (an index into the SAME events[]/
 * event_count table passed to the world_tick_year call that produced
 * this choice -- the caller is responsible for keeping that table
 * around until the choice is resolved). Handles multi-person events
 * (partner search, dual consequences, relationship updates) exactly
 * like automatic NPC resolution does, and logs the outcome to tick_log.
 * Does nothing if there is no pending choice or option_index is out of
 * range. Clears player_has_pending_choice on success.
 */
void world_apply_player_choice(WorldState* world, const EventDef* events, uint32_t event_count,
                                uint32_t option_index);

/* Dismisses the player's pending choice without applying any of the
 * options -- the year simply passes with no action taken. */
void world_skip_player_choice(WorldState* world);

#endif /* RAWLIFE_WORLD_H */
