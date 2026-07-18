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

    void* scratch = arena_alloc(arena, TICK_SCRATCH_SIZE, 16);
    if (scratch == NULL) {
        return NULL;
    }
    world->frame_arena.base = (uint8_t*)scratch;
    world->frame_arena.size = TICK_SCRATCH_SIZE;
    world->frame_arena.used = 0;

    rng_seed(&world->rng, seed);
    world->year = 0;

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

        event_apply(&events[chosen], pool, i);
    }
}

void world_tick_year(WorldState* world, const EventDef* events, uint32_t event_count) {
    pass_age_up(world->people);
    pass_resolve_events(world, events, event_count);

    arena_reset(&world->frame_arena);
    world->year++;
}
