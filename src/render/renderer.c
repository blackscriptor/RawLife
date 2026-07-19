/*
 * Direct2D/DirectWrite are COM APIs. C has no member-function syntax, so
 * every call goes through the Interface_Method(instance, args...) macro
 * pattern -- COBJMACROS (defined below, before the headers that need it)
 * is what makes those macros available. There's no D2D1:: helper
 * namespace in C either, so structs like D2D1_RENDER_TARGET_PROPERTIES
 * are filled in by hand; a zeroed struct happens to mean "use sensible
 * defaults" for most of Direct2D's property structs, which keeps this
 * shorter than it could be.
 *
 * INITGUID forces GUID constants (IID_ID2D1Factory, IID_IDWriteFactory,
 * etc.) to be instantiated directly in this translation unit rather than
 * declared as external symbols. Direct2D's GUIDs happen to be present in
 * MinGW's libuuid.a, but DirectWrite's aren't -- INITGUID covers both
 * uniformly rather than depending on which library happened to include
 * which GUIDs.
 */
#define COBJMACROS
#define INITGUID
#include <initguid.h>

#include "render/renderer.h"

#include <d2d1.h>
#include <dwrite.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct Renderer {
    ID2D1Factory*          factory;
    ID2D1HwndRenderTarget* target;
    IDWriteFactory*        dwrite_factory;
    IDWriteTextFormat*     text_format;
    ID2D1SolidColorBrush*  brush; /* recolored per-draw-call rather than recreated */
};

Renderer* renderer_create(HWND hwnd) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    Renderer* r = (Renderer*)calloc(1, sizeof(Renderer));
    if (r == NULL) {
        return NULL;
    }

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void**)&r->factory);
    if (FAILED(hr)) {
        free(r);
        return NULL;
    }

    RECT rc;
    GetClientRect(hwnd, &rc);

    D2D1_RENDER_TARGET_PROPERTIES rt_props;
    memset(&rt_props, 0, sizeof(rt_props)); /* zeroed = default type/pixel format/DPI */

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props;
    memset(&hwnd_props, 0, sizeof(hwnd_props));
    hwnd_props.hwnd = hwnd;
    hwnd_props.pixelSize.width = (UINT32)(rc.right - rc.left);
    hwnd_props.pixelSize.height = (UINT32)(rc.bottom - rc.top);

    hr = ID2D1Factory_CreateHwndRenderTarget(r->factory, &rt_props, &hwnd_props, &r->target);
    if (FAILED(hr)) {
        ID2D1Factory_Release(r->factory);
        free(r);
        return NULL;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown**)&r->dwrite_factory);
    if (FAILED(hr)) {
        ID2D1HwndRenderTarget_Release(r->target);
        ID2D1Factory_Release(r->factory);
        free(r);
        return NULL;
    }

    hr = IDWriteFactory_CreateTextFormat(
        r->dwrite_factory,
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0f,
        L"en-us",
        &r->text_format
    );
    if (FAILED(hr)) {
        IDWriteFactory_Release(r->dwrite_factory);
        ID2D1HwndRenderTarget_Release(r->target);
        ID2D1Factory_Release(r->factory);
        free(r);
        return NULL;
    }

    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    hr = ID2D1HwndRenderTarget_CreateSolidColorBrush(r->target, &white, NULL, &r->brush);
    if (FAILED(hr)) {
        IDWriteTextFormat_Release(r->text_format);
        IDWriteFactory_Release(r->dwrite_factory);
        ID2D1HwndRenderTarget_Release(r->target);
        ID2D1Factory_Release(r->factory);
        free(r);
        return NULL;
    }

    return r;
}

void renderer_destroy(Renderer* r) {
    if (r == NULL) {
        return;
    }
    if (r->brush != NULL) ID2D1SolidColorBrush_Release(r->brush);
    if (r->text_format != NULL) IDWriteTextFormat_Release(r->text_format);
    if (r->dwrite_factory != NULL) IDWriteFactory_Release(r->dwrite_factory);
    if (r->target != NULL) ID2D1HwndRenderTarget_Release(r->target);
    if (r->factory != NULL) ID2D1Factory_Release(r->factory);
    free(r);
}

void renderer_resize(Renderer* r, uint32_t width, uint32_t height) {
    if (r == NULL || r->target == NULL) {
        return;
    }
    D2D1_SIZE_U size;
    size.width = width;
    size.height = height;
    ID2D1HwndRenderTarget_Resize(r->target, &size);
}

void renderer_begin_frame(Renderer* r, RenderColor clear_color) {
    ID2D1HwndRenderTarget_BeginDraw(r->target);
    D2D1_COLOR_F c;
    c.r = clear_color.r; c.g = clear_color.g; c.b = clear_color.b; c.a = clear_color.a;
    ID2D1HwndRenderTarget_Clear(r->target, &c);
}

bool renderer_end_frame(Renderer* r) {
    HRESULT hr = ID2D1HwndRenderTarget_EndDraw(r->target, NULL, NULL);
    return hr != D2DERR_RECREATE_TARGET;
}

void renderer_draw_text(Renderer* r, float x, float y, float w, float h, const wchar_t* text, RenderColor color) {
    D2D1_COLOR_F c;
    c.r = color.r; c.g = color.g; c.b = color.b; c.a = color.a;
    ID2D1SolidColorBrush_SetColor(r->brush, &c);

    D2D1_RECT_F rect;
    rect.left = x; rect.top = y; rect.right = x + w; rect.bottom = y + h;

    ID2D1HwndRenderTarget_DrawText(
        r->target,
        text,
        (UINT32)wcslen(text),
        r->text_format,
        &rect,
        (ID2D1Brush*)r->brush,
        D2D1_DRAW_TEXT_OPTIONS_NONE,
        DWRITE_MEASURING_MODE_NATURAL
    );
}

void renderer_fill_rect(Renderer* r, float x, float y, float w, float h, RenderColor color) {
    D2D1_COLOR_F c;
    c.r = color.r; c.g = color.g; c.b = color.b; c.a = color.a;
    ID2D1SolidColorBrush_SetColor(r->brush, &c);

    D2D1_RECT_F rect;
    rect.left = x; rect.top = y; rect.right = x + w; rect.bottom = y + h;

    ID2D1HwndRenderTarget_FillRectangle(r->target, &rect, (ID2D1Brush*)r->brush);
}
