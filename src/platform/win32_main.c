/*
 * Native Win32 entry point and message loop.
 *
 * Built as a console-subsystem app (no -mwindows) for now, specifically
 * so the startup smoke test's printf output stays visible in a normal
 * terminal alongside the window. Once the console output isn't needed
 * for day-to-day debugging anymore, this switches to -mwindows.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "render/renderer.h"
#include "save/save.h"
#include "sim/arena.h"
#include "sim/event.h"
#include "sim/person.h"
#include "sim/relation.h"
#include "sim/traits.h"
#include "sim/world.h"

/* ---------------------------------------------------------------------
 * Startup smoke test -- exercises arena/person/event/relation/save-load
 * end to end via console output. Unchanged in shape from previous steps;
 * kept as a quick regression check that runs once before the window
 * opens. Removed once real automated tests exist.
 * --------------------------------------------------------------------- */

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
        default:                                return NULL;
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

static void sim_smoke_test(void) {
    Arena arena = arena_create(4 * 1024 * 1024);
    if (arena.base == NULL) {
        printf("RawLife: arena_create failed\n");
        return;
    }

    WorldState* world = world_create(&arena, 12345);
    if (world == NULL) {
        printf("RawLife: world_create failed\n");
        arena_destroy(&arena);
        return;
    }

    uint32_t dave  = person_spawn(world->people, "Dave", SEX_MALE, 2008);
    uint32_t erin  = person_spawn(world->people, "Erin", SEX_FEMALE, 2008);
    uint32_t frank = person_spawn(world->people, "Frank", SEX_MALE, 2000);
    uint32_t grace = person_spawn(world->people, "Grace", SEX_FEMALE, 2001);
    uint32_t mary  = person_spawn(world->people, "Mary", SEX_FEMALE, 1975);
    uint32_t holly = person_spawn(world->people, "Holly", SEX_FEMALE, 2000);

    world->people->hot.age[dave] = 16;
    world->people->hot.age[erin] = 16;
    world->people->hot.age[frank] = 24;
    world->people->hot.age[grace] = 23;
    world->people->hot.age[mary] = 49;
    world->people->hot.age[holly] = 24;

    world->people->hot.trait_flags[frank] |= TRAIT_FLAG_MARRIED;
    world->people->hot.trait_flags[grace] |= TRAIT_FLAG_MARRIED;

    relation_add(world->relations, dave, mary, REL_CATEGORY_FAMILY_PARENT_CHILD, REL_STATUS_NONE, 0, 0, 0);
    relation_add(world->relations, dave, erin, REL_CATEGORY_NONE, REL_STATUS_BEST_FRIEND, 220, 5, 0);
    relation_add(world->relations, frank, holly, REL_CATEGORY_NONE, REL_STATUS_FRIEND, 140, 10, 60);
    relation_add(world->relations, frank, grace, REL_CATEGORY_NONE, REL_STATUS_SPOUSE, 180, 200, 150);

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

/* ---------------------------------------------------------------------
 * The actual window: a persistent world, rendered with Direct2D, with
 * spacebar advancing the simulation by one year. First rendering pass --
 * deliberately just text on a cleared background, no layout system or
 * interaction beyond the one key. AppState is a stack local in
 * wWinMain (see below) whose address is stashed in the window's
 * GWLP_USERDATA slot -- safe because wWinMain's frame lives for the
 * entire message loop.
 *
 * This is the first player-centric pass: one person (player_id) is *the*
 * character the game is about, born at age 0 this run, with two parents
 * who exist in the world but aren't the focus. There's now a real
 * character creation screen (name entry) before the world starts --
 * player_id/mother_id/father_id are UINT32_MAX until the name is
 * confirmed and begin_game() spawns everyone.
 * --------------------------------------------------------------------- */

typedef enum {
    SCREEN_CHARACTER_CREATION,
    SCREEN_GAME,
} AppScreen;

#define NAME_BUFFER_MAX 31 /* leaves room for the NUL, matches PersonCold's MAX_NAME_LEN - 1 */

typedef struct {
    Arena arena;
    WorldState* world;
    const EventDef* events;
    uint32_t event_count;
    uint32_t player_id;
    uint32_t mother_id;
    uint32_t father_id;
    Renderer* renderer;
    AppScreen screen;
    wchar_t name_buffer[NAME_BUFFER_MAX + 1];
    uint32_t name_length;
} AppState;

static const EventDef* find_event_by_id(const EventDef* events, uint32_t event_count, uint16_t event_id) {
    for (uint32_t i = 0; i < event_count; i++) {
        if (events[i].event_id == event_id) {
            return &events[i];
        }
    }
    return NULL;
}

static void render_character_creation(AppState* app) {
    RenderColor bg = { 0.08f, 0.08f, 0.11f, 1.0f };
    RenderColor text_color = { 0.92f, 0.92f, 0.92f, 1.0f };
    RenderColor input_color = { 1.0f, 0.85f, 0.35f, 1.0f };
    RenderColor hint_color = { 0.6f, 0.6f, 0.6f, 1.0f };

    renderer_begin_frame(app->renderer, bg);

    float y = 20.0f;
    const float line_height = 26.0f;

    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, L"RawLife", text_color);
    y += line_height * 2.0f;

    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, L"What's your name?", text_color);
    y += line_height * 1.5f;

    /* Trailing underscore stands in for a text cursor -- no blink timer
     * yet, just a static marker showing where typing lands. */
    wchar_t input_line[NAME_BUFFER_MAX + 2];
    wsprintfW(input_line, L"%s_", app->name_buffer);
    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, input_line, input_color);
    y += line_height * 2.0f;

    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height,
                        L"(type your name, press ENTER to begin)", hint_color);

    if (!renderer_end_frame(app->renderer)) {
        OutputDebugStringA("RawLife: renderer device lost, recreation not yet implemented\n");
    }
}

static void app_render_frame(AppState* app) {
    if (app->screen == SCREEN_CHARACTER_CREATION) {
        render_character_creation(app);
        return;
    }

    RenderColor bg = { 0.08f, 0.08f, 0.11f, 1.0f };
    RenderColor text_color = { 0.92f, 0.92f, 0.92f, 1.0f };
    RenderColor player_color = { 1.0f, 0.85f, 0.35f, 1.0f };
    RenderColor log_color = { 0.65f, 0.85f, 1.0f, 1.0f };
    RenderColor choice_color = { 0.55f, 1.0f, 0.55f, 1.0f };

    renderer_begin_frame(app->renderer, bg);

    PersonPool* pool = app->world->people;
    wchar_t line[128];
    float y = 20.0f;
    const float line_height = 26.0f;
    bool pending = app->world->player_has_pending_choice;

    if (pending) {
        wsprintfW(line, L"RawLife -- Year %u   (press a number to choose, or S to skip)", app->world->year);
    } else {
        wsprintfW(line, L"RawLife -- Year %u   (press SPACE to advance a year)", app->world->year);
    }
    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, line, text_color);
    y += line_height * 1.5f;

    wsprintfW(line, L"YOU: %hs   age=%u   loyalty=%u   charisma=%u",
              pool->cold.name[app->player_id], pool->hot.age[app->player_id],
              pool->warm.loyalty[app->player_id], pool->warm.charisma[app->player_id]);
    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, line, player_color);
    y += line_height * 1.5f;

    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, L"Family:", text_color);
    y += line_height;

    wsprintfW(line, L"  %hs (mother)   age=%u", pool->cold.name[app->mother_id], pool->hot.age[app->mother_id]);
    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, line, text_color);
    y += line_height;

    wsprintfW(line, L"  %hs (father)   age=%u", pool->cold.name[app->father_id], pool->hot.age[app->father_id]);
    renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, line, text_color);
    y += line_height;

    y += line_height * 0.5f;

    if (pending) {
        const EventDef* pending_event = &app->events[app->world->player_pending_event_index];

        wchar_t header[128];
        wsprintfW(header, L"%hs!", pending_event->name);
        renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, header, text_color);
        y += line_height;

        for (uint32_t i = 0; i < pending_event->choice_count; i++) {
            wsprintfW(line, L"  [%u] %hs", i + 1, pending_event->choices[i].label);
            renderer_draw_text(app->renderer, 40.0f, y, 700.0f, line_height, line, choice_color);
            y += line_height;
        }
    } else {
        renderer_draw_text(app->renderer, 20.0f, y, 700.0f, line_height, L"This year:", text_color);
        y += line_height;

        if (app->world->tick_log_count == 0) {
            renderer_draw_text(app->renderer, 40.0f, y, 700.0f, line_height, L"(nothing yet)", log_color);
            y += line_height;
        } else {
            for (uint32_t i = 0; i < app->world->tick_log_count; i++) {
                const EventLogEntry* entry = &app->world->tick_log[i];
                const EventDef* ev = find_event_by_id(app->events, app->event_count, entry->event_id);
                const char* event_name = (ev != NULL) ? ev->name : "(unknown event)";
                bool has_choice = (ev != NULL) && entry->choice_index != NO_CHOICE_MADE
                                   && entry->choice_index < ev->choice_count;

                if (has_choice) {
                    const char* choice_label = ev->choices[entry->choice_index].label;
                    wsprintfW(line, L"%hs (%hs) -- %hs", event_name, choice_label, pool->cold.name[entry->person_id]);
                } else if (entry->partner_id != UINT32_MAX) {
                    wsprintfW(line, L"%hs -- %hs & %hs",
                              event_name, pool->cold.name[entry->person_id], pool->cold.name[entry->partner_id]);
                } else {
                    wsprintfW(line, L"%hs -- %hs", event_name, pool->cold.name[entry->person_id]);
                }
                renderer_draw_text(app->renderer, 40.0f, y, 700.0f, line_height, line, log_color);
                y += line_height;
            }
        }
    }

    if (!renderer_end_frame(app->renderer)) {
        /* Render target lost (e.g. graphics device reset). Not yet
         * handled -- would need to destroy and recreate the Renderer
         * here. Flagging rather than silently leaving the window blank. */
        OutputDebugStringA("RawLife: renderer device lost, recreation not yet implemented\n");
    }
}

/*
 * Spawns the player (using whatever name was entered on the character
 * creation screen) plus two parents, sets up the starting family
 * relationships, loads the event table, and switches to SCREEN_GAME.
 * Called once, when the player confirms their name with Enter.
 */
static void begin_game(AppState* app) {
    char player_name[128];
    int converted = WideCharToMultiByte(CP_UTF8, 0, app->name_buffer, -1,
                                         player_name, sizeof(player_name), NULL, NULL);
    if (converted <= 0) {
        strcpy(player_name, "Player"); /* conversion failed -- fall back rather than spawn with garbage */
    }

    /* Current year is 2026, so a person born at age 0 this run has that
     * birth year. */
    app->player_id = person_spawn(app->world->people, player_name, SEX_MALE, 2026);
    app->mother_id = person_spawn(app->world->people, "Mary", SEX_FEMALE, 1996);
    app->father_id = person_spawn(app->world->people, "Robert", SEX_MALE, 1994);

    app->world->people->hot.age[app->player_id] = 0;
    app->world->people->hot.age[app->mother_id] = 30;
    app->world->people->hot.age[app->father_id] = 32;
    app->world->people->hot.trait_flags[app->mother_id] |= TRAIT_FLAG_MARRIED;
    app->world->people->hot.trait_flags[app->father_id] |= TRAIT_FLAG_MARRIED;

    relation_add(app->world->relations, app->player_id, app->mother_id,
                 REL_CATEGORY_FAMILY_PARENT_CHILD, REL_STATUS_NONE, 0, 0, 0);
    relation_add(app->world->relations, app->player_id, app->father_id,
                 REL_CATEGORY_FAMILY_PARENT_CHILD, REL_STATUS_NONE, 0, 0, 0);
    relation_add(app->world->relations, app->mother_id, app->father_id,
                 REL_CATEGORY_NONE, REL_STATUS_SPOUSE, 180, 200, 150);

    world_set_player(app->world, app->player_id);

    app->screen = SCREEN_GAME;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    AppState* app = (AppState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_SIZE:
            if (app != NULL && app->renderer != NULL) {
                renderer_resize(app->renderer, LOWORD(lparam), HIWORD(lparam));
            }
            return 0;

        case WM_ERASEBKGND:
            return 1; /* Direct2D repaints the whole client area every frame -- avoid GDI flicker */

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (app != NULL) {
                app_render_frame(app);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CHAR:
            if (app != NULL && app->screen == SCREEN_CHARACTER_CREATION) {
                /* Printable characters only -- this excludes backspace (0x08),
                 * enter (0x0D), and other control chars, all handled via
                 * WM_KEYDOWN instead for clarity. */
                if (wparam >= 0x20 && wparam < 0x7F && app->name_length < NAME_BUFFER_MAX) {
                    app->name_buffer[app->name_length++] = (wchar_t)wparam;
                    app->name_buffer[app->name_length] = L'\0';
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;

        case WM_KEYDOWN:
            if (app == NULL) {
                return 0;
            }

            if (app->screen == SCREEN_CHARACTER_CREATION) {
                if (wparam == VK_BACK && app->name_length > 0) {
                    app->name_length--;
                    app->name_buffer[app->name_length] = L'\0';
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (wparam == VK_RETURN && app->name_length > 0) {
                    begin_game(app);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            /* SCREEN_GAME */
            if (app->world->player_has_pending_choice) {
                if (wparam >= '1' && wparam <= '9') {
                    uint32_t choice_index = (uint32_t)(wparam - '1');
                    const EventDef* pending_event = &app->events[app->world->player_pending_event_index];
                    if (choice_index < pending_event->choice_count) {
                        world_apply_player_choice(app->world, app->events, app->event_count, choice_index);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                } else if (wparam == 'S') {
                    world_skip_player_choice(app->world);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else if (wparam == VK_SPACE) {
                world_tick_year(app->world, app->events, app->event_count);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_DESTROY:
            if (app != NULL) {
                renderer_destroy(app->renderer);
                app->renderer = NULL;
            }
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    sim_smoke_test();

    AppState app;
    app.arena = arena_create(8 * 1024 * 1024);
    if (app.arena.base == NULL) {
        printf("RawLife: failed to allocate world arena\n");
        return 1;
    }

    app.world = world_create(&app.arena, 777);
    if (app.world == NULL) {
        printf("RawLife: world_create failed\n");
        return 1;
    }

    app.events = g_example_events;
    app.event_count = g_example_event_count;
    event_table_load(&app.arena, "build/events.bin", &app.events, &app.event_count);

    /* Player/parents aren't spawned yet -- that happens in begin_game(),
     * once a name is entered on the character creation screen. */
    app.player_id = UINT32_MAX;
    app.mother_id = UINT32_MAX;
    app.father_id = UINT32_MAX;
    app.screen = SCREEN_CHARACTER_CREATION;
    app.name_buffer[0] = L'\0';
    app.name_length = 0;

    app.renderer = NULL;

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
        arena_destroy(&app.arena);
        return 1;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)&app);

    app.renderer = renderer_create(hwnd);
    if (app.renderer == NULL) {
        printf("RawLife: renderer_create failed -- Direct2D/DirectWrite unavailable?\n");
        arena_destroy(&app.arena);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    arena_destroy(&app.arena);
    return 0;
}
