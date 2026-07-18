#include "save/save.h"

#include <stdio.h>

#define SAVE_MAGIC   0x4C574152u /* 'RAWL' -- little-endian on x86, fine for a Windows-only target */
#define SAVE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t person_count; /* informational snapshot of active_count at save time */
    uint64_t rng_state[2];
    uint32_t world_year;
} SaveHeader;

bool world_save(const WorldState* world, const char* path) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }

    SaveHeader header;
    header.magic = SAVE_MAGIC;
    header.version = SAVE_VERSION;
    header.person_count = world->people->active_count;
    header.rng_state[0] = world->rng.s[0];
    header.rng_state[1] = world->rng.s[1];
    header.world_year = world->year;

    bool ok = fwrite(&header, sizeof(header), 1, f) == 1;
    ok = ok && fwrite(world->people, sizeof(*world->people), 1, f) == 1;
    ok = ok && fwrite(world->relations, sizeof(*world->relations), 1, f) == 1;

    fclose(f);
    return ok;
}

bool world_load(WorldState* world, const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }

    SaveHeader header;
    bool ok = fread(&header, sizeof(header), 1, f) == 1;

    if (ok && header.magic != SAVE_MAGIC) {
        ok = false; /* not a RawLife save file */
    }
    if (ok && header.version != SAVE_VERSION) {
        ok = false; /* no migration path yet -- versions must match exactly */
    }

    ok = ok && fread(world->people, sizeof(*world->people), 1, f) == 1;
    ok = ok && fread(world->relations, sizeof(*world->relations), 1, f) == 1;

    if (ok) {
        world->rng.s[0] = header.rng_state[0];
        world->rng.s[1] = header.rng_state[1];
        world->year = header.world_year;
    }

    fclose(f);
    return ok;
}
