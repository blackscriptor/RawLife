# Life Simulation Game — Technical Design Document

**Codename:** (TBD)
**Platform:** Windows (native, Win32 + Direct2D)
**Language:** C
**Design philosophy:** RollerCoaster Tycoon-era discipline — fixed memory, no runtime allocation, data-oriented layout, zero unnecessary indirection.

---

## 1. Design Goals

1. Simulate a small town (low thousands of NPCs) across full lifetimes, with yearly ticks, at effectively unmeasurable cost per tick.
2. Fully data-driven trait & event system — new content added via data files, not code changes.
3. Deterministic simulation (seeded RNG) for debugging, tuning, and reproducibility.
4. Zero heap allocation during simulation — everything preallocated in fixed arenas.
5. Content maturity gating (18+) enforced structurally through the data schema, not scattered runtime checks.

---

## 2. Memory Model

No `malloc`/`free` during gameplay. One large arena is reserved at startup via `VirtualAlloc` (bypasses the CRT heap entirely) and carved into fixed-capacity sub-pools.

```c
#define MAX_PEOPLE               8192
#define MAX_RELATIONS_PER_PERSON 32
#define MAX_ACTIVE_EVENTS        4096
#define TRAIT_COUNT               24
#define KINK_COUNT                16

typedef struct {
    uint8_t* base;
    size_t   size;
    size_t   used;   // frame_arena only — reset every tick
} Arena;

typedef struct {
    PersonHot   hot;
    PersonWarm  warm;
    PersonCold  cold;
    RelationPool relations;
    EventQueue   events;
    Arena        frame_arena;   // scratch memory, reset every simulation tick
    uint32_t     active_count;
    uint32_t     free_list[MAX_PEOPLE]; // recycled dead slots
    uint32_t     free_count;
} WorldState;
```

Dead NPCs' slots go onto `free_list` and get recycled for newborns — the arrays never shrink, grow, or fragment. `active_count` is a high-water mark, not a live count; `alive[i]` bits are checked during iteration.

---

## 3. Person Data — Struct of Arrays, Hot/Cold Split

Fields are grouped by **access frequency during simulation**, not by logical category. This is the single biggest performance lever in the whole design — it determines what fits in L1/L2 cache during a full-population pass.

### 3.1 Hot data (touched every tick, every NPC)

```c
typedef struct {
    uint8_t  age[MAX_PEOPLE];
    uint8_t  sex[MAX_PEOPLE];
    uint8_t  alive_bits[MAX_PEOPLE / 8]; // packed bitset
    uint32_t trait_flags[MAX_PEOPLE];    // boolean traits, one bit each
} PersonHot;
```

### 3.2 Warm data (touched during event resolution, not every tick)

```c
typedef struct {
    uint8_t  loyalty[MAX_PEOPLE];    // 0-255 fixed point
    uint8_t  charisma[MAX_PEOPLE];
    uint8_t  strength[MAX_PEOPLE];
    uint8_t  beauty[MAX_PEOPLE];
    uint8_t  intelligence[MAX_PEOPLE];
    uint8_t  libido[MAX_PEOPLE];
    uint16_t kink_mask[MAX_PEOPLE];  // bitmask into kink ID table (see 5.3)
} PersonWarm;
```

All traits are `uint8_t` fixed-point (0–255) rather than `float`. This halves memory footprint versus float, avoids FPU round-trips, and turns trait comparisons into plain integer ops (`loyalty[i] > 180`).

### 3.3 Cold data (rarely touched)

```c
typedef struct {
    char     name[MAX_PEOPLE][32];
    uint32_t family_id[MAX_PEOPLE];
    uint32_t birth_year[MAX_PEOPLE];
    uint32_t backstory_flags[MAX_PEOPLE];
} PersonCold;
```

Rule of thumb: if a field is read during the per-NPC yearly loop for *every* NPC, it belongs in Hot. If it's only read when an event actually fires for that specific NPC, it's Warm. If it's UI/flavor-only, it's Cold.

---

## 4. Simulation Loop

No virtual dispatch, no function pointers per entity. Each yearly tick is a sequence of flat linear passes over the arrays — predictable branches, prefetcher-friendly, auto-vectorizable by the compiler.

```
tick_year(world):
    pass_age_up(world)                    // age[i]++ for all alive
    pass_mortality_check(world)           // age/health-based death rolls
    pass_gather_eligible_events(world)    // bitmask filter -> frame_arena list
    pass_resolve_events(world)            // weighted roll + apply trait deltas
    pass_relationship_decay(world)
    pass_births(world)                    // new NPCs from eligible pairs
    pass_compact_dead(world)              // return dead slots to free_list
    frame_arena_reset(world)
```

Each pass is a tight `for (i = 0; i < active_count; i++) { if (!alive(i)) continue; ... }`. No allocation happens inside any pass — the eligible-events list for the whole population is built into the pre-sized `frame_arena` and discarded at the end of the tick.

---

## 5. Trait & Event System

### 5.1 Trait table (data-driven)

Traits aren't hardcoded struct fields conceptually — they're defined in a data file (`traits.def`) with an ID, display name, and default value, and the compiler tool generates the `PersonWarm` layout from this definition. Adding a trait means adding a line to the data file and recompiling the schema, not restructuring code by hand.

### 5.2 Event definitions

Authored in a simple human-readable DSL at content-creation time, compiled to a flat binary blob (`events.bin`) that's `mmap`'d directly at game startup — zero parse cost at runtime.

```c
typedef struct {
    uint16_t event_id;
    uint8_t  min_age;
    uint8_t  max_age;
    uint32_t required_trait_mask;   // traits that must be present to be eligible
    uint32_t forbidden_trait_mask;  // traits that exclude this event
    uint16_t weight_base;
    int8_t   trait_deltas[TRAIT_COUNT];
    uint16_t flag_set;
    uint16_t flag_clear;
} EventDef;

static const EventDef g_event_table[EVENT_COUNT]; // loaded via file-mapped binary
```

Eligibility filtering is pure bitmask AND/OR — no string comparison, no branching per trait:

```c
bool event_eligible(const EventDef* e, uint8_t age, uint32_t traits) {
    if (age < e->min_age || age > e->max_age) return false;
    if ((traits & e->required_trait_mask) != e->required_trait_mask) return false;
    if (traits & e->forbidden_trait_mask) return false;
    return true;
}
```

Example of how NPC behavior falls naturally out of this: a "loyal" trait flag included in an event's `forbidden_trait_mask` for infidelity-type events means high-loyalty NPCs are structurally excluded from that event pool — no special-cased `if (npc.isLoyal)` logic scattered through code, it's just data the filter already respects.

### 5.3 Kink/fetish system

Modeled the same way as any other trait: a bitmask (`kink_mask`) against a data-defined kink ID table, feeding the same eligibility filter as every other event. No separate subsystem — it's the same generic mechanism as loyalty, charisma, etc., just another set of bits.

### 5.4 Age-gating (structural, not scattered checks)

Every `EventDef` carries `min_age`. Content intended only for adult characters is simply authored with `min_age = 18` in the data file. Because the eligibility filter (`event_eligible`) is the *only* path by which an event can ever be selected, age-gating is enforced at a single choke point rather than needing per-feature guards throughout the codebase. This also means it's easy to audit — one pass over `events.bin` can verify every mature-tagged event has `min_age >= 18` before shipping.

---

## 6. Relationship Graph

Sparse adjacency, not a full NxN matrix (unnecessary at this scale and wasteful in memory):

```c
typedef struct {
    uint32_t person_a;
    uint32_t person_b;
    uint8_t  relation_type;   // enum: family, friend, partner, etc.
    uint8_t  strength;        // 0-255
    uint16_t flags;
} RelationEdge;

typedef struct {
    RelationEdge edges[MAX_PEOPLE * MAX_RELATIONS_PER_PERSON];
    uint32_t     edge_count;
    uint32_t     person_edge_start[MAX_PEOPLE]; // index into edges, CSR-style
    uint32_t     person_edge_count[MAX_PEOPLE];
} RelationPool;
```

CSR (compressed sparse row) layout keeps per-person relationship lookups a contiguous slice rather than a pointer-chased linked list.

---

## 7. RNG

CRT `rand()` is avoided (slow, poor quality, global state). Use **xorshift128+** or **PCG32** — a few instructions per call, no allocation, trivially seedable.

```c
typedef struct { uint64_t s[2]; } Rng;
uint64_t rng_next(Rng* r);       // xorshift128+ step
void     rng_seed(Rng* r, uint64_t seed);
```

The world's RNG is seeded once per save and its state persists in the save file, so a given save's future is fully reproducible for a given sequence of player choices — invaluable for debugging "why did this NPC do that."

---

## 8. Save Format

Flat binary, versioned header, direct array dumps — no serialization framework, since the in-memory layout *is* the save layout.

```c
typedef struct {
    uint32_t magic;        // e.g. 'LSIM'
    uint32_t version;
    uint32_t person_count;
    uint64_t rng_state[2];
    uint32_t world_year;
} SaveHeader;

// Save = SaveHeader, then raw fwrite of:
//   PersonHot, PersonWarm, PersonCold arrays (up to active_count)
//   RelationPool edges (up to edge_count)
//   event log (for undo/history features, optional)
```

`version` allows future migrations (e.g., a new trait added later) via a small upgrade path rather than breaking old saves.

---

## 9. Rendering — Win32 + Direct2D

Given the performance priority and Windows-only target: raw **Win32** window/message loop with **Direct2D** for drawing. This gets hardware-accelerated 2D rendering with essentially zero framework overhead — no engine abstraction layers between your draw calls and the GPU.

- `ID2D1HwndRenderTarget` bound directly to the window.
- UI is custom-built (buttons, lists, stat bars) — not using a retained-mode toolkit, since BitLife-style UIs are simple enough that immediate-mode-style custom drawing is both faster and more controllable.
- Text rendering via DirectWrite (pairs naturally with Direct2D).
- All UI layout data-driven where sensible (panel positions, colors) to avoid recompiling for visual tweaks.

This is more code upfront than raylib/ImGui, but avoids any framework tax and matches the "optimize everything" goal for the whole project, not just the simulation core.

---

## 10. Build Pipeline

An offline **event/data compiler** is a separate small C program that:
1. Parses the human-readable event/trait DSL files.
2. Validates them (e.g., asserts all mature-tagged events have `min_age >= 18`, checks trait IDs resolve, checks for weight sanity).
3. Emits `events.bin` / `traits.bin`.

This runs as an **automatic pre-build step** (invoked from your build script/Makefile before compiling `main.c`), so the workflow is just: edit data file → build → run. No manual tool invocation needed, and the validation step catches authoring mistakes (like a missing age gate) at build time rather than at runtime.

```
/tools/
  event_compiler.c      -- DSL -> events.bin, with validation
/sim/                    -- pure simulation, no I/O or rendering, unit-testable
  person.c/h
  events.c/h
  relationships.c/h
  rng.c/h
  arena.c/h
/platform/               -- Win32 window, input, timing
  win32_main.c
/render/                 -- Direct2D drawing, DirectWrite text
  renderer.c/h
  ui.c/h
/save/
  save.c/h
/data/                   -- authored DSL source files (not compiled into binary)
  traits.def
  events.def
main.c
```

Keeping `/sim` free of any rendering or platform calls means the simulation can be benchmarked and fuzz-tested in total isolation — e.g., a standalone test harness running 50 years across 8192 NPCs in a loop, profiled with zero UI overhead, to validate the "hyper optimized" goal directly against numbers rather than assumption.

---

## 11. Performance Targets (rough, to validate against once code exists)

- Full-population yearly tick (8192 NPCs): sub-millisecond, single-threaded.
- Save/load: dominated by disk I/O, not CPU — should be near-instant for this scale.
- Steady-state memory footprint: fixed at startup, no growth over a play session (this is a direct consequence of the fixed-arena design, not something that needs separate tuning).

---

## 12. Open Items for Next Phase

- Exact trait list and starting values (full `traits.def` content).
- Exact event catalog — this is really the bulk of "writing the game" at this point; the engine above will run whatever event data it's given.
- UI wireframe / screen flow (life stages, stat screens, relationship views).
- Birth/family-tree mechanics in detail (how partner selection and inheritance of traits works).
- Whether to support user-authored/moddable event packs (the binary-compiled format makes this possible later, but needs a defined external format if so).
