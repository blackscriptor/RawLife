#ifndef RAWLIFE_RENDERER_H
#define RAWLIFE_RENDERER_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

/*
 * Thin wrapper around Direct2D (drawing) + DirectWrite (text), bound to a
 * single HWND. Deliberately minimal for now: clear, filled rectangles,
 * and single-line text -- enough to get the simulation's state on screen.
 * No layout system, no sprites/images, no animation yet.
 */
typedef struct Renderer Renderer;

typedef struct {
    float r, g, b, a; /* 0.0-1.0 range */
} RenderColor;

/* Creates a Direct2D render target bound to `hwnd`. Returns NULL on
 * failure (Direct2D/DirectWrite unavailable, out of memory, etc). */
Renderer* renderer_create(HWND hwnd);

void renderer_destroy(Renderer* renderer);

/* Call from WM_SIZE so the render target's backing surface matches the
 * new client area (width/height in pixels). */
void renderer_resize(Renderer* renderer, uint32_t width, uint32_t height);

/* Begins a frame: clears to `clear_color`. Must be paired with a matching
 * renderer_end_frame call. */
void renderer_begin_frame(Renderer* renderer, RenderColor clear_color);

/* Ends and presents a frame. Returns false if the render target was lost
 * (e.g. a graphics device reset) -- the caller should destroy and
 * recreate the Renderer if this happens. Not yet handled automatically. */
bool renderer_end_frame(Renderer* renderer);

/* Draws `text` left-aligned, top-aligned within the box (x, y, w, h). */
void renderer_draw_text(Renderer* renderer, float x, float y, float w, float h,
                         const wchar_t* text, RenderColor color);

void renderer_fill_rect(Renderer* renderer, float x, float y, float w, float h, RenderColor color);

#endif /* RAWLIFE_RENDERER_H */
