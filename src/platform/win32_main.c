/*
 * Native Win32 entry point and message loop.
 *
 * This is intentionally minimal for now: it opens a window and pumps
 * messages. Rendering (Direct2D) is not wired in yet -- this step is the
 * platform shell + a smoke test that runs the world simulation forward
 * several years with a small family/friend/spouse network, so we have a
 * real buildable baseline before layering in rendering.
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
#include "sim/relation.h"
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

static const char* relation_type_name(uint8_t type) {
    switch (type) {
        case RELATION_FAMILY_PARENT_CHILD: return "parent/child";
        case RELATION_FAMILY_SIBLING:      return "sibling";
        case RELATION_SPOUSE:              return "spouse";
        case RELATION_FRIEND:              return "friend";
        case RELATION_RIVAL:               return "rival";
        case RELATION_AFFAIR:              return "affair";
        default:                           return "none";
    }
}

static void print_person(const WorldState* world, uint32_t id) {
    const PersonPool* pool = world->people;
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

    uint8_t edge_count = world->relations->edge_count[id];
    for (uint8_t i = 0; i < edge_count; i++) {
        const RelationEdge* edge = &world->relations->edges[id][i];
        printf("      -> %s with %s (strength=%u)\n",
               relation_type_name(edge->type),
               pool->cold.name[edge->other_id],
               edge->strength);
    }
}

/* Sanity check for the full simulation stack so far: arena, person pool,
 * event schema, relationship graph, and the yearly tick loop. Builds a
 * small family/friend network and runs it forward several years.
 * Removed once real unit tests and real UI exist. */
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
    uint32_t grace = person_spawn(world->people, "Grace", SEX_FEMALE, 2001);
    uint32_t mary  = person_spawn(world->people, "Mary", SEX_FEMALE, 1975); /* Dave's mother */

    world->people->hot.age[dave] = 16;
    world->people->hot.age[erin] = 16;
    world->people->hot.age[frank] = 24;
    world->people->hot.age[grace] = 23;
    world->people->hot.age[mary] = 49;

    world->people->hot.trait_flags[frank] |= TRAIT_FLAG_MARRIED;
    world->people->hot.trait_flags[grace] |= TRAIT_FLAG_MARRIED;

    relation_add(world->relations, dave, mary, RELATION_FAMILY_PARENT_CHILD, 200);
    relation_add(world->relations, dave, erin, RELATION_FRIEND, 150);
    relation_add(world->relations, frank, grace, RELATION_SPOUSE, 180);

    printf("RawLife: -- year 0 --\n");
    print_person(world, dave);
    print_person(world, erin);
    print_person(world, frank);
    print_person(world, grace);
    print_person(world, mary);

    for (uint32_t y = 1; y <= 10; y++) {
        world_tick_year(world, g_example_events, g_example_event_count);
        printf("RawLife: -- year %u --\n", world->year);
        print_person(world, dave);
        print_person(world, erin);
        print_person(world, frank);
        print_person(world, grace);
        print_person(world, mary);
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
