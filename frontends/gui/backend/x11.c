/**
 * @file frontends/gui/backend/x11.c
 *
 * X11/Xlib framebuffer window: display connection, UTF-8 input, resize/close handling and CPU-
 * buffer presentation.
 *
 * Backend: the windowing/graphics implementation of an interface. The frontend builds widgets
 * without changing how a metric is measured.
 *
 * Framebuffer: an in-memory pixel array. Pixels use 0x00RRGGBB and stride is measured in pixels;
 * presentation on a display is a separate operation.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "window.h"
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

struct quanta_window
{
    Display *display;
    Window window;
    GC gc;
    Atom protocols, close;
    XIM im;
    XIC ic;
    Visual *visual;
    int depth, width, height;
};

int quanta_window_open(const char *title, int width, int height,
                       quanta_window **out, char *error, size_t capacity)
{
    quanta_window *p;
    int screen;
    if (!out || !title || width <= 0 || height <= 0 || width > 4096 || height > 4096) return 0;
    *out = NULL;
    p = (quanta_window *)quanta_calloc(1, sizeof(*p));
    if (!p)
    {
        quanta_snprintf(error, capacity, "cannot allocate X11 window state");
        return 0;
    }
    p->display = XOpenDisplay(NULL);
    if (!p->display)
    {
        quanta_snprintf(error, capacity, "cannot open X11 display; check DISPLAY and XAUTHORITY");
        quanta_window_close(p);
        return 0;
    }
    screen = DefaultScreen(p->display);
    p->visual = DefaultVisual(p->display, screen);
    p->depth = DefaultDepth(p->display, screen);
    if (p->visual->class != TrueColor || p->depth != 24 ||
        p->visual->red_mask != 0xff0000 || p->visual->green_mask != 0xff00 ||
        p->visual->blue_mask != 0xff)
    {
        quanta_snprintf(error, capacity, "framebuffer window needs a 24-bit RGB TrueColor visual");
        quanta_window_close(p);
        return 0;
    }
    p->width = width;
    p->height = height;
    p->window = XCreateSimpleWindow(p->display, RootWindow(p->display, screen),
                                    0, 0, (unsigned)width, (unsigned)height, 0, 0, 0x101721);
    p->gc = XCreateGC(p->display, p->window, 0, NULL);
    XSelectInput(p->display, p->window, ExposureMask | StructureNotifyMask | KeyPressMask |
                  KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                  FocusChangeMask);
    p->protocols = XInternAtom(p->display, "WM_PROTOCOLS", False);
    p->close = XInternAtom(p->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(p->display, p->window, &p->close, 1);
    XStoreName(p->display, p->window, title);
    {
        Atom utf8 = XInternAtom(p->display, "UTF8_STRING", False);
        Atom name = XInternAtom(p->display, "_NET_WM_NAME", False);
        XChangeProperty(p->display, p->window, name, utf8, 8, PropModeReplace,
                        (const unsigned char *)title, (int)quanta_strlen(title));
    }
    /* The numeric parser stays locale-independent; only text input uses LC_CTYPE. */
    (void)setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    p->im = XOpenIM(p->display, NULL, NULL, NULL);
    if (p->im)
    {
        p->ic = XCreateIC(p->im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                         XNClientWindow, p->window, XNFocusWindow, p->window, NULL);
    }
    XMapWindow(p->display, p->window);
    XFlush(p->display);
    *out = p;
    return 1;
}

static int modifiers(unsigned state)
{
    return ((state & ShiftMask) ? QUANTA_X_SHIFT : 0) |
           ((state & ControlMask) ? QUANTA_X_CTRL : 0) |
           ((state & Mod1Mask) ? QUANTA_X_ALT : 0) |
           ((state & Mod4Mask) ? QUANTA_X_SUPER : 0);
}

int quanta_window_poll(quanta_window *p, quanta_window_event *out)
{
    XEvent event;
    quanta_memset(out, 0, sizeof(*out));
    if (!XPending(p->display)) return 0;
    XNextEvent(p->display, &event);
    if (XFilterEvent(&event, p->window)) return 1;
    switch (event.type)
    {
        case ClientMessage:
            if (event.xclient.message_type == p->protocols &&
                event.xclient.format == 32 && (Atom)event.xclient.data.l[0] == p->close)
                out->kind = QUANTA_X_CLOSE;
            break;
        case DestroyNotify:
            p->window = 0;
            out->kind = QUANTA_X_CLOSE;
            break;
        case ConfigureNotify:
            p->width = out->width = event.xconfigure.width;
            p->height = out->height = event.xconfigure.height;
            out->kind = QUANTA_X_RESIZE;
            break;
        case MotionNotify:
            out->kind = QUANTA_X_MOTION;
            out->x = event.xmotion.x;
            out->y = event.xmotion.y;
            break;
        case ButtonPress:
        case ButtonRelease:
            out->pressed = event.type == ButtonPress;
            if (event.xbutton.button >= 4 && event.xbutton.button <= 7)
            {
                if (out->pressed)
                {
                    out->kind = QUANTA_X_WHEEL;
                    out->y = event.xbutton.button == 4 ? 1 : event.xbutton.button == 5 ? -1 : 0;
                    out->x = event.xbutton.button == 6 ? 1 : event.xbutton.button == 7 ? -1 : 0;
                }
            }
            else if (event.xbutton.button >= 1 && event.xbutton.button <= 3)
            {
                out->kind = QUANTA_X_BUTTON;
                out->button = event.xbutton.button == 1 ? 0 : event.xbutton.button == 3 ? 1 : 2;
                out->x = event.xbutton.x;
                out->y = event.xbutton.y;
            }
            break;
        case FocusIn:
        case FocusOut:
            out->kind = QUANTA_X_FOCUS;
            out->pressed = event.type == FocusIn;
            if (p->ic)
            {
                if (out->pressed) XSetICFocus(p->ic);
                else XUnsetICFocus(p->ic);
            }
            break;
        case KeyPress:
        case KeyRelease:
        {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            int modifier = 0;
            out->kind = QUANTA_X_KEY;
            out->pressed = event.type == KeyPress;
            out->mods = modifiers(event.xkey.state);
            out->keysym = (uint32_t)sym;
            if (sym == XK_Shift_L || sym == XK_Shift_R) modifier = QUANTA_X_SHIFT;
            if (sym == XK_Control_L || sym == XK_Control_R) modifier = QUANTA_X_CTRL;
            if (sym == XK_Alt_L || sym == XK_Alt_R) modifier = QUANTA_X_ALT;
            if (sym == XK_Super_L || sym == XK_Super_R) modifier = QUANTA_X_SUPER;
            if (out->pressed) out->mods |= modifier;
            else out->mods &= ~modifier;
            if (out->pressed && !(out->mods & (QUANTA_X_CTRL | QUANTA_X_ALT | QUANTA_X_SUPER)))
            {
                if (p->ic)
                {
                    Status status;
                    int length = Xutf8LookupString(p->ic, &event.xkey, out->text,
                                                   (int)sizeof(out->text) - 1, &sym, &status);
                    if (status == XBufferOverflow || length < 0 ||
                        length >= (int)sizeof(out->text)) out->text[0] = 0;
                    else out->text[length] = 0;
                }
                else
                {
                    char text[16];
                    int length = XLookupString(&event.xkey, text, sizeof(text), &sym, NULL);
                    if (length == 1 && (unsigned char)text[0] >= 32)
                        out->codepoint = (unsigned char)text[0];
                    else if ((sym & 0xff000000u) == 0x01000000u)
                        out->codepoint = (uint32_t)sym & 0xffffffu;
                }
            }
            break;
        }
        default: break;
    }
    return 1;
}

int quanta_window_present(quanta_window *p, const uint32_t *pixels, int width, int height)
{
    XImage *image;
    const uint16_t endian = 1;
    int ok;
    if (!pixels || !p->window || width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return 0;
    image = XCreateImage(p->display, p->visual, (unsigned)p->depth, ZPixmap, 0,
                        (char *)pixels, (unsigned)width, (unsigned)height, 32, width * 4);
    if (!image) return 0;
    ok = image->bits_per_pixel == 32 &&
         image->byte_order == (*(const unsigned char *)&endian ? LSBFirst : MSBFirst);
    if (ok)
    {
        XPutImage(p->display, p->window, p->gc, image, 0, 0, 0, 0,
                  (unsigned)width, (unsigned)height);
        XFlush(p->display);
    }
    image->data = NULL; // The caller owns the pixels, XDestroyImage owns only its header.
    XDestroyImage(image);
    return ok;
}

void quanta_window_close(quanta_window *p)
{
    if (!p) return;
    if (p->ic) XDestroyIC(p->ic);
    if (p->im) XCloseIM(p->im);
    if (p->display)
    {
        if (p->gc) XFreeGC(p->display, p->gc);
        if (p->window) XDestroyWindow(p->display, p->window);
        XCloseDisplay(p->display);
    }
    quanta_free(p);
}
void *quanta_window_native(quanta_window *p) { (void)p; return NULL; }
float quanta_window_scale(quanta_window *p) { (void)p; return 1.0f; }
