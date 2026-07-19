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
#include "save/save.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

static const char* status_name(uint8_t status) {
    switch (status) {
        case REL_STATUS_ACQUAINTANCE: return "acquaintance";
        case REL_STATUS_FRIEND:       return "friend";
        case REL_STATUS_BEST_FRIEND:  return "best friend";
        case REL_STATUS_RIVAL:        return "rival";
        case REL_STATUS_ENEMY:        return "enemy";
        case REL_STATUS_DATING:       return "dating";
        case REL_STATUS_EXCLUSIVE:    return "exclusive (gf/bf)";
        case REL_STATUS_FIANCE:       return "fiance";
        case REL_STATUS_SPOUSE:       return "spouse";
        case REL_STATUS_FWB:          return "FWB";
        case REL_STATUS_AFFAIR:       return "affair";
        case REL_STATUS_EX:           return "ex";
        default:                      return "(no status)";
    }
}

static const char* category_name(uint8_t category) {
    switch (category) {
        case REL_CATEGORY_FAMILY_PARENT_CHILD: return "family:parent/child";
        case REL_CATEGORY_FAMILY_SIBLING:       return "family:sibling";
        case REL_CATEGORY_FAMILY_EXTENDED:      return "family:extended";
        default:                                return NULL; /* not family -- don't print a category */
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
        const char* cat = category_name(edge->category);

        char cat_suffix[48];
        if (cat != NULL) {
            snprintf(cat_suffix, sizeof(cat_suffix), " [%s]", cat);
        } else {
            cat_suffix[0] = '\0';
        }

        printf("      -> %s%s with %-6s  friendship=%3u romance=%3u lust=%3u\n",
               status_name(edge->status),
               cat_suffix,
               pool->cold.name[edge->other_id],
               edge->friendship, edge->romance, edge->lust);
    }
}

/* Sanity check for the full simulation stack so far: arena, person pool,
 * event schema, multi-axis relationship graph, and the yearly tick loop.
 * Builds a small family/friend network -- deliberately including two
 * different "friend" pairs with very different relationship shapes, to
 * demonstrate that friendship/romance/lust are independent axes rather
 * than one collapsed "relationship strength" number. Removed once real
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
    uint32_t grace = person_spawn(world->people, "Grace", SEX_FEMALE, 2001);
    uint32_t mary  = person_spawn(world->people, "Mary", SEX_FEMALE, 1975); /* Dave's mother */
    uint32_t holly = person_spawn(world->people, "Holly", SEX_FEMALE, 2000); /* Frank's close-but-platonic friend */

    world->people->hot.age[dave] = 16;
    world->people->hot.age[erin] = 16;
    world->people->hot.age[frank] = 24;
    world->people->hot.age[grace] = 23;
    world->people->hot.age[mary] = 49;
    world->people->hot.age[holly] = 24;

    world->people->hot.trait_flags[frank] |= TRAIT_FLAG_MARRIED;
    world->people->hot.trait_flags[grace] |= TRAIT_FLAG_MARRIED;

    /* Family tie -- permanent, independent of any status. */
    relation_add(world->relations, dave, mary, REL_CATEGORY_FAMILY_PARENT_CHILD, REL_STATUS_NONE, 0, 0, 0);

    /* Two very different "friend" relationships, to prove the axes are
     * independent rather than one collapsed number: */
    relation_add(world->relations, dave, erin, REL_CATEGORY_NONE, REL_STATUS_BEST_FRIEND, 220, 5, 0);
    relation_add(world->relations, frank, holly, REL_CATEGORY_NONE, REL_STATUS_FRIEND, 140, 10, 60);

    relation_add(world->relations, frank, grace, REL_CATEGORY_NONE, REL_STATUS_SPOUSE, 180, 200, 150);

    /* Load the compiled event table (src/data/events.def -> build/events.bin
     * via src/tools/event_compiler) instead of using the hardcoded
     * g_example_events -- this is what actually drives the game now.
     * Falls back to g_example_events if the compiled file is missing, so
     * the smoke test still runs even before you've built events.bin. */
    const EventDef* event_table = g_example_events;
    uint32_t event_table_count = g_example_event_count;
    if (event_table_load(&arena, "build/events.bin", &event_table, &event_table_count)) {
        printf("RawLife: loaded %u event(s) from build/events.bin\n", event_table_count);
    } else {
        printf("RawLife: could not load build/events.bin, falling back to g_example_events "
               "(%u events)\n", event_table_count);
    }

    printf("RawLife: -- year 0 --\n");
    print_person(world, dave);
    print_person(world, erin);
    print_person(world, frank);
    print_person(world, grace);
    print_person(world, mary);
    print_person(world, holly);

    for (uint32_t y = 1; y <= 20; y++) {
        world_tick_year(world, event_table, event_table_count);
        printf("RawLife: -- year %u --\n", world->year);
        print_person(world, dave);
        print_person(world, erin);
        print_person(world, frank);
        print_person(world, grace);
        print_person(world, mary);
        print_person(world, holly);
    }

    printf("RawLife: -- outcome check across 20 years --\n");
    uint32_t cast[] = { dave, erin, frank, grace, mary, holly };
    for (uint32_t p = 0; p < 6; p++) {
        uint32_t id = cast[p];
        uint8_t edge_count = world->relations->edge_count[id];
        for (uint8_t e = 0; e < edge_count; e++) {
            const RelationEdge* edge = &world->relations->edges[id][e];
            if (edge->status == REL_STATUS_AFFAIR || edge->status == REL_STATUS_FWB) {
                printf("  %s -> %s status changed to %s\n",
                       world->people->cold.name[id], world->people->cold.name[edge->other_id],
                       status_name(edge->status));
            }
        }
    }

    /* Save/load round trip: write world to disk, load it into a completely
     * separate WorldState (its own arena, its own pools), and confirm the
     * loaded copy matches -- proves the save format actually round-trips
     * rather than just not crashing. */
    const char* save_path = "rawlife_smoketest.sav";
    if (!world_save(world, save_path)) {
        printf("RawLife: world_save failed\n");
    } else {
        printf("RawLife: saved to %s\n", save_path);

        Arena load_arena = arena_create(4 * 1024 * 1024);
        WorldState* loaded = (load_arena.base != NULL) ? world_create(&load_arena, 0) : NULL;

        if (loaded == NULL || !world_load(loaded, save_path)) {
            printf("RawLife: world_load failed\n");
        } else {
            printf("RawLife: -- loaded copy, year %u (expected %u) --\n", loaded->year, world->year);
            print_person(loaded, dave);
            print_person(loaded, frank);

            bool match = (loaded->year == world->year) &&
                         (loaded->people->hot.age[dave] == world->people->hot.age[dave]) &&
                         (loaded->people->warm.loyalty[frank] == world->people->warm.loyalty[frank]) &&
                         (loaded->relations->edge_count[dave] == world->relations->edge_count[dave]);
            printf("RawLife: save/load round trip %s\n", match ? "OK" : "MISMATCH");
        }

        arena_destroy(&load_arena);
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
