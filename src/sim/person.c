#include "sim/person.h"

#include <stdio.h>
#include <string.h>

static void set_alive_bit(PersonHot* hot, uint32_t id, bool alive) {
    uint32_t byte = id / 8;
    uint8_t  mask = (uint8_t)(1u << (id % 8));
    if (alive) {
        hot->alive_bits[byte] |= mask;
    } else {
        hot->alive_bits[byte] &= (uint8_t)~mask;
    }
}

PersonPool* person_pool_create(Arena* arena) {
    PersonPool* pool = (PersonPool*)arena_alloc(arena, sizeof(PersonPool), _Alignof(PersonPool));
    if (pool == NULL) {
        return NULL;
    }

    memset(pool, 0, sizeof(*pool));
    pool->active_count = 0;
    pool->free_count = 0;
    return pool;
}

bool person_is_alive(const PersonPool* pool, uint32_t id) {
    if (id >= MAX_PEOPLE) {
        return false;
    }
    uint32_t byte = id / 8;
    uint8_t  mask = (uint8_t)(1u << (id % 8));
    return (pool->hot.alive_bits[byte] & mask) != 0;
}

uint32_t person_spawn(PersonPool* pool, const char* first_name, const char* last_name,
                       uint8_t sex, uint32_t birth_year) {
    uint32_t id;

    if (pool->free_count > 0) {
        pool->free_count -= 1;
        id = pool->free_list[pool->free_count];
    } else if (pool->active_count < MAX_PEOPLE) {
        id = pool->active_count;
        pool->active_count += 1;
    } else {
        return UINT32_MAX; /* pool exhausted */
    }

    pool->hot.age[id] = 0;
    pool->hot.sex[id] = sex;
    pool->hot.trait_flags[id] = 0;
    set_alive_bit(&pool->hot, id, true);

    pool->warm.loyalty[id] = 128;      /* neutral starting values -- */
    pool->warm.charisma[id] = 128;     /* tuning belongs in data once */
    pool->warm.strength[id] = 128;     /* the trait/event system lands */
    pool->warm.beauty[id] = 128;
    pool->warm.intelligence[id] = 128;
    pool->warm.libido[id] = 128;
    pool->warm.kink_mask[id] = 0;

    if (first_name != NULL) {
        strncpy(pool->cold.first_name[id], first_name, FIRST_NAME_MAX_LEN - 1);
        pool->cold.first_name[id][FIRST_NAME_MAX_LEN - 1] = '\0';
    } else {
        pool->cold.first_name[id][0] = '\0';
    }
    if (last_name != NULL) {
        strncpy(pool->cold.last_name[id], last_name, LAST_NAME_MAX_LEN - 1);
        pool->cold.last_name[id][LAST_NAME_MAX_LEN - 1] = '\0';
    } else {
        pool->cold.last_name[id][0] = '\0';
    }
    pool->cold.family_id[id] = 0;
    pool->cold.birth_year[id] = birth_year;

    return id;
}

void person_get_full_name(const PersonPool* pool, uint32_t id, char* out, size_t out_size) {
    snprintf(out, out_size, "%s %s", pool->cold.first_name[id], pool->cold.last_name[id]);
}

void person_kill(PersonPool* pool, uint32_t id) {
    if (id >= MAX_PEOPLE || !person_is_alive(pool, id)) {
        return;
    }

    set_alive_bit(&pool->hot, id, false);
    pool->free_list[pool->free_count] = id;
    pool->free_count += 1;
}
