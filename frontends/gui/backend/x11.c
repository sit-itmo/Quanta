/* SPDX-License-Identifier: MIT
 * Local X11 connection. Intentionally avoids xcb_connect(), DNS, NSS and
 * dynamically loaded graphics drivers. Authentication supports the standard
 * local MIT-MAGIC-COOKIE-1 Xauthority records. No network X11/SSH forwarding.
 */
#include "quanta/osal.h"
#include "window.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

struct quanta_window
{
    xcb_connection_t *c;
    xcb_screen_t *screen;
    xcb_window_t window;
    xcb_gcontext_t gc;
    xcb_atom_t wm_protocols, wm_delete;
    xcb_get_keyboard_mapping_reply_t *keymap;
    int width, height;
    uint8_t min_key, max_key;
};

static int u16(quanta_file *f, unsigned *v)
{
    int a = quanta_fgetc(f), b = quanta_fgetc(f);
    if (a == QUANTA_EOF || b == QUANTA_EOF)
    {
        return 0;
    }
    *v = (unsigned)((a << 8) | b);
    return 1;
}

static int field(quanta_file *f, unsigned char *b, size_t cap, size_t *len)
{
    unsigned n;
    if (!u16(f, &n) || n >= cap)
    {
        return 0;
    }
    if (quanta_fread(b, 1, n, f) != n)
    {
        return 0;
    }
    b[n] = 0;
    *len = n;
    return 1;
}

static int auth_cookie(int display, unsigned char *cookie, size_t *size)
{
    const char *xa = quanta_getenv("XAUTHORITY");
    char path[PATH_MAX], number[16], hostname[256];
    quanta_file *f;
    unsigned family;
    int best = 0;
    if (!xa || !*xa)
    {
        const char *home = quanta_getenv("HOME");
        if (!home || quanta_snprintf(path, sizeof(path), "%s/.Xauthority", home) >= (int)sizeof(path))
        {
            return 0;
        }
        xa = path;
    }
    f = quanta_fopen(xa, "rb");
    if (!f)
    {
        return 0;
    }
    quanta_snprintf(number, sizeof(number), "%d", display);
    hostname[0] = 0;
    gethostname(hostname, sizeof(hostname) - 1);
    hostname[sizeof(hostname) - 1] = 0;
    while (u16(f, &family))
    {
        unsigned char address[4096], num[128], name[256], data[4096];
        size_t al, nl, kl, dl;
        int score;
        if (!field(f, address, sizeof(address), &al) || !field(f, num, sizeof(num), &nl) ||
            !field(f, name, sizeof(name), &kl) || !field(f, data, sizeof(data), &dl))
        {
            break;
        }
        if ((family != 256 && family != 65535) || quanta_strcmp((char *)num, number) ||
            quanta_strcmp((char *)name, "MIT-MAGIC-COOKIE-1") || dl > 256)
        {
            continue;
        }
        score = family == 65535 ? 1 : !quanta_strcmp((char *)address, hostname) ? 3 : 2;
        if (score > best)
        {
            quanta_memcpy(cookie, data, dl);
            *size = dl;
            best = score;
        }
    }
    quanta_fclose(f);
    return best != 0;
}

static int local_socket(int number)
{
    int fd;
    struct sockaddr_un a;
    size_t len;
    char path[108];
    if (quanta_snprintf(path, sizeof(path), "/tmp/.X11-unix/X%d", number) >= (int)sizeof(path))
    {
        return -1;
    }
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        return -1;
    }
    quanta_memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    quanta_memcpy(a.sun_path, path, quanta_strlen(path) + 1);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0)
    {
        return fd;
    }
    close(fd);
    /* Linux abstract namespace is used by some Xwayland servers. */ fd =
        socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        return -1;
    }
    quanta_memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    len = quanta_strlen(path);
    quanta_memcpy(a.sun_path + 1, path, len);
    if (connect(fd, (struct sockaddr *)&a,
                (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + len)) == 0)
    {
        return fd;
    }
    close(fd);
    return -1;
}

static xcb_atom_t atom(quanta_window *p, const char *name)
{
    xcb_intern_atom_reply_t *r =
        xcb_intern_atom_reply(p->c, xcb_intern_atom(p->c, 0, (uint16_t)quanta_strlen(name), name), NULL);
    xcb_atom_t a = r ? r->atom : XCB_ATOM_NONE;
    quanta_free(r);
    return a;
}

static int checked(quanta_window *p, xcb_void_cookie_t request)
{
    xcb_generic_error_t *e = xcb_request_check(p->c, request);
    if (e)
    {
        quanta_free(e);
        return 0;
    }
    return !xcb_connection_has_error(p->c);
}

static void keymap(quanta_window *p)
{
    const xcb_setup_t *s = xcb_get_setup(p->c);
    quanta_free(p->keymap);
    p->min_key = s->min_keycode;
    p->max_key = s->max_keycode;
    p->keymap = xcb_get_keyboard_mapping_reply(
        p->c, xcb_get_keyboard_mapping(p->c, p->min_key, (uint8_t)(p->max_key - p->min_key + 1)),
        NULL);
}

int quanta_window_open(const char *title, int width, int height, quanta_window **out, char *err,
                       size_t cap)
{
    const char *display = quanta_getenv("DISPLAY"), *num;
    char *end;
    long d, scr = 0;
    int fd, screen_index;
    quanta_window *p;
    unsigned char cookie[256];
    size_t cookie_len = 0;
    xcb_auth_info_t auth;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t it;
    uint32_t values[2];
    xcb_atom_t utf8, name;
    int format_ok = 0, visual_ok = 0;
    uint16_t endian = 1;
    *out = NULL;
    if (!display || !*display)
    {
        quanta_snprintf(err, cap, "DISPLAY is unset; GUI needs a local X11/Xwayland session");
        return 0;
    }
    num = display;
    if (!quanta_strncmp(num, "unix:", 5))
    {
        num += 4;
    }
    if (*num != ':')
    {
        quanta_snprintf(err, cap,
                 "only local DISPLAY=:N[.screen] or unix:N is supported; no "
                 "TCP/SSH X11");
        return 0;
    }
    quanta_errno = 0;
    d = quanta_strtol(num + 1, &end, 10);
    if (quanta_errno || end == num + 1 || d < 0 || d > 65535)
    {
        quanta_snprintf(err, cap, "invalid DISPLAY");
        return 0;
    }
    if (*end == '.')
    {
        char *se;
        quanta_errno = 0;
        scr = quanta_strtol(end + 1, &se, 10);
        if (quanta_errno || se == end + 1 || *se || scr < 0 || scr > 31)
        {
            quanta_snprintf(err, cap, "invalid DISPLAY screen");
            return 0;
        }
    }
    else if (*end)
    {
        quanta_snprintf(err, cap, "invalid DISPLAY suffix");
        return 0;
    }
    fd = local_socket((int)d);
    if (fd < 0)
    {
        quanta_snprintf(err, cap, "cannot connect to local X server %s: %s", display, quanta_strerror(quanta_errno));
        return 0;
    }
    p = (quanta_window *)quanta_calloc(1, sizeof(*p));
    if (!p)
    {
        close(fd);
        quanta_snprintf(err, cap, "out of memory");
        return 0;
    }
    quanta_memset(&auth, 0, sizeof(auth));
    if (auth_cookie((int)d, cookie, &cookie_len))
    {
        auth.name = (char *)"MIT-MAGIC-COOKIE-1";
        auth.namelen = 18;
        auth.data = (char *)cookie;
        auth.datalen = (int)cookie_len;
    }
    p->c = xcb_connect_to_fd(fd, auth.name ? &auth : NULL);
    quanta_memset(cookie, 0, sizeof(cookie));
    if (!p->c || xcb_connection_has_error(p->c))
    {
        quanta_snprintf(err, cap,
                 "X11 handshake/authentication failed; check DISPLAY and "
                 "XAUTHORITY (do not run "
                 "GUI via sudo)");
        quanta_window_close(p);
        return 0;
    }
    setup = xcb_get_setup(p->c);
    it = xcb_setup_roots_iterator(setup);
    for (screen_index = 0; screen_index < scr && it.rem; screen_index++)
    {
        xcb_screen_next(&it);
    }
    if (!it.rem)
    {
        quanta_snprintf(err, cap, "X11 screen does not exist");
        quanta_window_close(p);
        return 0;
    }
    p->screen = it.data;
    {
        xcb_format_iterator_t fi = xcb_setup_pixmap_formats_iterator(setup);
        for (; fi.rem; xcb_format_next(&fi))
        {
            if (fi.data->depth == 24 && fi.data->bits_per_pixel == 32 &&
                fi.data->scanline_pad == 32)
            {
                format_ok = 1;
            }
        }
        xcb_depth_iterator_t di = xcb_screen_allowed_depths_iterator(p->screen);
        for (; di.rem; xcb_depth_next(&di))
        {
            xcb_visualtype_iterator_t vi = xcb_depth_visuals_iterator(di.data);
            for (; vi.rem; xcb_visualtype_next(&vi))
            {
                xcb_visualtype_t *v = vi.data;
                if (v->visual_id == p->screen->root_visual &&
                    v->_class == XCB_VISUAL_CLASS_TRUE_COLOR && v->red_mask == 0xff0000 &&
                    v->green_mask == 0x00ff00 && v->blue_mask == 0x0000ff)
                {
                    visual_ok = 1;
                }
            }
        }
    }
    if (p->screen->root_depth != 24 || !format_ok || !visual_ok ||
        setup->image_byte_order !=
            (*(uint8_t *)&endian ? XCB_IMAGE_ORDER_LSB_FIRST : XCB_IMAGE_ORDER_MSB_FIRST))
    {
        quanta_snprintf(err, cap, "GUI requires native-endian 24-bit RGB TrueColor with 32-bit pixels");
        quanta_window_close(p);
        return 0;
    }
    p->width = width;
    p->height = height;
    p->window = xcb_generate_id(p->c);
    p->gc = xcb_generate_id(p->c);
    values[0] = 0x0c1119;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_FOCUS_CHANGE;
    if (!checked(p, xcb_create_window_checked(p->c, 24, p->window, p->screen->root, 0, 0,
                                              (uint16_t)width, (uint16_t)height, 0,
                                              XCB_WINDOW_CLASS_INPUT_OUTPUT, p->screen->root_visual,
                                              XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values)) ||
        !checked(p, xcb_create_gc_checked(p->c, p->gc, p->window, 0, NULL)))
    {
        quanta_snprintf(err, cap, "cannot create X11 window");
        quanta_window_close(p);
        return 0;
    }
    p->wm_protocols = atom(p, "WM_PROTOCOLS");
    p->wm_delete = atom(p, "WM_DELETE_WINDOW");
    utf8 = atom(p, "UTF8_STRING");
    name = atom(p, "_NET_WM_NAME");
    xcb_change_property(p->c, XCB_PROP_MODE_REPLACE, p->window, p->wm_protocols, XCB_ATOM_ATOM, 32,
                        1, &p->wm_delete);
    xcb_change_property(p->c, XCB_PROP_MODE_REPLACE, p->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                        8, (uint32_t)quanta_strlen(title), title);
    if (utf8 && name)
    {
        xcb_change_property(p->c, XCB_PROP_MODE_REPLACE, p->window, name, utf8, 8,
                            (uint32_t)quanta_strlen(title), title);
    }
    xcb_change_property(p->c, XCB_PROP_MODE_REPLACE, p->window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING,
                        8, 8, "quanta\0INV");
    keymap(p);
    xcb_map_window(p->c, p->window);
    xcb_flush(p->c);
    *out = p;
    return 1;
}

static int modifiers(uint16_t s)
{
    return ((s & XCB_MOD_MASK_SHIFT) ? QUANTA_X_SHIFT : 0) |
           ((s & XCB_MOD_MASK_CONTROL) ? QUANTA_X_CTRL : 0) |
           ((s & XCB_MOD_MASK_1) ? QUANTA_X_ALT : 0) | ((s & XCB_MOD_MASK_4) ? QUANTA_X_SUPER : 0);
}

int quanta_window_poll(quanta_window *p, quanta_window_event *out)
{
    xcb_generic_event_t *e = xcb_poll_for_event(p->c);
    uint8_t type;
    quanta_memset(out, 0, sizeof(*out));
    if (!e)
    {
        return xcb_connection_has_error(p->c) ? -1 : 0;
    }
    type = e->response_type & 127u;
    if (type == 0)
    {
        xcb_generic_error_t *error = (xcb_generic_error_t *)e;
        quanta_fprintf(quanta_stderr, "quanta: X11 protocol error %u (major %u)\n", error->error_code,
                error->major_code);
        quanta_free(e);
        return -1;
    }
    if (type == XCB_CLIENT_MESSAGE)
    {
        xcb_client_message_event_t *m = (xcb_client_message_event_t *)e;
        if (m->type == p->wm_protocols && m->data.data32[0] == p->wm_delete)
        {
            out->kind = QUANTA_X_CLOSE;
        }
    }
    else if (type == XCB_DESTROY_NOTIFY)
    {
        out->kind = QUANTA_X_CLOSE;
    }
    else if (type == XCB_CONFIGURE_NOTIFY)
    {
        xcb_configure_notify_event_t *c = (xcb_configure_notify_event_t *)e;
        if (c->window == p->window)
        {
            p->width = c->width;
            p->height = c->height;
            out->kind = QUANTA_X_RESIZE;
            out->width = c->width;
            out->height = c->height;
        }
    }
    else if (type == XCB_MOTION_NOTIFY)
    {
        xcb_motion_notify_event_t *m = (xcb_motion_notify_event_t *)e;
        out->kind = QUANTA_X_MOTION;
        out->x = m->event_x;
        out->y = m->event_y;
    }
    else if (type == XCB_BUTTON_PRESS || type == XCB_BUTTON_RELEASE)
    {
        xcb_button_press_event_t *b = (xcb_button_press_event_t *)e;
        out->pressed = type == XCB_BUTTON_PRESS;
        out->x = b->event_x;
        out->y = b->event_y;
        if (b->detail >= 4 && b->detail <= 7)
        {
            if (out->pressed)
            {
                out->kind = QUANTA_X_WHEEL;
                out->y = b->detail == 4 ? 1 : b->detail == 5 ? -1 : 0;
                out->x = b->detail == 6 ? 1 : b->detail == 7 ? -1 : 0;
            }
        }
        else if (b->detail >= 1 && b->detail <= 3)
        {
            out->kind = QUANTA_X_BUTTON;
            out->button = b->detail == 1 ? 0 : b->detail == 3 ? 1 : 2;
        }
    }
    else if (type == XCB_FOCUS_IN || type == XCB_FOCUS_OUT)
    {
        out->kind = QUANTA_X_FOCUS;
        out->pressed = type == XCB_FOCUS_IN;
    }
    else if (type == XCB_MAPPING_NOTIFY)
    {
        keymap(p);
    }
    else if (type == XCB_KEY_PRESS || type == XCB_KEY_RELEASE)
    {
        xcb_key_press_event_t *k = (xcb_key_press_event_t *)e;
        uint32_t base = 0, sym = 0;
        out->kind = QUANTA_X_KEY;
        out->pressed = type == XCB_KEY_PRESS;
        out->mods = modifiers(k->state);
        if (p->keymap && k->detail >= p->min_key && k->detail <= p->max_key)
        {
            unsigned per = p->keymap->keysyms_per_keycode;
            size_t index = (size_t)(k->detail - p->min_key) * per;
            xcb_keysym_t *syms = xcb_get_keyboard_mapping_keysyms(p->keymap);
            if (per)
            {
                base = syms[index];
                sym = base;
                if (per > 1 && (k->state & XCB_MOD_MASK_SHIFT) && syms[index + 1])
                {
                    sym = syms[index + 1];
                }
            }
            if (k->state & XCB_MOD_MASK_LOCK)
            {
                if (sym >= 'a' && sym <= 'z')
                {
                    sym -= 32;
                }
                else if (sym >= 'A' && sym <= 'Z')
                {
                    sym += 32;
                }
            }
        }
        out->keysym = base;
        if (base == 0xffe1 || base == 0xffe2)
        {
            if (out->pressed)
            {
                out->mods |= QUANTA_X_SHIFT;
            }
            else
            {
                out->mods &= ~QUANTA_X_SHIFT;
            }
        }
        if (base == 0xffe3 || base == 0xffe4)
        {
            if (out->pressed)
            {
                out->mods |= QUANTA_X_CTRL;
            }
            else
            {
                out->mods &= ~QUANTA_X_CTRL;
            }
        }
        if (base == 0xffe9 || base == 0xffea)
        {
            if (out->pressed)
            {
                out->mods |= QUANTA_X_ALT;
            }
            else
            {
                out->mods &= ~QUANTA_X_ALT;
            }
        }
        if (base == 0xffeb || base == 0xffec)
        {
            if (out->pressed)
            {
                out->mods |= QUANTA_X_SUPER;
            }
            else
            {
                out->mods &= ~QUANTA_X_SUPER;
            }
        }
        if (out->pressed && !(out->mods & (QUANTA_X_CTRL | QUANTA_X_ALT | QUANTA_X_SUPER)))
        {
            if ((sym >= 32 && sym <= 126) || (sym >= 160 && sym <= 255))
            {
                out->codepoint = sym;
            }
            else if ((sym & 0xff000000u) == 0x01000000u)
            {
                uint32_t code = sym & 0xffffffu;
                if (code <= 0x10ffffu && !(code >= 0xd800u && code <= 0xdfffu))
                {
                    out->codepoint = code;
                }
            }
        }
    }
    quanta_free(e);
    return 1;
}

int quanta_window_present(quanta_window *p, const uint32_t *pixels, int width, int height)
{
    uint32_t limit = xcb_get_maximum_request_length(p->c) * 4u;
    unsigned row_bytes, rows;
    int y;
    if (width <= 0 || height <= 0 || width != p->width || height != p->height || width > 4096 ||
        height > 4096 || limit < 64)
    {
        return 0;
    }
    row_bytes = (unsigned)width * 4u;
    rows = (limit - 64) / row_bytes;
    if (!rows)
    {
        return 0;
    }
    for (y = 0; y < height;)
    {
        unsigned n = (unsigned)(height - y);
        if (n > rows)
        {
            n = rows;
        }
        xcb_put_image(p->c, XCB_IMAGE_FORMAT_Z_PIXMAP, p->window, p->gc, (uint16_t)width,
                      (uint16_t)n, 0, (int16_t)y, 0, 24, n * row_bytes,
                      (const uint8_t *)(pixels + (size_t)y * (size_t)width));
        y += (int)n;
    }
    if (xcb_flush(p->c) <= 0)
    {
        return 0;
    }
    return !xcb_connection_has_error(p->c);
}

void quanta_window_close(quanta_window *p)
{
    if (!p)
    {
        return;
    }
    quanta_free(p->keymap);
    if (p->c)
    {
        if (p->gc)
        {
            xcb_free_gc(p->c, p->gc);
        }
        if (p->window)
        {
            xcb_destroy_window(p->c, p->window);
        }
        xcb_disconnect(p->c);
    }
    quanta_free(p);
}

void *quanta_window_native(quanta_window *w)
{
    (void)w;
    return NULL;
}

float quanta_window_scale(quanta_window *w)
{
    (void)w;
    return 1.0f;
}
