/*
 * Native Win32 entry point and message loop.
 *
 * This is intentionally minimal for now: it opens a window and pumps
 * messages. Rendering (Direct2D) and the simulation tick loop are not
 * wired in yet -- this step is just the platform shell + a smoke test
 * for the arena allocator, so we have a real buildable baseline before
 * layering in simulation and rendering.
 */

#include <windows.h>
#include <stdio.h>

#include "sim/arena.h"
#include "sim/person.h"
#include "sim/event.h"
#include "sim/traits.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

/* Sanity check for the arena allocator + person pool before anything else
 * in the game depends on them. Exercises spawn, kill, and slot recycling.
 * Removed once real unit tests exist. */
static void sim_smoke_test(void) {
    Arena arena = arena_create(4 * 1024 * 1024); /* 4 MB -- plenty for one PersonPool */
    if (arena.base == NULL) {
        OutputDebugStringA("RawLife: arena_create failed\n");
        return;
    }

    PersonPool* pool = person_pool_create(&arena);
    if (pool == NULL) {
        OutputDebugStringA("RawLife: person_pool_create failed\n");
        arena_destroy(&arena);
        return;
    }

    uint32_t alice = person_spawn(pool, "Alice", SEX_FEMALE, 2000);
    uint32_t bob   = person_spawn(pool, "Bob", SEX_MALE, 1998);

    char buf[128];
    wsprintfA(buf, "RawLife: spawned %s (id=%lu, alive=%d) and %s (id=%lu, alive=%d)\n",
              pool->cold.name[alice], alice, person_is_alive(pool, alice),
              pool->cold.name[bob], bob, person_is_alive(pool, bob));
    OutputDebugStringA(buf);

    person_kill(pool, alice);
    wsprintfA(buf, "RawLife: killed Alice -- alive=%d, free_count=%lu\n",
              person_is_alive(pool, alice), pool->free_count);
    OutputDebugStringA(buf);

    /* Slot should be recycled here rather than allocating a fresh one. */
    uint32_t carol = person_spawn(pool, "Carol", SEX_FEMALE, 2001);
    wsprintfA(buf, "RawLife: spawned %s -- recycled id=%lu (expected %lu), free_count=%lu\n",
              pool->cold.name[carol], carol, alice, pool->free_count);
    OutputDebugStringA(buf);

    /* Event eligibility smoke test: prove age-gating and trait-gating
     * both flow through event_eligible() as the single choke point. */
    uint32_t dave = person_spawn(pool, "Dave", SEX_MALE, 2010);
    pool->hot.age[dave] = 16; /* still a minor */
    pool->hot.trait_flags[dave] |= TRAIT_FLAG_MARRIED; /* contrived, just for the test */

    for (uint32_t i = 0; i < g_example_event_count; i++) {
        const EventDef* e = &g_example_events[i];
        bool eligible = event_eligible(e, pool->hot.age[dave], pool->hot.trait_flags[dave]);
        wsprintfA(buf, "RawLife: [age %u] event '%s' eligible=%d\n",
                  pool->hot.age[dave], e->name, eligible);
        OutputDebugStringA(buf);
    }

    pool->hot.age[dave] = 25; /* now an adult */
    OutputDebugStringA("RawLife: -- Dave turns 25 --\n");
    for (uint32_t i = 0; i < g_example_event_count; i++) {
        const EventDef* e = &g_example_events[i];
        bool eligible = event_eligible(e, pool->hot.age[dave], pool->hot.trait_flags[dave]);
        wsprintfA(buf, "RawLife: [age %u] event '%s' eligible=%d\n",
                  pool->hot.age[dave], e->name, eligible);
        OutputDebugStringA(buf);
    }

    pool->hot.trait_flags[dave] |= TRAIT_FLAG_LOYAL; /* now loyal */
    OutputDebugStringA("RawLife: -- Dave becomes loyal --\n");
    const EventDef* cheat = &g_example_events[3]; /* "Cheated on partner" */
    wsprintfA(buf, "RawLife: event '%s' eligible=%d (expected 0 -- loyal excludes it)\n",
              cheat->name, event_eligible(cheat, pool->hot.age[dave], pool->hot.trait_flags[dave]));
    OutputDebugStringA(buf);

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
