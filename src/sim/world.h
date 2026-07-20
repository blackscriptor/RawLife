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
#define NO_CHOICE_MADE 0xFFu /* sentinel in EventLogEntry.choice_index: event had no choices */

/*
 * Records one event actually firing during a tick -- not every eligible
 * event, only ones that resolved (had a partner if one was required,
 * etc.). partner_id is UINT32_MAX for solo events. choice_index is which
 * EventChoice was applied (if the event had choices), or NO_CHOICE_MADE
 * if it resolved via its single top-level outcome. Purely descriptive
 * data; interpreting it into human-readable text (looking up names and
 * the event's/choice's display name) is a UI concern, done by the caller.
 */
typedef struct {
    uint32_t person_id;
    uint32_t partner_id;
    uint16_t event_id;
    uint8_t  choice_index;
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
     * If player_id != UINT32_MAX, that person's turn is handled
     * specially: WHICH event happens to them is still decided by the
     * exact same weighted-random selection used for every NPC (the
     * player doesn't get to pick their circumstances) -- but if that
     * event has choice_count > 0, resolution pauses instead of
     * auto-picking a choice, and player_pending_event_index records
     * which event (an index into the events[] table passed to
     * world_tick_year) is awaiting a reaction. The caller must then call
     * world_apply_player_choice with which EventChoice to apply, or
     * world_skip_player_choice to let the year pass with no reaction,
     * before advancing further. Events with no choices resolve
     * immediately for the player exactly like they do for any NPC --
     * only choice-bearing events pause.
     */
    uint32_t player_id;
    bool     player_has_pending_choice;
    uint32_t player_pending_event_index; /* valid only when player_has_pending_choice */
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
 *      event_eligible against the supplied table) and picks exactly one
 *      via weighted random selection -- the same selection process for
 *      everyone, player included
 *   3. for NPCs (and for the player when the selected event has no
 *      choices), resolves it immediately: applies consequences (via
 *      event_apply, or a weighted-random pick among the event's
 *      EventChoices if it has any) and records it in tick_log
 *   4. for the player, if the selected event DOES have choices, pauses
 *      instead of picking one automatically -- see player_id above
 *   5. reclaims the scratch memory used while gathering
 *
 * This is deliberately a flat linear pass over the person pool rather
 * than per-entity dispatch -- see docs/lifesim_design_doc.md section 4.
 */
void world_tick_year(WorldState* world, const EventDef* events, uint32_t event_count);

/* Designates `player_id` as the person whose choice-bearing events pause
 * for input. Pass UINT32_MAX to remove any player designation (restores
 * fully-automatic resolution for everyone, the original behavior). */
void world_set_player(WorldState* world, uint32_t player_id);

/*
 * Resolves the player's pending event by applying
 * events[player_pending_event_index].choices[choice_index] (the caller
 * is responsible for passing the SAME events[]/event_count table used by
 * the world_tick_year call that produced this pending choice). Handles
 * partner search/effects/relationship updates exactly like automatic
 * resolution does if the event also requires a partner, and logs the
 * outcome to tick_log. Does nothing if there is no pending choice or
 * choice_index is out of range for that event. Clears
 * player_has_pending_choice on success.
 */
void world_apply_player_choice(WorldState* world, const EventDef* events, uint32_t event_count,
                                uint32_t choice_index);

/* Dismisses the player's pending event without applying any reaction --
 * the year simply passes with no response taken. */
void world_skip_player_choice(WorldState* world);

#endif /* RAWLIFE_WORLD_H */
