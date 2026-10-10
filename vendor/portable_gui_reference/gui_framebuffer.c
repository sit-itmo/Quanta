#include "std.h"
#include "gui_framebuffer.h"
#include "gui_os.h"

static u32 gui_min_u32(u32 a, u32 b) { return a < b ? a : b; }

int GuiFramebuffer_IsValid(const gui_framebuffer_t *fb)
{
    return fb && fb->initialized && fb->base && fb->width && fb->height && fb->pitch &&
           (fb->bpp == 16U || fb->bpp == 24U || fb->bpp == 32U);
}

void *GuiFramebuffer_GetFrontBase(const gui_framebuffer_t *fb)
{
    if (!fb)
        return NULL;
    return fb->front_base ? fb->front_base : fb->base;
}

int GuiFramebuffer_EnableBackBuffer(gui_framebuffer_t *fb)
{
    u32 bytes_per_pixel;
    u32 size;
    void *buf;
    if (!GuiFramebuffer_IsValid(fb) || fb->double_buffered)
        return fb && fb->double_buffered;
    bytes_per_pixel = fb->bpp / 8U;
    if (bytes_per_pixel == 0 || fb->pitch < fb->width * bytes_per_pixel)
        return 0;
    size = fb->pitch * fb->height;
    if (!size)
        return 0;
    buf = GuiOs_Alloc(size);
    if (!buf)
        return 0;
    memset(buf, 0, size);
    fb->front_base = fb->base;
    fb->back_buffer = buf;
    fb->back_buffer_size = size;
    fb->base = buf;
    fb->double_buffered = 1;
    return 1;
}

void GuiFramebuffer_Present(gui_framebuffer_t *fb)
{
    if (!fb || !fb->double_buffered || !fb->front_base || !fb->back_buffer || !fb->back_buffer_size)
        return;
    GuiOs_PresentFramebuffer(fb->front_base, fb->back_buffer, fb->back_buffer_size);
}


void GuiFramebuffer_PresentRect(gui_framebuffer_t *fb, u32 x, u32 y, u32 w, u32 h)
{
    u32 bpp;
    u32 x2;
    u32 y2;
    u32 bytes;
    if (!fb || !fb->double_buffered || !fb->front_base || !fb->back_buffer || !fb->back_buffer_size)
        return;
    if (!w || !h || x >= fb->width || y >= fb->height)
        return;
    bpp = fb->bpp / 8U;
    if (!bpp)
        return;
    x2 = gui_min_u32(fb->width, x + w);
    y2 = gui_min_u32(fb->height, y + h);
    if (x2 <= x || y2 <= y)
        return;
    bytes = (x2 - x) * bpp;
    for (u32 yy = y; yy < y2; ++yy) {
        void *dst = (u8 *)fb->front_base + yy * fb->pitch + x * bpp;
        const void *src = (const u8 *)fb->back_buffer + yy * fb->pitch + x * bpp;
        memcpy(dst, src, bytes);
    }
}


static u32 gui_scale_channel(u8 value, u8 bits)
{
    if (bits >= 8U)
        return (u32)value;
    if (!bits)
        return 0;
    return ((u32)value * ((1U << bits) - 1U) + 127U) / 255U;
}

u32 GuiFramebuffer_PackColor(const gui_framebuffer_t *fb, u8 r, u8 g, u8 b)
{
    u32 value = 0;
    if (!fb)
        return 0;
    if (fb->red_mask_size)
        value |= gui_scale_channel(r, fb->red_mask_size) << fb->red_shift;
    if (fb->green_mask_size)
        value |= gui_scale_channel(g, fb->green_mask_size) << fb->green_shift;
    if (fb->blue_mask_size)
        value |= gui_scale_channel(b, fb->blue_mask_size) << fb->blue_shift;
    return value;
}

void GuiFramebuffer_PutPixel(gui_framebuffer_t *fb, u32 x, u32 y, u8 r, u8 g, u8 b)
{
    u8 *dst;
    u32 value;
    if (!GuiFramebuffer_IsValid(fb) || x >= fb->width || y >= fb->height)
        return;

    dst = (u8 *)fb->base + y * fb->pitch + x * (fb->bpp / 8U);
    value = GuiFramebuffer_PackColor(fb, r, g, b);
    if (fb->bpp == 32U) {
        *(volatile u32 *)dst = value;
    } else if (fb->bpp == 24U) {
        /* VBE RGB modes normally expose byte-aligned fields. Fall back to BGR
         * if a firmware reports an unusual 24-bit mode without byte positions. */
        if ((fb->red_shift % 8U) == 0 && (fb->green_shift % 8U) == 0 && (fb->blue_shift % 8U) == 0) {
            dst[fb->red_shift / 8U] = r;
            dst[fb->green_shift / 8U] = g;
            dst[fb->blue_shift / 8U] = b;
        } else {
            dst[0] = b; dst[1] = g; dst[2] = r;
        }
    } else if (fb->bpp == 16U) {
        *(volatile u16 *)dst = (u16)value;
    }
}

void GuiFramebuffer_FillRect(gui_framebuffer_t *fb, u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b)
{
    u32 xx, yy;
    u32 x2, y2;
    u32 value;
    u32 bytes_per_pixel;
    if (!GuiFramebuffer_IsValid(fb) || !w || !h || x >= fb->width || y >= fb->height)
        return;
    x2 = gui_min_u32(fb->width, x + w);
    y2 = gui_min_u32(fb->height, y + h);
    if (x2 <= x || y2 <= y)
        return;

    value = GuiFramebuffer_PackColor(fb, r, g, b);
    bytes_per_pixel = fb->bpp / 8U;
    if (fb->bpp == 16U) {
        for (yy = y; yy < y2; ++yy) {
            u16 *row = (u16 *)((u8 *)fb->base + yy * fb->pitch + x * bytes_per_pixel);
            for (xx = x; xx < x2; ++xx)
                *row++ = (u16)value;
        }
        return;
    }
    if (fb->bpp == 32U) {
        for (yy = y; yy < y2; ++yy) {
            u32 *row = (u32 *)((u8 *)fb->base + yy * fb->pitch + x * bytes_per_pixel);
            for (xx = x; xx < x2; ++xx)
                *row++ = value;
        }
        return;
    }

    for (yy = y; yy < y2; ++yy) {
        u8 *row = (u8 *)fb->base + yy * fb->pitch + x * bytes_per_pixel;
        for (xx = x; xx < x2; ++xx) {
            if ((fb->red_shift % 8U) == 0 && (fb->green_shift % 8U) == 0 && (fb->blue_shift % 8U) == 0) {
                row[fb->red_shift / 8U] = r;
                row[fb->green_shift / 8U] = g;
                row[fb->blue_shift / 8U] = b;
            } else {
                row[0] = b; row[1] = g; row[2] = r;
            }
            row += 3U;
        }
    }
}

void GuiFramebuffer_Rect(gui_framebuffer_t *fb, u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b)
{
    u32 i;
    if (!GuiFramebuffer_IsValid(fb) || !w || !h)
        return;
    for (i = 0; i < w; ++i) {
        GuiFramebuffer_PutPixel(fb, x + i, y, r, g, b);
        if (h > 1U)
            GuiFramebuffer_PutPixel(fb, x + i, y + h - 1U, r, g, b);
    }
    for (i = 0; i < h; ++i) {
        GuiFramebuffer_PutPixel(fb, x, y + i, r, g, b);
        if (w > 1U)
            GuiFramebuffer_PutPixel(fb, x + w - 1U, y + i, r, g, b);
    }
}

static void gui_draw_fallback_char(gui_framebuffer_t *fb, u32 x, u32 y, char ch, u8 r, u8 g, u8 b, u8 br, u8 bg, u8 bb)
{
    u32 row, col;
    u8 code = (u8)ch;
    GuiFramebuffer_FillRect(fb, x, y, 8, 16, br, bg, bb);
    for (row = 2; row < 14; ++row) {
        for (col = 1; col < 7; ++col) {
            u8 bit = (u8)((code >> ((row + col) & 7U)) & 1U);
            if (bit || row == 2 || row == 13 || col == 1 || col == 6)
                GuiFramebuffer_PutPixel(fb, x + col, y + row, r, g, b);
        }
    }
}

void GuiFramebuffer_DrawChar(gui_framebuffer_t *fb, u32 x, u32 y, char ch, u8 r, u8 g, u8 b, u8 br, u8 bg, u8 bb)
{
    const u8 *glyph;
    u32 row, col;
    u32 font_h;
    if (!GuiFramebuffer_IsValid(fb))
        return;

    font_h = fb->font_height ? fb->font_height : 16U;
    if (!fb->font || font_h > 32U) {
        gui_draw_fallback_char(fb, x, y, ch, r, g, b, br, bg, bb);
        return;
    }

    glyph = fb->font + ((u32)(u8)ch) * font_h;
    GuiFramebuffer_FillRect(fb, x, y, 8, font_h, br, bg, bb);
    for (row = 0; row < font_h; ++row) {
        u8 bits = glyph[row];
        for (col = 0; col < 8U; ++col) {
            if (bits & (0x80U >> col))
                GuiFramebuffer_PutPixel(fb, x + col, y + row, r, g, b);
        }
    }
}

void GuiFramebuffer_DrawTextBg(gui_framebuffer_t *fb, u32 x, u32 y, const char *text, u8 r, u8 g, u8 b, u8 br, u8 bg, u8 bb)
{
    u32 cx = x;
    u32 font_h;
    if (!GuiFramebuffer_IsValid(fb) || !text)
        return;
    font_h = fb->font_height ? fb->font_height : 16U;
    while (*text && cx + 8U <= fb->width) {
        if (*text == '\n') {
            cx = x;
            y += font_h;
        } else {
            GuiFramebuffer_DrawChar(fb, cx, y, *text, r, g, b, br, bg, bb);
            cx += 8U;
        }
        ++text;
        if (y + font_h > fb->height)
            break;
    }
}

void GuiFramebuffer_DrawText(gui_framebuffer_t *fb, u32 x, u32 y, const char *text, u8 r, u8 g, u8 b)
{
    GuiFramebuffer_DrawTextBg(fb, x, y, text, r, g, b, 10, 10, 10);
}

static void gui_sample_panel_pixel(const gui_pixel_panel_desc_t *panel, u32 sx, u32 sy, u8 *r, u8 *g, u8 *b)
{
    const u8 *p;
    if (!panel || !panel->pixels || sx >= panel->width || sy >= panel->height) {
        *r = *g = *b = 0;
        return;
    }
    p = (const u8 *)panel->pixels + sy * panel->pitch;
    switch (panel->format) {
    case GUI_PIXEL_FORMAT_RGB888:
        p += sx * 3U; *r = p[0]; *g = p[1]; *b = p[2]; break;
    case GUI_PIXEL_FORMAT_BGR888:
        p += sx * 3U; *b = p[0]; *g = p[1]; *r = p[2]; break;
    case GUI_PIXEL_FORMAT_XRGB8888:
        p += sx * 4U; *r = p[1]; *g = p[2]; *b = p[3]; break;
    case GUI_PIXEL_FORMAT_BGRX8888:
        p += sx * 4U; *b = p[0]; *g = p[1]; *r = p[2]; break;
    case GUI_PIXEL_FORMAT_RGB565: {
        u16 v;
        p += sx * 2U;
        v = (u16)p[0] | ((u16)p[1] << 8);
        *r = (u8)((((u32)((v >> 11) & 0x1fU)) * 255U) / 31U);
        *g = (u8)((((u32)((v >> 5) & 0x3fU)) * 255U) / 63U);
        *b = (u8)((((u32)(v & 0x1fU)) * 255U) / 31U);
        break;
    }
    default:
        *r = *g = *b = 0;
        break;
    }
}

void GuiFramebuffer_BlitPixelPanel(gui_framebuffer_t *fb, u32 x, u32 y, u32 w, u32 h, const gui_pixel_panel_desc_t *panel)
{
    u32 dx, dy;
    if (!GuiFramebuffer_IsValid(fb) || !panel || !panel->pixels || !panel->width || !panel->height || !w || !h)
        return;
    for (dy = 0; dy < h; ++dy) {
        u32 sy = (dy * panel->height) / h;
        for (dx = 0; dx < w; ++dx) {
            u32 sx = (dx * panel->width) / w;
            u8 r, g, b;
            gui_sample_panel_pixel(panel, sx, sy, &r, &g, &b);
            GuiFramebuffer_PutPixel(fb, x + dx, y + dy, r, g, b);
        }
    }
}

int GuiFramebuffer_InitFromDesc(gui_framebuffer_t *fb, const gui_os_framebuffer_desc_t *desc)
{
    if (!fb || !desc || !desc->base)
        return 0;
    memset(fb, 0, sizeof(*fb));
    fb->base = desc->base;
    fb->front_base = desc->base;
    fb->width = desc->width;
    fb->height = desc->height;
    fb->pitch = desc->pitch;
    fb->bpp = desc->bpp;
    fb->red_shift = desc->red_shift;
    fb->red_mask_size = desc->red_mask_size;
    fb->green_shift = desc->green_shift;
    fb->green_mask_size = desc->green_mask_size;
    fb->blue_shift = desc->blue_shift;
    fb->blue_mask_size = desc->blue_mask_size;
    fb->reserved_shift = desc->reserved_shift;
    fb->reserved_mask_size = desc->reserved_mask_size;
    fb->mode = desc->mode;
    fb->font = desc->font;
    fb->font_height = desc->font_height;

    if (fb->bpp == 32U && (!fb->red_mask_size || !fb->green_mask_size || !fb->blue_mask_size)) {
        fb->blue_shift = 0; fb->blue_mask_size = 8;
        fb->green_shift = 8; fb->green_mask_size = 8;
        fb->red_shift = 16; fb->red_mask_size = 8;
        fb->reserved_shift = 24; fb->reserved_mask_size = 8;
    }
    if (fb->bpp == 16U && (!fb->red_mask_size || !fb->green_mask_size || !fb->blue_mask_size)) {
        fb->blue_shift = 0; fb->blue_mask_size = 5;
        fb->green_shift = 5; fb->green_mask_size = 6;
        fb->red_shift = 11; fb->red_mask_size = 5;
    }

    fb->initialized = 1;
    if (fb->font_height < 8U || fb->font_height > 32U)
        fb->font_height = 16U;
    if (!GuiFramebuffer_IsValid(fb)) {
        memset(fb, 0, sizeof(*fb));
        return 0;
    }
    return 1;
}

int GuiFramebuffer_InitFromPlatform(gui_framebuffer_t *fb)
{
    gui_os_framebuffer_desc_t desc;
    if (!fb)
        return 0;
    if (!GuiOs_AcquireFramebuffer(&desc)) {
        memset(fb, 0, sizeof(*fb));
        return 0;
    }
    return GuiFramebuffer_InitFromDesc(fb, &desc);
}

int GuiFramebuffer_InitBitos(gui_framebuffer_t *fb)
{
    return GuiFramebuffer_InitFromPlatform(fb);
}
