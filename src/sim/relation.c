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

static bool add_one_side(RelationPool* pool, uint32_t person, uint32_t other_id, RelationType type, uint8_t strength) {
    if (pool->edge_count[person] >= MAX_RELATIONS_PER_PERSON) {
        return false; /* this person's relationship list is full */
    }

    RelationEdge* edge = &pool->edges[person][pool->edge_count[person]];
    edge->other_id = other_id;
    edge->type = (uint8_t)type;
    edge->strength = strength;
    edge->flags = 0;
    pool->edge_count[person]++;
    return true;
}

bool relation_add(RelationPool* pool, uint32_t a, uint32_t b, RelationType type, uint8_t strength) {
    bool ok_a = add_one_side(pool, a, b, type, strength);
    bool ok_b = add_one_side(pool, b, a, type, strength);
    return ok_a && ok_b;
}

void relation_remove(RelationPool* pool, uint32_t person, uint32_t other_id, RelationType type) {
    uint8_t count = pool->edge_count[person];
    for (uint8_t i = 0; i < count; i++) {
        RelationEdge* edge = &pool->edges[person][i];
        if (edge->other_id == other_id && edge->type == (uint8_t)type) {
            /* swap-remove: overwrite with the last edge, shrink count */
            *edge = pool->edges[person][count - 1];
            pool->edge_count[person] = (uint8_t)(count - 1);
            return;
        }
    }
}

uint32_t relation_find_first(const RelationPool* pool, uint32_t person, RelationType type) {
    uint8_t count = pool->edge_count[person];
    for (uint8_t i = 0; i < count; i++) {
        if (pool->edges[person][i].type == (uint8_t)type) {
            return pool->edges[person][i].other_id;
        }
    }
    return UINT32_MAX;
}
