# RawLife

A hyper-optimized, data-driven life simulation game for Windows, in the spirit of
BitLife — but deeper. NPCs are simulated with hidden trait vectors (loyalty,
charisma, strength, beauty, libido, criminal tendencies, and more), and every
life event, for the player and every NPC, is resolved against those traits
rather than scripted outcomes. A loyal NPC won't cheat. A disloyal one might.
A low-empathy NPC with the right traits might turn to violence. The system is
generic enough that new categories of event — romantic, criminal, social,
whatever — are added entirely through data, not code changes.

Mature content (sexual content, graphic violence, crime) is structurally
gated by age through the same eligibility system that governs every other
event — see `docs/lifesim_design_doc.md` for details. No such content is
reachable for any character under 18, by construction of the data schema,
not by scattered runtime checks.

## Design Philosophy

Built the way RollerCoaster Tycoon was built: fixed memory footprint, no
runtime heap allocation, struct-of-arrays data layout, flat linear passes
over the simulation state instead of per-entity dispatch. See
`docs/lifesim_design_doc.md` for the full technical design.

## Status

Early scaffold. Currently: a native Win32 window, the fixed-arena
allocator, the Person pool, the trait/event schema, a yearly simulation
tick loop, multi-person event resolution, and a **multi-axis relationship
graph**: family category (permanent) is independent from relationship
status (mutable -- friend, best friend, dating, exclusive, fiance,
spouse, FWB, affair, ex...), which is independent from three separate
0-255 scalars (friendship, romance, lust). Events can search for a
partner fresh from the population OR among a person's existing
relationships of a given status, and can transition that status/those
scalars as a consequence. Event content is now authored in
src/data/events.def (a small DSL) and compiled to build/events.bin by
src/tools/event_compiler as part of every build -- no more editing
hardcoded C arrays to add content. Save/load is a flat binary dump/load of
the person and relation pools plus RNG state and world year (see
docs/lifesim_design_doc.md section 8). See sections 13-16 for how
jobs/economy/skills/kink content extend this without engine changes.
The game now has a real window: Direct2D/DirectWrite render the current
world state as text, and spacebar advances the simulation by a year live
(src/render/renderer.h/c -- deliberately minimal so far, clear + text +
filled rectangles, no layout system or interaction beyond one key yet).
Each tick also records which events actually fired (WorldState.tick_log)
so the window shows what just happened, not just updated numbers. The
window is now player-centric: you're born at age 0 as a specific person
(with two parents who exist in the world but aren't the focus) rather
than watching a symmetric cast of NPCs -- no character creation or
player choices yet (events still resolve automatically for the player
exactly like any NPC), both natural next steps now that this foundation
is in.

## Building

Requires [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain
(`pacman -S mingw-w64-ucrt-x86_64-gcc` if you don't already have it).

From a **UCRT64** terminal (not plain MSYS, not MINGW64 -- check your
prompt/terminal profile):

```
./build.sh
```

This produces `build/rawlife.exe`. Runs as a normal native Windows exe --
you can launch it from Explorer or a regular cmd/PowerShell window too.

## Project Layout

```
src/
  platform/   -- Win32 window, input, timing (OS-facing code only)
  sim/        -- pure simulation: arena allocator, person pools, events,
                 relationships. No rendering or platform calls -- fully
                 unit-testable in isolation.
  render/     -- Direct2D/DirectWrite drawing
  save/       -- binary save/load
  tools/      -- offline data compiler (event/trait DSL -> binary)
  data/       -- authored data files (traits.def, events.def)
docs/
  lifesim_design_doc.md -- full technical design document
```

## Roadmap

- [x] Repo scaffold, Win32 window, arena allocator
- [x] Person pool (struct-of-arrays, hot/cold split, free-list recycling)
- [x] Trait table + event schema
- [x] Simulation tick loop (age up, weighted event resolution)
- [x] Relationship graph (family/social edges between NPCs)
- [x] Multi-person events (partner search, dual consequences, relation edges)
- [x] Multi-axis relationships (family category / mutable status / friendship-romance-lust scalars, existing-relationship-based partner search)
- [x] Event compiler tool (DSL -> events.bin, replaces hardcoded g_example_events)
- [x] Save/load
- [x] Direct2D rendering + basic UI (text-only, spacebar advances a year)
- [x] Player-centric model (one person born at age 0, two parents, distinguished in UI)
- [ ] First playable content pass
  - [x] Character creation screen (first name entry via keyboard, ENTER to begin) --
        last name is inherited from parents (both parents share one, fixed for now),
        not typed. See docs/lifesim_design_doc.md section 12 for the deferred
        marriage-driven surname change system this sets up for.
  - [x] Player choices during events -- the event fires on its own (same weighted
        selection as NPCs use), and if it has choices, the player picks the REACTION
        (e.g. vaccination happens; stay calm / cry / bite the nurse), not which event
        occurs. NPCs auto-pick a reaction via each choice's own weight.
  - [ ] Expanded events.def content
  - [ ] UI polish (currently plain text lines, no layout system)
- [ ] Countries/cities (regional event weighting + name pools) -- design documented in
      docs/lifesim_design_doc.md section 17, not yet implemented
