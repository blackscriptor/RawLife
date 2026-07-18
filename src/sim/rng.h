#ifndef RAWLIFE_RNG_H
#define RAWLIFE_RNG_H

#include <stdint.h>

/*
 * xorshift128+ -- a few instructions per call, no allocation, trivially
 * seedable. Used instead of the CRT rand() (slow, poor quality, global
 * state). The world's RNG state is seeded once per save and persists in
 * the save file, so a save's future is fully reproducible for a given
 * sequence of player choices -- useful for debugging "why did this NPC
 * do that."
 */
typedef struct {
    uint64_t s[2];
} Rng;

/* Seeds the generator from a single 64-bit seed (internally expanded via
 * splitmix64 so the two state words aren't correlated). */
void rng_seed(Rng* r, uint64_t seed);

/* Returns the next pseudo-random 64-bit value. */
uint64_t rng_next(Rng* r);

#endif /* RAWLIFE_RNG_H */
