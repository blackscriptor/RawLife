#!/usr/bin/env bash
# Build script for RawLife using MSYS2 UCRT64's gcc.
# Run this from a UCRT64 terminal (not plain MSYS, not MINGW64) so the
# right headers/libs are on the path.
set -e

mkdir -p build

# --- Step 1: compile the offline event data compiler ---
# Standalone -- depends only on shared headers for the EventDef layout,
# not on any sim implementation code, so nothing else needs linking here.
gcc -std=c11 -Wall -Wextra -Werror -O2 \
    src/tools/event_compiler.c \
    -I src \
    -o build/event_compiler.exe

# --- Step 2: compile event content data -> binary ---
# This is the "automatic pre-build step" from the design doc: edit
# src/data/events.def, rebuild, run -- no manual tool invocation needed.
./build/event_compiler.exe src/data/events.def build/events.bin

# --- Step 3: compile the game itself ---
gcc -std=c11 -Wall -Wextra -Werror -O2 \
    -municode \
    src/platform/win32_main.c \
    src/sim/arena.c \
    src/sim/person.c \
    src/sim/event.c \
    src/sim/rng.c \
    src/sim/relation.c \
    src/sim/world.c \
    src/save/save.c \
    src/render/renderer.c \
    -I src \
    -o build/rawlife.exe \
    -luser32 -lgdi32 -ld2d1 -ldwrite -lole32 -luuid

echo "Build succeeded: build/rawlife.exe (events compiled from src/data/events.def)"
