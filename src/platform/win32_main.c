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

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

static void arena_smoke_test(void) {
    /* Sanity check that the arena allocator works before anything else
     * in the game depends on it. Removed once real unit tests exist. */
    Arena arena = arena_create(1024 * 1024); /* 1 MB */
    if (arena.base == NULL) {
        OutputDebugStringA("RawLife: arena_create failed\n");
        return;
    }

    int* a = (int*)arena_alloc(&arena, sizeof(int) * 16, sizeof(int));
    int* b = (int*)arena_alloc(&arena, sizeof(int) * 16, sizeof(int));
    if (a != NULL && b != NULL) {
        a[0] = 1;
        b[0] = 2;
        char buf[64];
        wsprintfA(buf, "RawLife: arena OK, used=%zu bytes\n", arena.used);
        OutputDebugStringA(buf);
    }

    arena_destroy(&arena);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    arena_smoke_test();

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
