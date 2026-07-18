#ifndef RAWLIFE_SAVE_H
#define RAWLIFE_SAVE_H

#include <stdbool.h>

#include "sim/world.h"

/*
 * Saves world state to a flat binary file at `path`: a small versioned
 * header (magic, version, RNG state, year) followed by direct dumps of
 * the PersonPool and RelationPool structs. No serialization framework --
 * the in-memory layout is the save layout, per docs/lifesim_design_doc.md
 * section 8.
 *
 * Dumps the full fixed-size pools (MAX_PEOPLE slots) rather than only
 * the active_count in use -- simpler (no free-list reconstruction needed
 * on load) at the cost of some wasted space for a mostly-empty world.
 * Worth revisiting if save file size ever becomes a real concern.
 *
 * Returns false on any I/O failure.
 */
bool world_save(const WorldState* world, const char* path);

/*
 * Loads a save file into an already-created WorldState (its PersonPool
 * and RelationPool must already be allocated via world_create -- this
 * overwrites their contents in place, it does not allocate new ones).
 *
 * Returns false if the file can't be opened, the header's magic or
 * version doesn't match (no migration logic exists yet -- versions must
 * match exactly), or a read fails partway through. On failure, treat
 * `world`'s contents as no longer trustworthy.
 */
bool world_load(WorldState* world, const char* path);

#endif /* RAWLIFE_SAVE_H */
