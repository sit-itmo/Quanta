/* SPDX-License-Identifier: MIT
 * Native Win32 event transport + GDI presentation. No OpenGL, DirectX,
 * dynamically loaded vendor renderer or third-party platform library.
 */
#include "quanta/osal.h"
#include "window.h"
#include "windows_internal.h"
#include <windowsx.h>
#define QUANTA_EVENT_CAP 1024
static const wchar_t window_class[] = L"Quanta.NativeWindow.0_3";

struct quanta_window
{
    HWND hwnd;
    HINSTANCE module;
    quanta_window_event events[QUANTA_EVENT_CAP];
    size_t head, count;
    int error, buttons, wheel_vertical, wheel_horizontal;
    uint16_t surrogate;
    uint32_t *pixels;
    size_t pixel_capacity;
    int width, height;
    float scale;
};

static void queue(quanta_window *w, const quanta_window_event *e)
{
    size_t last;
    if (w->error)
    {
        return;
    }
    /* Motion and resize bursts may be coalesced; key/button transitions may not.
     */
    if (w->count && (e->kind == QUANTA_X_MOTION || e->kind == QUANTA_X_RESIZE))
    {
        last = (w->head + w->count - 1) % QUANTA_EVENT_CAP;
        if (w->events[last].kind == e->kind)
        {
            w->events[last] = *e;
            return;
        }
    }
    if (w->count == QUANTA_EVENT_CAP)
    {
        w->error = 1;
        return;
    }
    w->events[(w->head + w->count) % QUANTA_EVENT_CAP] = *e;
    w->count++;
}

static int modifiers(void)
{
    return ((GetKeyState(VK_SHIFT) & 0x8000) ? QUANTA_X_SHIFT : 0) |
           ((GetKeyState(VK_CONTROL) & 0x8000) ? QUANTA_X_CTRL : 0) |
           ((GetKeyState(VK_MENU) & 0x8000) ? QUANTA_X_ALT : 0) |
           (((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) ? QUANTA_X_SUPER : 0);
}

/* Stable, internal key IDs shared with the XCB adapter; no X11 API dependency.
 */
static uint32_t key_id(WPARAM key, LPARAM flags)
{
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
    {
        return (uint32_t)key;
    }
    switch (key)
    {
        case VK_TAB:
            return 0xff09;
        case VK_RETURN:
            return 0xff0d;
        case VK_ESCAPE:
            return 0xff1b;
        case VK_BACK:
            return 0xff08;
        case VK_DELETE:
            return 0xffff;
        case VK_INSERT:
            return 0xff63;
        case VK_HOME:
            return 0xff50;
        case VK_END:
            return 0xff57;
        case VK_PRIOR:
            return 0xff55;
        case VK_NEXT:
            return 0xff56;
        case VK_LEFT:
            return 0xff51;
        case VK_UP:
            return 0xff52;
        case VK_RIGHT:
            return 0xff53;
        case VK_DOWN:
            return 0xff54;
        case VK_SHIFT:
            return MapVirtualKeyW((UINT)((flags >> 16) & 0xff), MAPVK_VSC_TO_VK_EX) == VK_RSHIFT
                       ? 0xffe2
                       : 0xffe1;
        case VK_CONTROL:
            return (flags & (1L << 24)) ? 0xffe4 : 0xffe3;
        case VK_MENU:
            return (flags & (1L << 24)) ? 0xffea : 0xffe9;
        case VK_LWIN:
            return 0xffeb;
        case VK_RWIN:
            return 0xffec;
        case VK_SPACE:
            return ' ';
        case VK_OEM_MINUS:
            return '-';
        case VK_OEM_PLUS:
            return '=';
        case VK_OEM_COMMA:
            return ',';
        case VK_OEM_PERIOD:
            return '.';
        case VK_OEM_1:
            return ';';
        case VK_OEM_2:
            return '/';
        case VK_OEM_3:
            return '`';
        case VK_OEM_4:
            return '[';
        case VK_OEM_5:
            return '\\';
        case VK_OEM_6:
            return ']';
        case VK_OEM_7:
            return '\'';
        default:
            return 0;
    }
}

static int paint(quanta_window *w, HDC dc)
{
    BITMAPINFO bitmap;
    RECT rect;
    int result;
    if (!w->pixels || w->width <= 0 || w->height <= 0)
    {
        return 1;
    }
    if (!GetClientRect(w->hwnd, &rect))
    {
        return 0;
    }
    if (rect.right <= 0 || rect.bottom <= 0)
    {
        return 1;
    }
    quanta_memset(&bitmap, 0, sizeof(bitmap));
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = w->width;
    bitmap.bmiHeader.biHeight = -w->height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    result = StretchDIBits(dc, 0, 0, rect.right, rect.bottom, 0, 0, w->width, w->height, w->pixels,
                           &bitmap, DIB_RGB_COLORS, SRCCOPY);
    return result != GDI_ERROR && result != 0;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    quanta_window *w = (quanta_window *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    quanta_window_event e;
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTW *cs = (const CREATESTRUCTW *)lp;
        w = (quanta_window *)cs->lpCreateParams;
        w->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
    }
    if (!w)
    {
        return DefWindowProcW(hwnd, message, wp, lp);
    }
    quanta_memset(&e, 0, sizeof(e));
    switch (message)
    {
        case WM_CLOSE:
            e.kind = QUANTA_X_CLOSE;
            queue(w, &e);
            return 0;
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED && LOWORD(lp) > 0 && HIWORD(lp) > 0)
            {
                e.kind = QUANTA_X_RESIZE;
                e.width = (int)LOWORD(lp);
                e.height = (int)HIWORD(lp);
                queue(w, &e);
            }
            return 0;
        case WM_MOUSEMOVE:
            e.kind = QUANTA_X_MOTION;
            e.x = GET_X_LPARAM(lp);
            e.y = GET_Y_LPARAM(lp);
            queue(w, &e);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            e.kind = QUANTA_X_BUTTON;
            e.x = GET_X_LPARAM(lp);
            e.y = GET_Y_LPARAM(lp);
            e.button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)   ? 0
                       : (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? 1
                                                                                : 2;
            e.pressed =
                message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
            if (e.pressed)
            {
                SetFocus(hwnd);
                SetCapture(hwnd);
                w->buttons |= 1 << e.button;
            }
            else
            {
                w->buttons &= ~(1 << e.button);
                if (!w->buttons)
                {
                    ReleaseCapture();
                }
            }
            queue(w, &e);
            return 0;
        case WM_CAPTURECHANGED:
            if ((HWND)lp != hwnd)
            {
                int b;
                for (b = 0; b < 3; b++)
                {
                    if (w->buttons & (1 << b))
                    {
                        e.kind = QUANTA_X_BUTTON;
                        e.button = b;
                        e.pressed = 0;
                        queue(w, &e);
                    }
                }
                w->buttons = 0;
            }
            return 0;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            e.kind = QUANTA_X_WHEEL;
            if (message == WM_MOUSEWHEEL)
            {
                w->wheel_vertical += GET_WHEEL_DELTA_WPARAM(wp);
                e.y = w->wheel_vertical / WHEEL_DELTA;
                w->wheel_vertical %= WHEEL_DELTA;
            }
            else
            {
                w->wheel_horizontal += GET_WHEEL_DELTA_WPARAM(wp);
                e.x = w->wheel_horizontal / WHEEL_DELTA;
                w->wheel_horizontal %= WHEEL_DELTA;
            }
            if (e.x || e.y)
            {
                queue(w, &e);
            }
            return 0;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            e.kind = QUANTA_X_FOCUS;
            e.pressed = message == WM_SETFOCUS;
            if (!e.pressed)
            {
                w->surrogate = 0;
            }
            queue(w, &e);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            e.kind = QUANTA_X_KEY;
            e.keysym = key_id(wp, lp);
            e.mods = modifiers();
            e.pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            queue(w, &e);
            /* Preserve native Alt+F4 and system-key handling. */
            if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
            {
                return DefWindowProcW(hwnd, message, wp, lp);
            }
            return 0;
        case WM_CHAR:
            e.kind = QUANTA_X_KEY;
            e.mods = modifiers();
            e.pressed = 1;
            if (wp >= 0xd800 && wp <= 0xdbff)
            {
                w->surrogate = (uint16_t)wp;
                return 0;
            }
            if (wp >= 0xdc00 && wp <= 0xdfff)
            {
                if (w->surrogate)
                {
                    e.codepoint =
                        0x10000 + (((uint32_t)w->surrogate - 0xd800) << 10) + (uint32_t)wp - 0xdc00;
                }
                w->surrogate = 0;
            }
            else
            {
                w->surrogate = 0;
                if (wp >= 32 && wp != 127)
                {
                    e.codepoint = (uint32_t)wp;
                }
            }
            if (e.codepoint)
            {
                queue(w, &e);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (!dc || !paint(w, dc))
            {
                w->error = 1;
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            w->hwnd = NULL;
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

int quanta_window_open(const char *title, int width, int height, quanta_window **out, char *error,
                       size_t cap)
{
    quanta_window *w;
    WNDCLASSEXW wc;
    RECT rect, work;
    wchar_t *wide;
    HDC screen;
    DWORD style = WS_OVERLAPPEDWINDOW, ex = WS_EX_APPWINDOW;
    if (!out || width <= 0 || height <= 0 || width > 4096 || height > 4096)
    {
        return 0;
    }
    *out = NULL;
    w = (quanta_window *)quanta_calloc(1, sizeof(*w));
    wide = quanta_win_widen(title);
    if (!w || !wide)
    {
        quanta_free(w);
        quanta_free(wide);
        quanta_copy(error, cap, "Cannot allocate window state/title");
        return 0;
    }
    w->module = GetModuleHandleW(NULL);
    w->scale = 1.0f;
    screen = GetDC(NULL);
    if (screen)
    {
        int dpi = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(NULL, screen);
        if (dpi >= 96 && dpi <= 384)
        {
            w->scale = (float)dpi / 96.0f;
        }
    }
    quanta_memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.hInstance = w->module;
    wc.lpfnWndProc = window_proc;
    wc.lpszClassName = window_class;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        goto fail;
    }
    rect.left = rect.top = 0;
    rect.right = (LONG)(width * w->scale);
    rect.bottom = (LONG)(height * w->scale);
    if (!AdjustWindowRectEx(&rect, style, FALSE, ex))
    {
        goto fail;
    }
    width = (int)(rect.right - rect.left);
    height = (int)(rect.bottom - rect.top);
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0))
    {
        width = QUANTA_MIN(width, (int)(work.right - work.left));
        height = QUANTA_MIN(height, (int)(work.bottom - work.top));
    }
    w->hwnd = CreateWindowExW(ex, window_class, wide, style, CW_USEDEFAULT, CW_USEDEFAULT, width,
                              height, NULL, NULL, w->module, w);
    if (!w->hwnd)
    {
        goto fail;
    }
    quanta_free(wide);
    ShowWindow(w->hwnd, SW_SHOW);
    UpdateWindow(w->hwnd);
    *out = w;
    return 1;
fail:
    if (error && cap)
    {
        quanta_snprintf(error, cap, "Win32 window creation failed (error %lu)",
                 (unsigned long)GetLastError());
    }
    quanta_free(wide);
    quanta_window_close(w);
    return 0;
}

int quanta_window_poll(quanta_window *w, quanta_window_event *e)
{
    MSG message;
    size_t dispatched = 0;
    if (!w || !e || w->error)
    {
        return -1;
    }
    while (w->count < QUANTA_EVENT_CAP / 2 && dispatched++ < 256 &&
           PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            quanta_memset(e, 0, sizeof(*e));
            e->kind = QUANTA_X_CLOSE;
            return 1;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (w->error)
    {
        return -1;
    }
    if (!w->count)
    {
        return 0;
    }
    *e = w->events[w->head];
    w->head = (w->head + 1) % QUANTA_EVENT_CAP;
    w->count--;
    return 1;
}

int quanta_window_present(quanta_window *w, const uint32_t *pixels, int width, int height)
{
    size_t count;
    HDC dc;
    int ok;
    if (!w || !w->hwnd || w->error || !pixels || width <= 0 || height <= 0 || width > 4096 ||
        height > 4096)
    {
        return 0;
    }
    count = (size_t)width * (size_t)height;
    if (count > w->pixel_capacity)
    {
        uint32_t *next = (uint32_t *)quanta_realloc(w->pixels, count * sizeof(*next));
        if (!next)
        {
            return 0;
        }
        w->pixels = next;
        w->pixel_capacity = count;
    }
    quanta_memcpy(w->pixels, pixels, count * sizeof(*pixels));
    w->width = width;
    w->height = height;
    if (IsIconic(w->hwnd))
    {
        return 1;
    }
    dc = GetDC(w->hwnd);
    if (!dc)
    {
        return 0;
    }
    ok = paint(w, dc);
    ReleaseDC(w->hwnd, dc);
    return ok;
}

void quanta_window_close(quanta_window *w)
{
    if (!w)
    {
        return;
    }
    if (w->hwnd)
    {
        DestroyWindow(w->hwnd);
    }
    quanta_free(w->pixels);
    quanta_free(w);
}

void *quanta_window_native(quanta_window *w)
{
    return w ? (void *)w->hwnd : NULL;
}

float quanta_window_scale(quanta_window *w)
{
    return w ? w->scale : 1.0f;
}
