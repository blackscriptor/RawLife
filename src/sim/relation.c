#include "sim/relation.h"

#include <stdint.h>

RelationPool* relation_pool_create(Arena* arena) {
    RelationPool* pool = (RelationPool*)arena_alloc(arena, sizeof(RelationPool), _Alignof(RelationPool));
    if (pool == NULL) {
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_PEOPLE; i++) {
        pool->edge_count[i] = 0;
    }
    return pool;
}

RelationEdge* relation_find_edge(RelationPool* pool, uint32_t person, uint32_t other_id) {
    uint8_t count = pool->edge_count[person];
    for (uint8_t i = 0; i < count; i++) {
        if (pool->edges[person][i].other_id == other_id) {
            return &pool->edges[person][i];
        }
    }
    return NULL;
}

static bool add_one_side(RelationPool* pool, uint32_t person, uint32_t other_id, RelationCategory category,
                          RelationStatus status, uint8_t friendship, uint8_t romance, uint8_t lust) {
    if (pool->edge_count[person] >= MAX_RELATIONS_PER_PERSON) {
        return false; /* this person's relationship list is full */
    }

    RelationEdge* edge = &pool->edges[person][pool->edge_count[person]];
    edge->other_id = other_id;
    edge->category = (uint8_t)category;
    edge->status = (uint8_t)status;
    edge->friendship = friendship;
    edge->romance = romance;
    edge->lust = lust;
    edge->flags = 0;
    pool->edge_count[person]++;
    return true;
}

bool relation_add(RelationPool* pool, uint32_t a, uint32_t b, RelationCategory category,
                   RelationStatus status, uint8_t friendship, uint8_t romance, uint8_t lust) {
    if (relation_find_edge(pool, a, b) != NULL) {
        return false; /* already exists -- use relation_upsert for create-or-update */
    }

    bool ok_a = add_one_side(pool, a, b, category, status, friendship, romance, lust);
    bool ok_b = add_one_side(pool, b, a, category, status, friendship, romance, lust);
    return ok_a && ok_b;
}

static uint8_t clamp_add_i8(uint8_t base, int8_t delta) {
    int32_t v = (int32_t)base + delta;
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

static void upsert_one_side(RelationPool* pool, uint32_t person, uint32_t other_id, RelationStatus new_status,
                             int8_t friendship_delta, int8_t romance_delta, int8_t lust_delta) {
    RelationEdge* edge = relation_find_edge(pool, person, other_id);
    if (edge == NULL) {
        if (!add_one_side(pool, person, other_id, REL_CATEGORY_NONE, REL_STATUS_NONE, 0, 0, 0)) {
            return; /* edge list full -- nothing we can do */
        }
        edge = relation_find_edge(pool, person, other_id);
    }

    if (new_status != REL_STATUS_KEEP) {
        edge->status = (uint8_t)new_status;
    }
    edge->friendship = clamp_add_i8(edge->friendship, friendship_delta);
    edge->romance = clamp_add_i8(edge->romance, romance_delta);
    edge->lust = clamp_add_i8(edge->lust, lust_delta);
}

void relation_upsert(RelationPool* pool, uint32_t a, uint32_t b, RelationStatus new_status,
                      int8_t friendship_delta, int8_t romance_delta, int8_t lust_delta) {
    upsert_one_side(pool, a, b, new_status, friendship_delta, romance_delta, lust_delta);
    upsert_one_side(pool, b, a, new_status, friendship_delta, romance_delta, lust_delta);
}

static void remove_one_side(RelationPool* pool, uint32_t person, uint32_t other_id) {
    uint8_t count = pool->edge_count[person];
    for (uint8_t i = 0; i < count; i++) {
        if (pool->edges[person][i].other_id == other_id) {
            /* swap-remove: overwrite with the last edge, shrink count */
            pool->edges[person][i] = pool->edges[person][count - 1];
            pool->edge_count[person] = (uint8_t)(count - 1);
            return;
        }
    }
}

void relation_remove(RelationPool* pool, uint32_t a, uint32_t b) {
    remove_one_side(pool, a, b);
    remove_one_side(pool, b, a);
}

uint32_t relation_find_first_by_status(const RelationPool* pool, uint32_t person, RelationStatus status) {
    uint8_t count = pool->edge_count[person];
    for (uint8_t i = 0; i < count; i++) {
        if (pool->edges[person][i].status == (uint8_t)status) {
            return pool->edges[person][i].other_id;
        }
    }
    return UINT32_MAX;
}

uint32_t relation_find_first_by_category(const RelationPool* pool, uint32_t person, RelationCategory category) {
    uint8_t count = pool->edge_count[person];
    for (uint8_t i = 0; i < count; i++) {
        if (pool->edges[person][i].category == (uint8_t)category) {
            return pool->edges[person][i].other_id;
        }
    }
    return UINT32_MAX;
}
