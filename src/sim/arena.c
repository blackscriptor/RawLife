#include "sim/arena.h"

#include <windows.h>
#include <string.h>

static size_t align_up(size_t value, size_t alignment) {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

Arena arena_create(size_t size) {
    Arena arena;
    memset(&arena, 0, sizeof(arena));

    void* mem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem == NULL) {
        return arena; /* zeroed -- base == NULL signals failure */
    }

    arena.base = (uint8_t*)mem;
    arena.size = size;
    arena.used = 0;
    return arena;
}

void arena_destroy(Arena* arena) {
    if (arena->base != NULL) {
        VirtualFree(arena->base, 0, MEM_RELEASE);
    }
    memset(arena, 0, sizeof(*arena));
}

void* arena_alloc(Arena* arena, size_t size, size_t alignment) {
    size_t current = (size_t)(arena->base + arena->used);
    size_t aligned = align_up(current, alignment);
    size_t padding = aligned - current;

    if (arena->used + padding + size > arena->size) {
        return NULL; /* out of space -- fixed capacity by design */
    }

    arena->used += padding;
    void* result = arena->base + arena->used;
    arena->used += size;
    return result;
}

void arena_reset(Arena* arena) {
    arena->used = 0;
}
