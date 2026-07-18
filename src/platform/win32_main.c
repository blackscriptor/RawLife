/*
 * Native Win32 entry point and message loop.
 *
 * This is intentionally minimal for now: it opens a window and pumps
 * messages. Rendering (Direct2D) is not wired in yet -- this step is the
 * platform shell + a smoke test that runs the world simulation forward
 * several years, so we have a real buildable baseline before layering in
 * rendering.
 *
 * Built as a console-subsystem app (no -mwindows) so smoke test output
 * is visible in a normal terminal. Once real rendering lands, this
 * switches to -mwindows and diagnostics move to a log file instead.
 */

#include <windows.h>
#include <stdio.h>

#include "sim/arena.h"
#include "sim/event.h"
#include "sim/person.h"
#include "sim/traits.h"
#include "sim/world.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

static void print_person(const PersonPool* pool, uint32_t id) {
    uint32_t flags = pool->hot.trait_flags[id];
    printf("  %-6s age=%3u loyalty=%3u  [%s%s%s%s%s]\n",
           pool->cold.name[id],
           pool->hot.age[id],
           pool->warm.loyalty[id],
           (flags & TRAIT_FLAG_LOYAL) ? "LOYAL " : "",
           (flags & TRAIT_FLAG_MARRIED) ? "MARRIED " : "",
           (flags & TRAIT_FLAG_EMPLOYED) ? "EMPLOYED " : "",
           (flags & TRAIT_FLAG_CRIMINAL_RECORD) ? "CRIMINAL " : "",
           (flags & TRAIT_FLAG_WANTED) ? "WANTED " : "");
}

/* Sanity check for the full simulation stack so far: arena, person pool,
 * event schema, and the yearly tick loop. Runs a small cast of NPCs
 * forward several years and prints how they change. Removed once real
 * unit tests and real UI exist. */
static void sim_smoke_test(void) {
    Arena arena = arena_create(4 * 1024 * 1024); /* 4 MB -- plenty for one world */
    if (arena.base == NULL) {
        printf("RawLife: arena_create failed\n");
        return;
    }

    WorldState* world = world_create(&arena, 12345 /* fixed seed -- reproducible run */);
    if (world == NULL) {
        printf("RawLife: world_create failed\n");
        arena_destroy(&arena);
        return;
    }

    uint32_t dave  = person_spawn(world->people, "Dave", SEX_MALE, 2008);
    uint32_t erin  = person_spawn(world->people, "Erin", SEX_FEMALE, 2008);
    uint32_t frank = person_spawn(world->people, "Frank", SEX_MALE, 2000);

    world->people->hot.age[dave] = 16;
    world->people->hot.age[erin] = 16;
    world->people->hot.age[frank] = 24;
    world->people->hot.trait_flags[frank] |= TRAIT_FLAG_MARRIED; /* eligible for the cheating event later */

    printf("RawLife: -- year 0 --\n");
    print_person(world->people, dave);
    print_person(world->people, erin);
    print_person(world->people, frank);

    for (uint32_t y = 1; y <= 10; y++) {
        world_tick_year(world, g_example_events, g_example_event_count);
        printf("RawLife: -- year %u --\n", world->year);
        print_person(world->people, dave);
        print_person(world->people, erin);
        print_person(world->people, frank);
    }

    fflush(stdout);
    arena_destroy(&arena);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    sim_smoke_test();

    const wchar_t CLASS_NAME[] = L"RawLifeWindowClass";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"RawLife",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
