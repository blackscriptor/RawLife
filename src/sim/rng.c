#include "sim/rng.h"

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9E3779B97f4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void rng_seed(Rng* r, uint64_t seed) {
    uint64_t sm = seed;
    r->s[0] = splitmix64(&sm);
    r->s[1] = splitmix64(&sm);
    if (r->s[0] == 0 && r->s[1] == 0) {
        r->s[0] = 1; /* xorshift128+ requires non-zero state */
    }
}

uint64_t rng_next(Rng* r) {
    uint64_t x = r->s[0];
    const uint64_t y = r->s[1];
    r->s[0] = y;
    x ^= x << 23;
    x ^= x >> 17;
    x ^= y ^ (y >> 26);
    r->s[1] = x;
    return x + y;
}
