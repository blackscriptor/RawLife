#!/usr/bin/env bash
# Build script for RawLife using MSYS2 UCRT64's gcc.
# Run this from a UCRT64 terminal (not plain MSYS, not MINGW64) so the
# right headers/libs are on the path.
set -e

mkdir -p build

gcc -std=c11 -Wall -Wextra -Werror -O2 \
    src/platform/win32_main.c \
    src/sim/arena.c \
    -I src \
    -o build/rawlife.exe \
    -luser32 -lgdi32

echo "Build succeeded: build/rawlife.exe"
