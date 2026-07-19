#include "sim/world.h"

#define TICK_SCRATCH_SIZE (64 * 1024)

WorldState* world_create(Arena* arena, uint64_t seed) {
    WorldState* world = (WorldState*)arena_alloc(arena, sizeof(WorldState), _Alignof(WorldState));
    if (world == NULL) {
        return NULL;
    }

    world->people = person_pool_create(arena);
    if (world->people == NULL) {
        return NULL;
    }

    world->relations = relation_pool_create(arena);
    if (world->relations == NULL) {
        return NULL;
    }

    void* scratch = arena_alloc(arena, TICK_SCRATCH_SIZE, 16);
    if (scratch == NULL) {
        return NULL;
    }
    world->frame_arena.base = (uint8_t*)scratch;
    world->frame_arena.size = TICK_SCRATCH_SIZE;
    world->frame_arena.used = 0;

    rng_seed(&world->rng, seed);
    world->year = 0;
    world->tick_log_count = 0;

    return world;
}

static void pass_age_up(PersonPool* pool) {
    for (uint32_t i = 0; i < pool->active_count; i++) {
        if (!person_is_alive(pool, i)) {
            continue;
        }
        if (pool->hot.age[i] < 255) {
            pool->hot.age[i]++;
        }
    }
}

/*
 * Checks whether candidate `c` satisfies an event's partner_* age/trait
 * constraints. Shared by both partner-source search modes below.
 */
static bool candidate_matches(const PersonPool* pool, uint32_t c, const EventDef* event) {
    if (pool->hot.age[c] < event->partner_min_age || pool->hot.age[c] > event->partner_max_age) {
        return false;
    }
    if ((pool->hot.trait_flags[c] & event->partner_required_trait_mask) != event->partner_required_trait_mask) {
        return false;
    }
    if (pool->hot.trait_flags[c] & event->partner_forbidden_trait_mask) {
        return false;
    }
    return true;
}

/*
 * Two-pass uniform selection among living NPCs satisfying an event's
 * partner_* constraints. O(active_count) per call -- acceptable at medium
 * population scale for now; if profiling later shows this is hot,
 * candidates can be cached per-tick instead of rescanned per event.
 */
static uint32_t find_partner_any_population(WorldState* world, uint32_t initiator, const EventDef* event) {
    PersonPool* pool = world->people;

    uint32_t exclude_id = UINT32_MAX;
    if (event->partner_exclude_spouse) {
        exclude_id = relation_find_first_by_status(world->relations, initiator, REL_STATUS_SPOUSE);
    }

    uint32_t match_count = 0;
    for (uint32_t c = 0; c < pool->active_count; c++) {
        if (c == initiator || !person_is_alive(pool, c)) {
            continue;
        }
        if (event->partner_exclude_spouse && c == exclude_id) {
            continue;
        }
        if (candidate_matches(pool, c, event)) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return UINT32_MAX;
    }

    uint32_t target_index = (uint32_t)(rng_next(&world->rng) % match_count);
    uint32_t seen = 0;
    for (uint32_t c = 0; c < pool->active_count; c++) {
        if (c == initiator || !person_is_alive(pool, c)) {
            continue;
        }
        if (event->partner_exclude_spouse && c == exclude_id) {
            continue;
        }
        if (!candidate_matches(pool, c, event)) {
            continue;
        }
        if (seen == target_index) {
            return c;
        }
        seen++;
    }

    return UINT32_MAX; /* unreachable if match_count > 0, kept for safety */
}

/*
 * Selection among the initiator's EXISTING relationship edges with a
 * matching status (e.g. only fire if they already have a FRIEND-status
 * edge). Bounded by MAX_RELATIONS_PER_PERSON per person, so this is cheap
 * regardless of population size.
 */
static uint32_t find_partner_existing_relation(WorldState* world, uint32_t initiator, const EventDef* event) {
    PersonPool* pool = world->people;
    RelationPool* relations = world->relations;

    uint8_t edge_count = relations->edge_count[initiator];
    uint32_t match_count = 0;
    for (uint8_t e = 0; e < edge_count; e++) {
        const RelationEdge* edge = &relations->edges[initiator][e];
        if (edge->status != (uint8_t)event->partner_required_status) {
            continue;
        }
        if (!person_is_alive(pool, edge->other_id)) {
            continue;
        }
        if (candidate_matches(pool, edge->other_id, event)) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return UINT32_MAX;
    }

    uint32_t target_index = (uint32_t)(rng_next(&world->rng) % match_count);
    uint32_t seen = 0;
    for (uint8_t e = 0; e < edge_count; e++) {
        const RelationEdge* edge = &relations->edges[initiator][e];
        if (edge->status != (uint8_t)event->partner_required_status) {
            continue;
        }
        if (!person_is_alive(pool, edge->other_id)) {
            continue;
        }
        if (!candidate_matches(pool, edge->other_id, event)) {
            continue;
        }
        if (seen == target_index) {
            return edge->other_id;
        }
        seen++;
    }

    return UINT32_MAX; /* unreachable if match_count > 0, kept for safety */
}

static uint32_t find_event_partner(WorldState* world, uint32_t initiator, const EventDef* event) {
    if (event->partner_source == PARTNER_SOURCE_EXISTING_RELATION) {
        return find_partner_existing_relation(world, initiator, event);
    }
    return find_partner_any_population(world, initiator, event);
}

static void log_event(WorldState* world, uint32_t person_id, uint32_t partner_id, uint16_t event_id) {
    if (world->tick_log_count >= MAX_TICK_LOG_ENTRIES) {
        return; /* log full for this tick -- silently drop rather than overflow */
    }
    EventLogEntry* entry = &world->tick_log[world->tick_log_count++];
    entry->person_id = person_id;
    entry->partner_id = partner_id;
    entry->event_id = event_id;
}

static void pass_resolve_events(WorldState* world, const EventDef* events, uint32_t event_count) {
    PersonPool* pool = world->people;

    /* Scratch buffers sized to event_count, reused per person -- overwritten
     * each iteration, only actually reclaimed once at the end of the tick. */
    uint32_t* eligible_ids = (uint32_t*)arena_alloc(
        &world->frame_arena, sizeof(uint32_t) * event_count, sizeof(uint32_t));
    uint32_t* eligible_weights = (uint32_t*)arena_alloc(
        &world->frame_arena, sizeof(uint32_t) * event_count, sizeof(uint32_t));
    if (eligible_ids == NULL || eligible_weights == NULL) {
        return; /* scratch arena too small for this event table -- bump TICK_SCRATCH_SIZE */
    }

    for (uint32_t i = 0; i < pool->active_count; i++) {
        if (!person_is_alive(pool, i)) {
            continue;
        }

        uint32_t eligible_count = 0;
        uint32_t total_weight = 0;
        for (uint32_t e = 0; e < event_count; e++) {
            if (event_eligible(&events[e], pool->hot.age[i], pool->hot.trait_flags[i])) {
                eligible_ids[eligible_count] = e;
                eligible_weights[eligible_count] = events[e].weight_base;
                total_weight += events[e].weight_base;
                eligible_count++;
            }
        }

        if (eligible_count == 0 || total_weight == 0) {
            continue; /* nothing eligible this year for this person */
        }

        uint32_t roll = (uint32_t)(rng_next(&world->rng) % total_weight);
        uint32_t chosen = eligible_ids[0];
        uint32_t cumulative = 0;
        for (uint32_t k = 0; k < eligible_count; k++) {
            cumulative += eligible_weights[k];
            if (roll < cumulative) {
                chosen = eligible_ids[k];
                break;
            }
        }

        const EventDef* event = &events[chosen];

        if (event->requires_partner) {
            uint32_t partner = find_event_partner(world, i, event);
            if (partner == UINT32_MAX) {
                continue; /* no valid partner this year -- event doesn't fire */
            }

            event_apply(event, pool, i);
            event_apply_partner(event, pool, partner);

            relation_upsert(world->relations, i, partner,
                             (RelationStatus)event->sets_relation_status,
                             event->relation_friendship_delta,
                             event->relation_romance_delta,
                             event->relation_lust_delta);

            log_event(world, i, partner, event->event_id);
        } else {
            event_apply(event, pool, i);
            log_event(world, i, UINT32_MAX, event->event_id);
        }
    }
}

void world_tick_year(WorldState* world, const EventDef* events, uint32_t event_count) {
    world->tick_log_count = 0;

    pass_age_up(world->people);
    pass_resolve_events(world, events, event_count);

    arena_reset(&world->frame_arena);
    world->year++;
}
