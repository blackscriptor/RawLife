#ifndef RAWLIFE_ARENA_H
#define RAWLIFE_ARENA_H

#include <stddef.h>
#include <stdint.h>

/*
 * Fixed-block arena allocator.
 *
 * All simulation memory is carved from one large block reserved once at
 * startup (see arena_create, which uses VirtualAlloc on Windows -- this
 * bypasses the CRT heap entirely). No malloc/free happens during gameplay.
 *
 * Two usage patterns:
 *   - Persistent sub-allocations (person pools, relationship pool, event
 *     table) are carved out once at world-init time via arena_alloc and
 *     never freed individually.
 *   - Scratch/per-tick allocations (e.g. building the eligible-events list
 *     for a simulation tick) use arena_reset to reclaim the whole block at
 *     once at the end of the tick, instead of freeing piece by piece.
 */

typedef struct {
    uint8_t* base;
    size_t   size;   /* total capacity in bytes */
    size_t   used;   /* bytes currently allocated */
} Arena;

/* Reserves `size` bytes of memory and returns an arena over it.
 * Returns a zeroed Arena (base == NULL) on failure. */
Arena arena_create(size_t size);

/* Releases the memory backing the arena. Do not use the arena after this. */
void arena_destroy(Arena* arena);

/* Allocates `size` bytes with the given alignment from the arena.
 * Returns NULL if the arena doesn't have enough remaining space.
 * There is no arena_free for individual allocations -- see arena_reset. */
void* arena_alloc(Arena* arena, size_t size, size_t alignment);

/* Reclaims all memory in the arena in one step (used == 0). Intended for
 * scratch arenas that are fully rebuilt every simulation tick. */
void arena_reset(Arena* arena);

#endif /* RAWLIFE_ARENA_H */
