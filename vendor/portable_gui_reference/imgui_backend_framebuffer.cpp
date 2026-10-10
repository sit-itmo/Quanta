extern "C" {
#include "std.h"
#include "gui_framebuffer.h"
#include "gui_os.h"
}
#include "imgui.h"
#include "imgui_backend_framebuffer.h"

#ifndef BITOS_IMGUI_FB_PROFILE
#define BITOS_IMGUI_FB_PROFILE 0
#endif
#ifndef BITOS_IMGUI_FAST_FONT_MONO
#define BITOS_IMGUI_FAST_FONT_MONO 1
#endif
#ifndef BITOS_IMGUI_FAST_PANEL_OPAQUE
#define BITOS_IMGUI_FAST_PANEL_OPAQUE 1
#endif
#ifndef BITOS_IMGUI_FONT_ALPHA_THRESHOLD
#define BITOS_IMGUI_FONT_ALPHA_THRESHOLD 80U
#endif
#ifndef BITOS_IMGUI_SKIP_FULL_CLEAR
#define BITOS_IMGUI_SKIP_FULL_CLEAR 1
#endif
#ifndef BITOS_IMGUI_FULL_CLEAR_INTERVAL
#define BITOS_IMGUI_FULL_CLEAR_INTERVAL 0U
#endif
#ifndef BITOS_IMGUI_CURSOR_ERASE_PAD
#define BITOS_IMGUI_CURSOR_ERASE_PAD 32
#endif
#ifndef BITOS_IMGUI_CURSOR_OVERLAY
#define BITOS_IMGUI_CURSOR_OVERLAY 1
#endif

#define FB_EPSILON 0.01f
#define FB_I32_SAFE_MIN_F (-2147483000.0f)
#define FB_I32_SAFE_MAX_F (2147483000.0f)
#define FB_COORD_ABS_LIMIT_F (16777216.0f)

enum FbClipStatus
{
    FB_CLIP_EMPTY = 0,
    FB_CLIP_OK = 1,
    FB_CLIP_INVALID = -1
};

struct ImGuiFbTexture
{
    const unsigned char *pixels;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
    int is_panel;
};

struct FbQuad
{
    ImDrawVert tl;
    ImDrawVert tr;
    ImDrawVert br;
    ImDrawVert bl;
};

static gui_framebuffer_t *g_fb = 0;
static ImGuiFbTexture g_font_texture;
static ImTextureID g_font_tex_id = (ImTextureID)1;
static unsigned char *g_external_font_data;
static unsigned int g_external_font_data_size;
static char g_font_source[128] = "builtin-bitmap";
static unsigned char g_clear_r = 14;
static unsigned char g_clear_g = 16;
static unsigned char g_clear_b = 24;
static imgui_framebuffer_stats_t g_stats;
static int g_profile_enabled = BITOS_IMGUI_FB_PROFILE;
static int g_mouse_x;
static int g_mouse_y;
static unsigned int g_mouse_buttons;
static int g_prev_mouse_x = -4096;
static int g_prev_mouse_y = -4096;
static unsigned int g_prev_mouse_buttons;
static unsigned int g_full_clear_request = 2;
static int g_dirty_rects_enabled = 1;
static int g_dirty_x0;
static int g_dirty_y0;
static int g_dirty_x1;
static int g_dirty_y1;

#define FB_CURSOR_W 18
#define FB_CURSOR_H 26
static int g_cursor_overlay_enabled;
static int g_cursor_visible;
static int g_cursor_saved_x0;
static int g_cursor_saved_y0;
static int g_cursor_saved_x1;
static int g_cursor_saved_y1;
static unsigned char g_cursor_saved[FB_CURSOR_W * FB_CURSOR_H * 4];
static unsigned int g_cursor_overlay_moves_total;
static unsigned int g_cursor_overlay_pixels_total;
static unsigned int g_full_ui_frames_total;
static unsigned int g_cached_cursor_frames_total;

static int g_external_plane_enabled;
static gui_pixel_panel_desc_t g_external_plane_panel;
static int g_external_plane_x;
static int g_external_plane_y;
static int g_external_plane_w;
static int g_external_plane_h;

static int fb_blit_panel_rect_fast(const gui_pixel_panel_desc_t *panel, int x, int y, int w, int h);

static void fb_sync_main_viewport(void)
{
    if (!g_fb || !ImGui::GetCurrentContext())
        return;
    ImGuiIO &io = ImGui::GetIO();
    ImGuiViewport *vp = ImGui::GetMainViewport();
    io.DisplaySize = ImVec2((float)g_fb->width, (float)g_fb->height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    if (vp) {
        vp->Pos = ImVec2(0.0f, 0.0f);
        vp->Size = io.DisplaySize;
        vp->FramebufferScale = ImVec2(1.0f, 1.0f);
        vp->WorkPos = ImVec2(0.0f, 0.0f);
        vp->WorkSize = io.DisplaySize;
        vp->DpiScale = 1.0f;
    }
}


static void *imgui_fb_alloc(size_t size, void *)
{
    return GuiOs_Alloc(size);
}

static void imgui_fb_free(void *ptr, void *)
{
    GuiOs_Free(ptr);
}

static int fb_clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}


static void fb_dirty_reset(void)
{
    g_dirty_x0 = 0x7fffffff;
    g_dirty_y0 = 0x7fffffff;
    g_dirty_x1 = 0;
    g_dirty_y1 = 0;
}

static void fb_dirty_add(int x0, int y0, int x1, int y1)
{
    if (!g_fb || !g_dirty_rects_enabled)
        return;
    x0 = fb_clamp_i(x0, 0, (int)g_fb->width);
    y0 = fb_clamp_i(y0, 0, (int)g_fb->height);
    x1 = fb_clamp_i(x1, 0, (int)g_fb->width);
    y1 = fb_clamp_i(y1, 0, (int)g_fb->height);
    if (x1 <= x0 || y1 <= y0)
        return;
    if (x0 < g_dirty_x0) g_dirty_x0 = x0;
    if (y0 < g_dirty_y0) g_dirty_y0 = y0;
    if (x1 > g_dirty_x1) g_dirty_x1 = x1;
    if (y1 > g_dirty_y1) g_dirty_y1 = y1;
}

static unsigned int fb_dirty_area(void)
{
    if (g_dirty_x1 <= g_dirty_x0 || g_dirty_y1 <= g_dirty_y0)
        return 0;
    return (unsigned int)(g_dirty_x1 - g_dirty_x0) * (unsigned int)(g_dirty_y1 - g_dirty_y0);
}

static unsigned int fb_float_bits(float v)
{
    union { float f; unsigned int u; } x;
    x.f = v;
    return x.u;
}

static int fb_float_is_finite(float v)
{
    return (fb_float_bits(v) & 0x7f800000U) != 0x7f800000U;
}

static int fb_float_is_s32_safe(float v)
{
    return fb_float_is_finite(v) && v > FB_I32_SAFE_MIN_F && v < FB_I32_SAFE_MAX_F;
}

static int fb_float_is_coord_safe(float v)
{
    return fb_float_is_finite(v) && v > -FB_COORD_ABS_LIMIT_F && v < FB_COORD_ABS_LIMIT_F;
}

static int fb_floor_i_checked(float v, int *out)
{
    int i;
    if (!out || !fb_float_is_s32_safe(v))
        return 0;
    i = (int)v;
    if ((float)i > v)
        --i;
    *out = i;
    return 1;
}

static int fb_ceil_i_checked(float v, int *out)
{
    int i;
    if (!out || !fb_float_is_s32_safe(v))
        return 0;
    i = (int)v;
    if ((float)i < v)
        ++i;
    *out = i;
    return 1;
}

static int fb_trunc_i_checked(float v, int *out)
{
    if (!out || !fb_float_is_s32_safe(v))
        return 0;
    *out = (int)v;
    return 1;
}

static int fb_fixed16_checked(float v, int *out)
{
    if (!out || !fb_float_is_finite(v) || v <= FB_I32_SAFE_MIN_F || v >= FB_I32_SAFE_MAX_F)
        return 0;
    *out = (int)v;
    return 1;
}


static float fb_min_f(float a, float b)
{
    return a < b ? a : b;
}

static float fb_max_f(float a, float b)
{
    return a > b ? a : b;
}

static int fb_float_near(float a, float b)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= FB_EPSILON;
}


static unsigned char fb_float_to_u8(float v)
{
    int i;
    if (!fb_float_is_finite(v)) return 0;
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    i = (int)(v + 0.5f);
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return (unsigned char)i;
}

static unsigned char fb_col_r(unsigned int c) { return (unsigned char)((c >> IM_COL32_R_SHIFT) & 0xffU); }
static unsigned char fb_col_g(unsigned int c) { return (unsigned char)((c >> IM_COL32_G_SHIFT) & 0xffU); }
static unsigned char fb_col_b(unsigned int c) { return (unsigned char)((c >> IM_COL32_B_SHIFT) & 0xffU); }
static unsigned char fb_col_a(unsigned int c) { return (unsigned char)((c >> IM_COL32_A_SHIFT) & 0xffU); }

static unsigned int fb_div255(unsigned int v)
{
    v += 128U;
    return (v + (v >> 8)) >> 8;
}

static unsigned char fb_blend_u8(unsigned int src, unsigned int dst, unsigned int a)
{
    int diff = (int)src - (int)dst;
    int value = (int)dst + ((diff * (int)a + (diff >= 0 ? 127 : -127)) / 255);
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    return (unsigned char)value;
}

static int fb_is_rgb888_32_direct(const gui_framebuffer_t *fb)
{
    return fb && fb->bpp == 32U &&
           fb->red_mask_size == 8U && fb->green_mask_size == 8U && fb->blue_mask_size == 8U &&
           fb->red_shift < 32U && fb->green_shift < 32U && fb->blue_shift < 32U;
}

static unsigned int fb_pack_888_32_direct(const gui_framebuffer_t *fb, unsigned char r, unsigned char g, unsigned char b)
{
    return ((unsigned int)r << fb->red_shift) |
           ((unsigned int)g << fb->green_shift) |
           ((unsigned int)b << fb->blue_shift);
}

static unsigned int fb_blend_pack_888_32_direct(const gui_framebuffer_t *fb, unsigned int oldv, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned int dr = (oldv >> fb->red_shift) & 0xffU;
    unsigned int dg = (oldv >> fb->green_shift) & 0xffU;
    unsigned int db = (oldv >> fb->blue_shift) & 0xffU;
    return ((unsigned int)fb_blend_u8(r, dr, a) << fb->red_shift) |
           ((unsigned int)fb_blend_u8(g, dg, a) << fb->green_shift) |
           ((unsigned int)fb_blend_u8(b, db, a) << fb->blue_shift);
}

static unsigned int fb_mask_for_bits(unsigned int bits)
{
    if (!bits) return 0;
    if (bits >= 32U) return 0xffffffffU;
    return (1U << bits) - 1U;
}

static unsigned int fb_pack_color_fast(const gui_framebuffer_t *fb, unsigned char r, unsigned char g, unsigned char b)
{
    unsigned int value = 0;
    if (!fb)
        return 0;
    if (fb->red_mask_size)
        value |= (((unsigned int)r * fb_mask_for_bits(fb->red_mask_size) + 127U) / 255U) << fb->red_shift;
    if (fb->green_mask_size)
        value |= (((unsigned int)g * fb_mask_for_bits(fb->green_mask_size) + 127U) / 255U) << fb->green_shift;
    if (fb->blue_mask_size)
        value |= (((unsigned int)b * fb_mask_for_bits(fb->blue_mask_size) + 127U) / 255U) << fb->blue_shift;
    return value;
}


static int fb_cursor_can_overlay(void)
{
#if BITOS_IMGUI_CURSOR_OVERLAY
    return g_fb && g_cursor_overlay_enabled && fb_is_rgb888_32_direct(g_fb) && GuiFramebuffer_GetFrontBase(g_fb);
#else
    return 0;
#endif
}

static unsigned int fb_cursor_pack(unsigned char r, unsigned char g, unsigned char b)
{
    return fb_pack_888_32_direct(g_fb, r, g, b);
}

static void fb_cursor_restore(void)
{
    if (!fb_cursor_can_overlay() || !g_cursor_visible)
        return;
    unsigned char *front = (unsigned char *)GuiFramebuffer_GetFrontBase(g_fb);
    int w = g_cursor_saved_x1 - g_cursor_saved_x0;
    int h = g_cursor_saved_y1 - g_cursor_saved_y0;
    if (!front || w <= 0 || h <= 0) {
        g_cursor_visible = 0;
        return;
    }
    for (int y = 0; y < h; ++y) {
        unsigned char *dst = front + (unsigned int)(g_cursor_saved_y0 + y) * g_fb->pitch + (unsigned int)g_cursor_saved_x0 * 4U;
        const unsigned char *src = g_cursor_saved + (unsigned int)y * FB_CURSOR_W * 4U;
        memcpy(dst, src, (unsigned int)w * 4U);
    }
    g_cursor_visible = 0;
}

static void fb_cursor_save_rect(int x0, int y0, int x1, int y1)
{
    unsigned char *front;
    int w, h;
    if (!fb_cursor_can_overlay())
        return;
    x0 = fb_clamp_i(x0, 0, (int)g_fb->width);
    y0 = fb_clamp_i(y0, 0, (int)g_fb->height);
    x1 = fb_clamp_i(x1, 0, (int)g_fb->width);
    y1 = fb_clamp_i(y1, 0, (int)g_fb->height);
    w = x1 - x0;
    h = y1 - y0;
    if (w <= 0 || h <= 0 || w > FB_CURSOR_W || h > FB_CURSOR_H)
        return;
    front = (unsigned char *)GuiFramebuffer_GetFrontBase(g_fb);
    for (int y = 0; y < h; ++y) {
        const unsigned char *src = front + (unsigned int)(y0 + y) * g_fb->pitch + (unsigned int)x0 * 4U;
        unsigned char *dst = g_cursor_saved + (unsigned int)y * FB_CURSOR_W * 4U;
        memcpy(dst, src, (unsigned int)w * 4U);
    }
    g_cursor_saved_x0 = x0;
    g_cursor_saved_y0 = y0;
    g_cursor_saved_x1 = x1;
    g_cursor_saved_y1 = y1;
}

static void fb_cursor_front_pixel(int x, int y, unsigned int color)
{
    unsigned char *front;
    if (!g_fb || x < 0 || y < 0 || x >= (int)g_fb->width || y >= (int)g_fb->height)
        return;
    front = (unsigned char *)GuiFramebuffer_GetFrontBase(g_fb);
    if (!front)
        return;
    *(unsigned int *)(front + (unsigned int)y * g_fb->pitch + (unsigned int)x * 4U) = color;
}

static void fb_cursor_draw_pattern(int mx, int my, unsigned int fg, unsigned int bg)
{
    static const char *rows[FB_CURSOR_H] = {
        "X                 ",
        "XX                ",
        "X.X               ",
        "X..X              ",
        "X...X             ",
        "X....X            ",
        "X.....X           ",
        "X......X          ",
        "X.......X         ",
        "X........X        ",
        "X.....XXXXX       ",
        "X..X..X           ",
        "X.X X..X          ",
        "XX  X..X          ",
        "X    X..X         ",
        "     X..X         ",
        "      XX          ",
        "                  ",
        "                  ",
        "                  ",
        "                  ",
        "                  ",
        "                  ",
        "                  ",
        "                  ",
        "                  "
    };
    for (int y = 0; y < FB_CURSOR_H; ++y) {
        for (int x = 0; x < FB_CURSOR_W; ++x) {
            if (rows[y][x] == 'X') {
                for (int oy = -1; oy <= 1; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                        fb_cursor_front_pixel(mx + x + ox, my + y + oy, bg);
            }
        }
    }
    for (int y = 0; y < FB_CURSOR_H; ++y)
        for (int x = 0; x < FB_CURSOR_W; ++x)
            if (rows[y][x] == 'X')
                fb_cursor_front_pixel(mx + x, my + y, fg);
}

static void fb_cursor_show(int mx, int my)
{
    int x0, y0, x1, y1;
    if (!fb_cursor_can_overlay())
        return;
    fb_cursor_restore();
    x0 = mx;
    y0 = my;
    x1 = mx + FB_CURSOR_W;
    y1 = my + FB_CURSOR_H;
    fb_cursor_save_rect(x0, y0, x1, y1);
    if (g_cursor_saved_x1 <= g_cursor_saved_x0 || g_cursor_saved_y1 <= g_cursor_saved_y0)
        return;
    fb_cursor_draw_pattern(mx, my, fb_cursor_pack(255, 255, 255), fb_cursor_pack(0, 0, 0));
    g_cursor_visible = 1;
    g_cursor_overlay_moves_total++;
    g_cursor_overlay_pixels_total += (unsigned int)((g_cursor_saved_x1 - g_cursor_saved_x0) * (g_cursor_saved_y1 - g_cursor_saved_y0));
}

static void fb_unpack_color_fast(const gui_framebuffer_t *fb, unsigned int value, unsigned char *r, unsigned char *g, unsigned char *b)
{
    unsigned int mask;
    if (!fb || !r || !g || !b)
        return;
    mask = fb_mask_for_bits(fb->red_mask_size);
    *r = mask ? (unsigned char)((((value >> fb->red_shift) & mask) * 255U) / mask) : 0;
    mask = fb_mask_for_bits(fb->green_mask_size);
    *g = mask ? (unsigned char)((((value >> fb->green_shift) & mask) * 255U) / mask) : 0;
    mask = fb_mask_for_bits(fb->blue_mask_size);
    *b = mask ? (unsigned char)((((value >> fb->blue_shift) & mask) * 255U) / mask) : 0;
}

static int fb_is_rgb888_32(const gui_framebuffer_t *fb)
{
    return fb && fb->bpp == 32U &&
           fb->red_mask_size == 8U &&
           fb->green_mask_size == 8U &&
           fb->blue_mask_size == 8U;
}

static unsigned int fb_pack_888_32(const gui_framebuffer_t *fb, unsigned char r, unsigned char g, unsigned char b)
{
    return ((unsigned int)r << fb->red_shift) |
           ((unsigned int)g << fb->green_shift) |
           ((unsigned int)b << fb->blue_shift);
}

static void fb_write_pixel(gui_framebuffer_t *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    unsigned char *dst;
    unsigned int v;
    if (!fb || x < 0 || y < 0 || x >= (int)fb->width || y >= (int)fb->height)
        return;
    dst = (unsigned char *)fb->base + (unsigned int)y * fb->pitch + (unsigned int)x * (fb->bpp / 8U);
    v = fb_pack_color_fast(fb, r, g, b);
    if (fb->bpp == 32U)
        *(unsigned int *)dst = v;
    else if (fb->bpp == 16U)
        *(unsigned short *)dst = (unsigned short)v;
    else if (fb->bpp == 24U) {
        if ((fb->red_shift % 8U) == 0 && (fb->green_shift % 8U) == 0 && (fb->blue_shift % 8U) == 0) {
            dst[fb->red_shift / 8U] = r;
            dst[fb->green_shift / 8U] = g;
            dst[fb->blue_shift / 8U] = b;
        } else {
            dst[0] = b; dst[1] = g; dst[2] = r;
        }
    }
}

static void fb_blend_pixel(gui_framebuffer_t *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned char *dst;
    unsigned char dr = 0, dg = 0, db = 0;
    unsigned int oldv;
    unsigned int ia;
    if (!fb || x < 0 || y < 0 || x >= (int)fb->width || y >= (int)fb->height || a == 0)
        return;
    dst = (unsigned char *)fb->base + (unsigned int)y * fb->pitch + (unsigned int)x * (fb->bpp / 8U);
    if (fb->bpp == 32U && fb_is_rgb888_32_direct(fb)) {
        if (a == 255U)
            *(unsigned int *)dst = fb_pack_888_32_direct(fb, r, g, b);
        else
            *(unsigned int *)dst = fb_blend_pack_888_32_direct(fb, *(unsigned int *)dst, r, g, b, a);
        g_stats.blended_pixels++;
        return;
    }
    if (a == 255) {
        fb_write_pixel(fb, x, y, r, g, b);
        g_stats.blended_pixels++;
        return;
    }
    if (fb->bpp == 32U) {
        oldv = *(unsigned int *)dst;
        if (fb->red_mask_size == 8U && fb->green_mask_size == 8U && fb->blue_mask_size == 8U) {
            dr = (unsigned char)((oldv >> fb->red_shift) & 0xffU);
            dg = (unsigned char)((oldv >> fb->green_shift) & 0xffU);
            db = (unsigned char)((oldv >> fb->blue_shift) & 0xffU);
        } else {
            fb_unpack_color_fast(fb, oldv, &dr, &dg, &db);
        }
        ia = 255U - a;
        r = (unsigned char)fb_div255((unsigned int)r * a + (unsigned int)dr * ia);
        g = (unsigned char)fb_div255((unsigned int)g * a + (unsigned int)dg * ia);
        b = (unsigned char)fb_div255((unsigned int)b * a + (unsigned int)db * ia);
        if (fb_is_rgb888_32(fb))
            *(unsigned int *)dst = fb_pack_888_32(fb, r, g, b);
        else
            *(unsigned int *)dst = fb_pack_color_fast(fb, r, g, b);
    } else if (fb->bpp == 16U) {
        oldv = *(unsigned short *)dst;
        fb_unpack_color_fast(fb, oldv, &dr, &dg, &db);
        ia = 255U - a;
        r = (unsigned char)fb_div255((unsigned int)r * a + (unsigned int)dr * ia);
        g = (unsigned char)fb_div255((unsigned int)g * a + (unsigned int)dg * ia);
        b = (unsigned char)fb_div255((unsigned int)b * a + (unsigned int)db * ia);
        *(unsigned short *)dst = (unsigned short)fb_pack_color_fast(fb, r, g, b);
    } else if (fb->bpp == 24U) {
        if ((fb->red_shift % 8U) == 0 && (fb->green_shift % 8U) == 0 && (fb->blue_shift % 8U) == 0) {
            dr = dst[fb->red_shift / 8U];
            dg = dst[fb->green_shift / 8U];
            db = dst[fb->blue_shift / 8U];
        } else {
            db = dst[0]; dg = dst[1]; dr = dst[2];
        }
        ia = 255U - a;
        r = (unsigned char)fb_div255((unsigned int)r * a + (unsigned int)dr * ia);
        g = (unsigned char)fb_div255((unsigned int)g * a + (unsigned int)dg * ia);
        b = (unsigned char)fb_div255((unsigned int)b * a + (unsigned int)db * ia);
        if ((fb->red_shift % 8U) == 0 && (fb->green_shift % 8U) == 0 && (fb->blue_shift % 8U) == 0) {
            dst[fb->red_shift / 8U] = r;
            dst[fb->green_shift / 8U] = g;
            dst[fb->blue_shift / 8U] = b;
        } else {
            dst[0] = b; dst[1] = g; dst[2] = r;
        }
    }
    g_stats.blended_pixels++;
}

static void fb_fill_u32_row(unsigned int *row, unsigned int value, unsigned int count)
{
    unsigned int i = 0;
    for (; i + 8U <= count; i += 8U) {
        row[i + 0U] = value; row[i + 1U] = value; row[i + 2U] = value; row[i + 3U] = value;
        row[i + 4U] = value; row[i + 5U] = value; row[i + 6U] = value; row[i + 7U] = value;
    }
    for (; i < count; ++i)
        row[i] = value;
}

static void fb_fill_u16_row(unsigned short *row, unsigned short value, unsigned int count)
{
    unsigned int i = 0;
    for (; i + 8U <= count; i += 8U) {
        row[i + 0U] = value; row[i + 1U] = value; row[i + 2U] = value; row[i + 3U] = value;
        row[i + 4U] = value; row[i + 5U] = value; row[i + 6U] = value; row[i + 7U] = value;
    }
    for (; i < count; ++i)
        row[i] = value;
}

static void fb_blend_rect32_direct(gui_framebuffer_t *fb, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (!fb || !fb_is_rgb888_32_direct(fb) || a == 0)
        return;
    for (int y = y0; y < y1; ++y) {
        unsigned int *row = (unsigned int *)((unsigned char *)fb->base + (unsigned int)y * fb->pitch + (unsigned int)x0 * 4U);
        for (int x = x0; x < x1; ++x) {
            *row = fb_blend_pack_888_32_direct(fb, *row, r, g, b, a);
            ++row;
        }
    }
    g_stats.blended_pixels += (unsigned int)((x1 - x0) * (y1 - y0));
}

static void fb_fill_rect_fast(gui_framebuffer_t *fb, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned int area;
    if (!fb || a == 0)
        return;
    x0 = fb_clamp_i(x0, 0, (int)fb->width);
    y0 = fb_clamp_i(y0, 0, (int)fb->height);
    x1 = fb_clamp_i(x1, 0, (int)fb->width);
    y1 = fb_clamp_i(y1, 0, (int)fb->height);
    if (x1 <= x0 || y1 <= y0)
        return;
    area = (unsigned int)((x1 - x0) * (y1 - y0));
    g_stats.solid_rects++;
    if (a == 255 && fb->bpp == 32U) {
        unsigned int value = fb_is_rgb888_32(fb) ? fb_pack_888_32(fb, r, g, b) : fb_pack_color_fast(fb, r, g, b);
        unsigned int width = (unsigned int)(x1 - x0);
        unsigned int bytes = width * 4U;
        unsigned int *first = (unsigned int *)((unsigned char *)fb->base + (unsigned int)y0 * fb->pitch + (unsigned int)x0 * 4U);
        fb_fill_u32_row(first, value, width);
        for (int y = y0 + 1; y < y1; ++y) {
            unsigned int *row = (unsigned int *)((unsigned char *)fb->base + (unsigned int)y * fb->pitch + (unsigned int)x0 * 4U);
            memcpy(row, first, bytes);
        }
        g_stats.clear_pixels += area;
        return;
    }
    if (a == 255 && fb->bpp == 16U) {
        unsigned short value = (unsigned short)fb_pack_color_fast(fb, r, g, b);
        unsigned int width = (unsigned int)(x1 - x0);
        unsigned int bytes = width * 2U;
        unsigned short *first = (unsigned short *)((unsigned char *)fb->base + (unsigned int)y0 * fb->pitch + (unsigned int)x0 * 2U);
        fb_fill_u16_row(first, value, width);
        for (int y = y0 + 1; y < y1; ++y) {
            unsigned short *row = (unsigned short *)((unsigned char *)fb->base + (unsigned int)y * fb->pitch + (unsigned int)x0 * 2U);
            memcpy(row, first, bytes);
        }
        g_stats.clear_pixels += area;
        return;
    }
    if (a != 255 && fb->bpp == 32U && fb_is_rgb888_32_direct(fb)) {
        fb_blend_rect32_direct(fb, x0, y0, x1, y1, r, g, b, a);
        return;
    }
    if (a == 255) {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                fb_write_pixel(fb, x, y, r, g, b);
        g_stats.clear_pixels += area;
        return;
    }
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            fb_blend_pixel(fb, x, y, r, g, b, a);
}

static void fb_clear_fast(gui_framebuffer_t *fb)
{
    if (!fb)
        return;
    fb_fill_rect_fast(fb, 0, 0, (int)fb->width, (int)fb->height, g_clear_r, g_clear_g, g_clear_b, 255);
}

static void fb_sample_font(float u, float v, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a)
{
    int x, y;
    const unsigned char *p;
    if (!g_font_texture.pixels || g_font_texture.width <= 0 || g_font_texture.height <= 0) {
        *r = *g = *b = *a = 255;
        return;
    }
    if (!fb_float_is_finite(u) || !fb_float_is_finite(v)) {
        *r = *g = *b = 255;
        *a = 0;
        return;
    }
    if (!fb_trunc_i_checked(u * (float)g_font_texture.width, &x) ||
        !fb_trunc_i_checked(v * (float)g_font_texture.height, &y)) {
        *r = *g = *b = 255;
        *a = 0;
        return;
    }
    x = fb_clamp_i(x, 0, g_font_texture.width - 1);
    y = fb_clamp_i(y, 0, g_font_texture.height - 1);
    p = g_font_texture.pixels + y * g_font_texture.pitch + x * g_font_texture.bytes_per_pixel;
    if (g_font_texture.bytes_per_pixel == 1) {
        *r = *g = *b = 255;
        *a = p[0];
    } else if (g_font_texture.bytes_per_pixel >= 4) {
        *r = p[0]; *g = p[1]; *b = p[2]; *a = p[3];
    } else {
        *r = *g = *b = *a = p[0];
    }
}

static void fb_sample_panel(const gui_pixel_panel_desc_t *panel, float u, float v, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a)
{
    unsigned int x, y;
    const unsigned char *p;
    *a = 255;
    if (!panel || !panel->pixels || !panel->width || !panel->height) {
        *r = *g = *b = 0;
        return;
    }
    if (!fb_float_is_finite(u) || !fb_float_is_finite(v)) {
        *r = *g = *b = 0;
        *a = 0;
        return;
    }
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    x = (unsigned int)(u * (float)(panel->width - 1U));
    y = (unsigned int)(v * (float)(panel->height - 1U));
    p = (const unsigned char *)panel->pixels + y * panel->pitch;
    switch (panel->format) {
    case GUI_PIXEL_FORMAT_RGB888:
        p += x * 3U; *r = p[0]; *g = p[1]; *b = p[2]; break;
    case GUI_PIXEL_FORMAT_BGR888:
        p += x * 3U; *b = p[0]; *g = p[1]; *r = p[2]; break;
    case GUI_PIXEL_FORMAT_XRGB8888:
        p += x * 4U; *r = p[1]; *g = p[2]; *b = p[3]; break;
    case GUI_PIXEL_FORMAT_BGRX8888:
        p += x * 4U; *b = p[0]; *g = p[1]; *r = p[2]; break;
    case GUI_PIXEL_FORMAT_RGB565: {
        unsigned int vv;
        p += x * 2U;
        vv = (unsigned int)p[0] | ((unsigned int)p[1] << 8);
        *r = (unsigned char)((((vv >> 11) & 31U) * 255U) / 31U);
        *g = (unsigned char)((((vv >> 5) & 63U) * 255U) / 63U);
        *b = (unsigned char)(((vv & 31U) * 255U) / 31U);
        break;
    }
    default:
        *r = *g = *b = 0;
        break;
    }
}


static int fb_vertex_is_valid(const ImDrawVert &v)
{
    return fb_float_is_coord_safe(v.pos.x) && fb_float_is_coord_safe(v.pos.y) &&
           fb_float_is_finite(v.uv.x) && fb_float_is_finite(v.uv.y);
}

static int fb_quad_vertices_are_valid(const FbQuad *q)
{
    return q && fb_vertex_is_valid(q->tl) && fb_vertex_is_valid(q->tr) &&
           fb_vertex_is_valid(q->br) && fb_vertex_is_valid(q->bl);
}

static int fb_draw_index_valid(const ImDrawIdx *idx_buffer, unsigned int idx_size,
                               unsigned int idx_offset, unsigned int elem_offset,
                               unsigned int vtx_offset, unsigned int vtx_size, unsigned int *out)
{
    unsigned int absolute_index;
    unsigned int value;
    if (!idx_buffer || !out)
        return 0;
    if (elem_offset >= idx_size || idx_offset > idx_size - elem_offset - 1U)
        return 0;
    absolute_index = idx_offset + elem_offset;
    value = (unsigned int)idx_buffer[absolute_index] + vtx_offset;
    if (value < vtx_offset || value >= vtx_size)
        return 0;
    *out = value;
    return 1;
}

static int fb_make_clip_rect_checked(const ImDrawCmd *pcmd, const ImDrawData *draw_data,
                                     int *clip_x1, int *clip_y1, int *clip_x2, int *clip_y2)
{
    float x1f, y1f, x2f, y2f;
    int x1, y1, x2, y2;
    if (!pcmd || !draw_data || !g_fb || !clip_x1 || !clip_y1 || !clip_x2 || !clip_y2)
        return FB_CLIP_INVALID;
    x1f = pcmd->ClipRect.x - draw_data->DisplayPos.x;
    y1f = pcmd->ClipRect.y - draw_data->DisplayPos.y;
    x2f = pcmd->ClipRect.z - draw_data->DisplayPos.x;
    y2f = pcmd->ClipRect.w - draw_data->DisplayPos.y;
    if (!fb_float_is_coord_safe(x1f) || !fb_float_is_coord_safe(y1f) ||
        !fb_float_is_coord_safe(x2f) || !fb_float_is_coord_safe(y2f))
        return FB_CLIP_INVALID;
    if (!fb_floor_i_checked(x1f, &x1) || !fb_floor_i_checked(y1f, &y1) ||
        !fb_ceil_i_checked(x2f, &x2) || !fb_ceil_i_checked(y2f, &y2))
        return FB_CLIP_INVALID;
    x1 = fb_clamp_i(x1, 0, (int)g_fb->width);
    y1 = fb_clamp_i(y1, 0, (int)g_fb->height);
    x2 = fb_clamp_i(x2, 0, (int)g_fb->width);
    y2 = fb_clamp_i(y2, 0, (int)g_fb->height);
    *clip_x1 = x1;
    *clip_y1 = y1;
    *clip_x2 = x2;
    *clip_y2 = y2;
    return (x2 > x1 && y2 > y1) ? FB_CLIP_OK : FB_CLIP_EMPTY;
}

static void fb_render_triangle(const ImDrawVert &v0, const ImDrawVert &v1, const ImDrawVert &v2,
                               int clip_x1, int clip_y1, int clip_x2, int clip_y2, ImTextureID tex_id)
{
    float minx, miny, maxx, maxy;
    int tx0, ty0, tx1, ty1;
    int x0, y0, x1, y1;
    float area;
    const gui_pixel_panel_desc_t *panel = 0;
    if (!g_fb)
        return;
    if (!fb_vertex_is_valid(v0) || !fb_vertex_is_valid(v1) || !fb_vertex_is_valid(v2)) {
        g_stats.invalid_vertex_primitives++;
        return;
    }
    minx = fb_min_f(v0.pos.x, fb_min_f(v1.pos.x, v2.pos.x));
    miny = fb_min_f(v0.pos.y, fb_min_f(v1.pos.y, v2.pos.y));
    maxx = fb_max_f(v0.pos.x, fb_max_f(v1.pos.x, v2.pos.x));
    maxy = fb_max_f(v0.pos.y, fb_max_f(v1.pos.y, v2.pos.y));
    if (!fb_floor_i_checked(minx, &tx0) || !fb_floor_i_checked(miny, &ty0) ||
        !fb_ceil_i_checked(maxx, &tx1) || !fb_ceil_i_checked(maxy, &ty1)) {
        g_stats.invalid_vertex_primitives++;
        return;
    }
    x0 = fb_clamp_i(tx0, clip_x1, clip_x2);
    y0 = fb_clamp_i(ty0, clip_y1, clip_y2);
    x1 = fb_clamp_i(tx1, clip_x1, clip_x2);
    y1 = fb_clamp_i(ty1, clip_y1, clip_y2);
    if (x1 <= x0 || y1 <= y0)
        return;
    area = (v1.pos.x - v0.pos.x) * (v2.pos.y - v0.pos.y) - (v1.pos.y - v0.pos.y) * (v2.pos.x - v0.pos.x);
    if (!fb_float_is_finite(area) || (area > -0.00001f && area < 0.00001f))
        return;
    if (tex_id != g_font_tex_id && tex_id != (ImTextureID)0)
        panel = (const gui_pixel_panel_desc_t *)(uintptr_t)tex_id;

    fb_dirty_add(x0, y0, x1, y1);
    g_stats.fallback_triangles++;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float w0 = ((v1.pos.x - px) * (v2.pos.y - py) - (v1.pos.y - py) * (v2.pos.x - px)) / area;
            float w1 = ((v2.pos.x - px) * (v0.pos.y - py) - (v2.pos.y - py) * (v0.pos.x - px)) / area;
            float w2 = 1.0f - w0 - w1;
            if (w0 < -0.0001f || w1 < -0.0001f || w2 < -0.0001f)
                continue;
            float u = w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x;
            float v = w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y;
            unsigned int c0 = v0.col, c1 = v1.col, c2 = v2.col;
            unsigned char tr, tg, tb, ta;
            unsigned char cr = fb_float_to_u8(w0 * (float)fb_col_r(c0) + w1 * (float)fb_col_r(c1) + w2 * (float)fb_col_r(c2));
            unsigned char cg = fb_float_to_u8(w0 * (float)fb_col_g(c0) + w1 * (float)fb_col_g(c1) + w2 * (float)fb_col_g(c2));
            unsigned char cb = fb_float_to_u8(w0 * (float)fb_col_b(c0) + w1 * (float)fb_col_b(c1) + w2 * (float)fb_col_b(c2));
            unsigned char ca = fb_float_to_u8(w0 * (float)fb_col_a(c0) + w1 * (float)fb_col_a(c1) + w2 * (float)fb_col_a(c2));
            if (panel)
                fb_sample_panel(panel, u, v, &tr, &tg, &tb, &ta);
            else
                fb_sample_font(u, v, &tr, &tg, &tb, &ta);
            cr = (unsigned char)(((unsigned int)cr * tr) / 255U);
            cg = (unsigned char)(((unsigned int)cg * tg) / 255U);
            cb = (unsigned char)(((unsigned int)cb * tb) / 255U);
            ca = (unsigned char)(((unsigned int)ca * ta) / 255U);
            fb_blend_pixel(g_fb, x, y, cr, cg, cb, ca);
            g_stats.fallback_pixels++;
        }
    }
}

static int fb_assign_quad_corner(const ImDrawVert &v, float minx, float miny, float maxx, float maxy, FbQuad *q, unsigned int *mask)
{
    unsigned int bit = 0;
    if (fb_float_near(v.pos.x, minx) && fb_float_near(v.pos.y, miny)) { q->tl = v; bit = 1U; }
    else if (fb_float_near(v.pos.x, maxx) && fb_float_near(v.pos.y, miny)) { q->tr = v; bit = 2U; }
    else if (fb_float_near(v.pos.x, maxx) && fb_float_near(v.pos.y, maxy)) { q->br = v; bit = 4U; }
    else if (fb_float_near(v.pos.x, minx) && fb_float_near(v.pos.y, maxy)) { q->bl = v; bit = 8U; }
    else return 0;
    if (*mask & bit)
        return 0;
    *mask |= bit;
    return 1;
}

static int fb_try_make_quad_direct(const ImDrawVert *vtx_buffer, unsigned int vtx_size, const ImDrawIdx *idx, unsigned int vtx_offset, FbQuad *out)
{
    unsigned int i0, i1, i2, i3, i4, i5;
    const ImDrawVert *a;
    const ImDrawVert *b;
    const ImDrawVert *c;
    const ImDrawVert *d;
    if (!vtx_buffer || !idx || !out)
        return 0;
    i0 = (unsigned int)idx[0] + vtx_offset;
    i1 = (unsigned int)idx[1] + vtx_offset;
    i2 = (unsigned int)idx[2] + vtx_offset;
    i3 = (unsigned int)idx[3] + vtx_offset;
    i4 = (unsigned int)idx[4] + vtx_offset;
    i5 = (unsigned int)idx[5] + vtx_offset;
    if (i0 < vtx_offset || i1 < vtx_offset || i2 < vtx_offset ||
        i3 < vtx_offset || i4 < vtx_offset || i5 < vtx_offset ||
        i0 >= vtx_size || i1 >= vtx_size || i2 >= vtx_size ||
        i3 >= vtx_size || i4 >= vtx_size || i5 >= vtx_size)
        return 0;
    if (!(i3 == i0 && i4 == i2 && i1 == i0 + 1U && i2 == i0 + 2U && i5 == i0 + 3U))
        return 0;
    a = &vtx_buffer[i0];
    b = &vtx_buffer[i1];
    c = &vtx_buffer[i2];
    d = &vtx_buffer[i5];
    if (!fb_vertex_is_valid(*a) || !fb_vertex_is_valid(*b) || !fb_vertex_is_valid(*c) || !fb_vertex_is_valid(*d))
        return 0;
    if (a->col != b->col || a->col != c->col || a->col != d->col)
        return 0;
    if (!fb_float_near(a->pos.y, b->pos.y) || !fb_float_near(d->pos.y, c->pos.y) ||
        !fb_float_near(a->pos.x, d->pos.x) || !fb_float_near(b->pos.x, c->pos.x))
        return 0;
    if ((c->pos.x - a->pos.x) <= 0.5f || (c->pos.y - a->pos.y) <= 0.5f)
        return 0;
    out->tl = *a;
    out->tr = *b;
    out->br = *c;
    out->bl = *d;
    return 1;
}

static int fb_try_make_quad(const ImDrawVert *vtx_buffer, unsigned int vtx_size, const ImDrawIdx *idx, unsigned int vtx_offset, FbQuad *out)
{
    unsigned int a0, a1, a2, b0, b1, b2;
    unsigned int ids[4];
    const ImDrawVert *v[4];
    float minx, maxx, miny, maxy;
    unsigned int mask = 0;
    if (!vtx_buffer || !idx || !out)
        return 0;
    a0 = (unsigned int)idx[0] + vtx_offset;
    a1 = (unsigned int)idx[1] + vtx_offset;
    a2 = (unsigned int)idx[2] + vtx_offset;
    b0 = (unsigned int)idx[3] + vtx_offset;
    b1 = (unsigned int)idx[4] + vtx_offset;
    b2 = (unsigned int)idx[5] + vtx_offset;

    if (a0 < vtx_offset || a1 < vtx_offset || a2 < vtx_offset ||
        b0 < vtx_offset || b1 < vtx_offset || b2 < vtx_offset ||
        a0 >= vtx_size || a1 >= vtx_size || a2 >= vtx_size ||
        b0 >= vtx_size || b1 >= vtx_size || b2 >= vtx_size)
        return 0;

    if (a0 == b0 && a2 == b1) {
        ids[0] = a0; ids[1] = a1; ids[2] = a2; ids[3] = b2;
    } else if (a2 == b0 && a0 == b2) {
        ids[0] = a0; ids[1] = a1; ids[2] = a2; ids[3] = b1;
    } else {
        return 0;
    }
    if (ids[0] == ids[1] || ids[0] == ids[2] || ids[0] == ids[3] ||
        ids[1] == ids[2] || ids[1] == ids[3] || ids[2] == ids[3])
        return 0;

    v[0] = &vtx_buffer[ids[0]];
    v[1] = &vtx_buffer[ids[1]];
    v[2] = &vtx_buffer[ids[2]];
    v[3] = &vtx_buffer[ids[3]];
    if (!fb_vertex_is_valid(*v[0]) || !fb_vertex_is_valid(*v[1]) ||
        !fb_vertex_is_valid(*v[2]) || !fb_vertex_is_valid(*v[3]))
        return 0;
    if (v[0]->col != v[1]->col || v[0]->col != v[2]->col || v[0]->col != v[3]->col)
        return 0;

    minx = fb_min_f(v[0]->pos.x, fb_min_f(v[1]->pos.x, fb_min_f(v[2]->pos.x, v[3]->pos.x)));
    maxx = fb_max_f(v[0]->pos.x, fb_max_f(v[1]->pos.x, fb_max_f(v[2]->pos.x, v[3]->pos.x)));
    miny = fb_min_f(v[0]->pos.y, fb_min_f(v[1]->pos.y, fb_min_f(v[2]->pos.y, v[3]->pos.y)));
    maxy = fb_max_f(v[0]->pos.y, fb_max_f(v[1]->pos.y, fb_max_f(v[2]->pos.y, v[3]->pos.y)));
    if ((maxx - minx) <= 0.5f || (maxy - miny) <= 0.5f)
        return 0;

    if (!fb_assign_quad_corner(*v[0], minx, miny, maxx, maxy, out, &mask)) return 0;
    if (!fb_assign_quad_corner(*v[1], minx, miny, maxx, maxy, out, &mask)) return 0;
    if (!fb_assign_quad_corner(*v[2], minx, miny, maxx, maxy, out, &mask)) return 0;
    if (!fb_assign_quad_corner(*v[3], minx, miny, maxx, maxy, out, &mask)) return 0;
    return mask == 0x0fU;
}

static int fb_quad_has_constant_uv(const FbQuad *q)
{
    if (!q)
        return 0;
    return fb_float_near(q->tl.uv.x, q->tr.uv.x) &&
           fb_float_near(q->tl.uv.x, q->br.uv.x) &&
           fb_float_near(q->tl.uv.x, q->bl.uv.x) &&
           fb_float_near(q->tl.uv.y, q->tr.uv.y) &&
           fb_float_near(q->tl.uv.y, q->br.uv.y) &&
           fb_float_near(q->tl.uv.y, q->bl.uv.y);
}


static const unsigned char *fb_panel_pixel_ptr(const gui_pixel_panel_desc_t *panel, unsigned int x, unsigned int y)
{
    if (!panel || !panel->pixels || x >= panel->width || y >= panel->height)
        return 0;
    switch (panel->format) {
    case GUI_PIXEL_FORMAT_RGB888:
    case GUI_PIXEL_FORMAT_BGR888:
        return (const unsigned char *)panel->pixels + y * panel->pitch + x * 3U;
    case GUI_PIXEL_FORMAT_XRGB8888:
    case GUI_PIXEL_FORMAT_BGRX8888:
        return (const unsigned char *)panel->pixels + y * panel->pitch + x * 4U;
    case GUI_PIXEL_FORMAT_RGB565:
        return (const unsigned char *)panel->pixels + y * panel->pitch + x * 2U;
    default:
        return 0;
    }
}

static void fb_panel_read_pixel(const gui_pixel_panel_desc_t *panel, unsigned int x, unsigned int y, unsigned char *r, unsigned char *g, unsigned char *b)
{
    const unsigned char *p = fb_panel_pixel_ptr(panel, x, y);
    *r = *g = *b = 0;
    if (!p)
        return;
    switch (panel->format) {
    case GUI_PIXEL_FORMAT_RGB888:
        *r = p[0]; *g = p[1]; *b = p[2]; break;
    case GUI_PIXEL_FORMAT_BGR888:
        *b = p[0]; *g = p[1]; *r = p[2]; break;
    case GUI_PIXEL_FORMAT_XRGB8888:
        *r = p[1]; *g = p[2]; *b = p[3]; break;
    case GUI_PIXEL_FORMAT_BGRX8888:
        *b = p[0]; *g = p[1]; *r = p[2]; break;
    case GUI_PIXEL_FORMAT_RGB565: {
        unsigned int vv = (unsigned int)p[0] | ((unsigned int)p[1] << 8);
        *r = (unsigned char)((((vv >> 11) & 31U) * 255U) / 31U);
        *g = (unsigned char)((((vv >> 5) & 63U) * 255U) / 63U);
        *b = (unsigned char)(((vv & 31U) * 255U) / 31U);
        break;
    }
    default:
        break;
    }
}

static void fb_render_font_quad_fast32(const FbQuad *q, int x0, int y0, int x1, int y1, unsigned char cr, unsigned char cg, unsigned char cb, unsigned char ca)
{
    unsigned int packed;
    float w, h;
    int u_start_fp, v_start_fp, u_step_fp, v_step_fp;
    if (!g_fb || !fb_is_rgb888_32_direct(g_fb) || !g_font_texture.pixels || x1 <= x0 || y1 <= y0)
        return;
    w = q->tr.pos.x - q->tl.pos.x;
    h = q->bl.pos.y - q->tl.pos.y;
    if (w == 0.0f || h == 0.0f)
        return;
    if (!fb_fixed16_checked(((q->br.uv.x - q->tl.uv.x) * (float)g_font_texture.width * 65536.0f) / w, &u_step_fp) ||
        !fb_fixed16_checked(((q->br.uv.y - q->tl.uv.y) * (float)g_font_texture.height * 65536.0f) / h, &v_step_fp))
        return;
    if (!fb_fixed16_checked(q->tl.uv.x * (float)g_font_texture.width * 65536.0f +
                            (((float)x0 + 0.5f - q->tl.pos.x) * (float)u_step_fp), &u_start_fp) ||
        !fb_fixed16_checked(q->tl.uv.y * (float)g_font_texture.height * 65536.0f +
                            (((float)y0 + 0.5f - q->tl.pos.y) * (float)v_step_fp), &v_start_fp))
        return;
    packed = fb_pack_888_32_direct(g_fb, cr, cg, cb);
    for (int y = y0, vfp = v_start_fp; y < y1; ++y, vfp += v_step_fp) {
        int ty = vfp >> 16;
        if (ty < 0) ty = 0;
        if (ty >= g_font_texture.height) ty = g_font_texture.height - 1;
        const unsigned char *src_row = g_font_texture.pixels + ty * g_font_texture.pitch;
        unsigned int *dst = (unsigned int *)((unsigned char *)g_fb->base + (unsigned int)y * g_fb->pitch + (unsigned int)x0 * 4U);
        int ufp = u_start_fp;
        for (int x = x0; x < x1; ++x, ufp += u_step_fp) {
            int tx = ufp >> 16;
            unsigned int alpha;
            if (tx < 0) tx = 0;
            if (tx >= g_font_texture.width) tx = g_font_texture.width - 1;
            alpha = (g_font_texture.bytes_per_pixel == 1) ? src_row[tx] : src_row[tx * g_font_texture.bytes_per_pixel + 3];
            if (alpha <= BITOS_IMGUI_FONT_ALPHA_THRESHOLD) {
                ++dst;
                continue;
            }
            if (ca != 255U)
                alpha = fb_div255(alpha * ca);
#if BITOS_IMGUI_FAST_FONT_MONO
            if (alpha >= 128U) {
                *dst = packed;
            } else {
                *dst = fb_blend_pack_888_32_direct(g_fb, *dst, cr, cg, cb, (unsigned char)alpha);
                g_stats.blended_pixels++;
            }
#else
            if (alpha >= 252U)
                *dst = packed;
            else {
                *dst = fb_blend_pack_888_32_direct(g_fb, *dst, cr, cg, cb, (unsigned char)alpha);
                g_stats.blended_pixels++;
            }
#endif
            ++dst;
        }
    }
}

static int fb_panel_can_direct_copy32(const gui_pixel_panel_desc_t *panel, int x0, int y0, int x1, int y1, unsigned char cr, unsigned char cg, unsigned char cb, unsigned char ca)
{
    (void)y0;
    if (!g_fb || !panel || !panel->pixels)
        return 0;
    if (!fb_is_rgb888_32_direct(g_fb) || g_fb->red_shift != 16U || g_fb->green_shift != 8U || g_fb->blue_shift != 0U)
        return 0;
    if (panel->format != GUI_PIXEL_FORMAT_BGRX8888)
        return 0;
    if (cr != 255U || cg != 255U || cb != 255U || ca < 252U)
        return 0;
    if ((unsigned int)(x1 - x0) != panel->width || (unsigned int)(y1 - y0) != panel->height)
        return 0;
    return 1;
}

static void fb_render_panel_quad_direct_copy32(int x0, int y0, int x1, int y1, const gui_pixel_panel_desc_t *panel)
{
    unsigned int row_bytes;
    if (!g_fb || !panel || !panel->pixels || x1 <= x0 || y1 <= y0)
        return;
    row_bytes = (unsigned int)(x1 - x0) * 4U;
    for (int y = y0; y < y1; ++y) {
        const unsigned char *src = (const unsigned char *)panel->pixels + (unsigned int)(y - y0) * panel->pitch;
        unsigned char *dst = (unsigned char *)g_fb->base + (unsigned int)y * g_fb->pitch + (unsigned int)x0 * 4U;
        memcpy(dst, src, row_bytes);
    }
}

static void fb_render_panel_quad_fast32(const FbQuad *q, int x0, int y0, int x1, int y1, const gui_pixel_panel_desc_t *panel, unsigned char cr, unsigned char cg, unsigned char cb, unsigned char ca)
{
    float w, h;
    int u_start_fp, v_start_fp, u_step_fp, v_step_fp;
    if (!g_fb || !fb_is_rgb888_32_direct(g_fb) || !panel || !panel->pixels || x1 <= x0 || y1 <= y0)
        return;
    w = q->tr.pos.x - q->tl.pos.x;
    h = q->bl.pos.y - q->tl.pos.y;
    if (w == 0.0f || h == 0.0f)
        return;
    if (fb_panel_can_direct_copy32(panel, x0, y0, x1, y1, cr, cg, cb, ca) &&
        fb_float_near(q->tl.uv.x, 0.0f) && fb_float_near(q->tl.uv.y, 0.0f) &&
        fb_float_near(q->br.uv.x, 1.0f) && fb_float_near(q->br.uv.y, 1.0f)) {
        fb_render_panel_quad_direct_copy32(x0, y0, x1, y1, panel);
        return;
    }
    if (!fb_fixed16_checked(((q->br.uv.x - q->tl.uv.x) * (float)panel->width * 65536.0f) / w, &u_step_fp) ||
        !fb_fixed16_checked(((q->br.uv.y - q->tl.uv.y) * (float)panel->height * 65536.0f) / h, &v_step_fp))
        return;
    if (!fb_fixed16_checked(q->tl.uv.x * (float)panel->width * 65536.0f +
                            (((float)x0 + 0.5f - q->tl.pos.x) * (float)u_step_fp), &u_start_fp) ||
        !fb_fixed16_checked(q->tl.uv.y * (float)panel->height * 65536.0f +
                            (((float)y0 + 0.5f - q->tl.pos.y) * (float)v_step_fp), &v_start_fp))
        return;
    for (int y = y0, vfp = v_start_fp; y < y1; ++y, vfp += v_step_fp) {
        int sy_i = vfp >> 16;
        if (sy_i < 0) sy_i = 0;
        if (sy_i >= (int)panel->height) sy_i = (int)panel->height - 1;
        unsigned int sy = (unsigned int)sy_i;
        unsigned int *dst = (unsigned int *)((unsigned char *)g_fb->base + (unsigned int)y * g_fb->pitch + (unsigned int)x0 * 4U);
        int ufp = u_start_fp;
        for (int x = x0; x < x1; ++x, ufp += u_step_fp) {
            int sx_i = ufp >> 16;
            if (sx_i < 0) sx_i = 0;
            if (sx_i >= (int)panel->width) sx_i = (int)panel->width - 1;
            unsigned char r, g, b;
            fb_panel_read_pixel(panel, (unsigned int)sx_i, sy, &r, &g, &b);
            r = (unsigned char)(((unsigned int)r * cr) / 255U);
            g = (unsigned char)(((unsigned int)g * cg) / 255U);
            b = (unsigned char)(((unsigned int)b * cb) / 255U);
#if BITOS_IMGUI_FAST_PANEL_OPAQUE
            if (ca >= 252U)
                *dst = fb_pack_888_32_direct(g_fb, r, g, b);
            else
#endif
            {
                *dst = fb_blend_pack_888_32_direct(g_fb, *dst, r, g, b, ca);
                g_stats.blended_pixels++;
            }
            ++dst;
        }
    }
}


static void fb_render_quad_textured(const FbQuad *q, int clip_x1, int clip_y1, int clip_x2, int clip_y2, ImTextureID tex_id)
{
    int x0, y0, x1, y1;
    int qx0, qy0, qx1, qy1;
    float width, height;
    float inv_w, inv_h;
    unsigned char cr, cg, cb, ca;
    const gui_pixel_panel_desc_t *panel = 0;
    if (!q || !g_fb)
        return;
    if (!fb_quad_vertices_are_valid(q) ||
        !fb_floor_i_checked(q->tl.pos.x, &qx0) || !fb_floor_i_checked(q->tl.pos.y, &qy0) ||
        !fb_ceil_i_checked(q->br.pos.x, &qx1) || !fb_ceil_i_checked(q->br.pos.y, &qy1)) {
        g_stats.invalid_vertex_primitives++;
        return;
    }
    x0 = fb_clamp_i(qx0, clip_x1, clip_x2);
    y0 = fb_clamp_i(qy0, clip_y1, clip_y2);
    x1 = fb_clamp_i(qx1, clip_x1, clip_x2);
    y1 = fb_clamp_i(qy1, clip_y1, clip_y2);
    if (x1 <= x0 || y1 <= y0)
        return;
    cr = fb_col_r(q->tl.col);
    cg = fb_col_g(q->tl.col);
    cb = fb_col_b(q->tl.col);
    ca = fb_col_a(q->tl.col);
    if (ca == 0)
        return;
    if ((tex_id == g_font_tex_id || tex_id == (ImTextureID)0) && fb_quad_has_constant_uv(q)) {
        fb_fill_rect_fast(g_fb, x0, y0, x1, y1, cr, cg, cb, ca);
        g_stats.fast_quads++;
        return;
    }

    if (tex_id != g_font_tex_id && tex_id != (ImTextureID)0)
        panel = (const gui_pixel_panel_desc_t *)(uintptr_t)tex_id;

    width = q->tr.pos.x - q->tl.pos.x;
    height = q->bl.pos.y - q->tl.pos.y;
    if (!fb_float_is_finite(width) || !fb_float_is_finite(height) || width == 0.0f || height == 0.0f) {
        g_stats.invalid_vertex_primitives++;
        return;
    }

    fb_dirty_add(x0, y0, x1, y1);
    g_stats.fast_quads++;
    g_stats.textured_quads++;
    if (!panel && fb_is_rgb888_32_direct(g_fb) && g_font_texture.pixels) {
        fb_render_font_quad_fast32(q, x0, y0, x1, y1, cr, cg, cb, ca);
        return;
    }
    if (panel && fb_is_rgb888_32_direct(g_fb)) {
        fb_render_panel_quad_fast32(q, x0, y0, x1, y1, panel, cr, cg, cb, ca);
        return;
    }

    inv_w = 1.0f / width;
    inv_h = 1.0f / height;
    for (int y = y0; y < y1; ++y) {
        float fy = ((float)y + 0.5f - q->tl.pos.y) * inv_h;
        float v_left = q->tl.uv.y + (q->bl.uv.y - q->tl.uv.y) * fy;
        float v_right = q->tr.uv.y + (q->br.uv.y - q->tr.uv.y) * fy;
        for (int x = x0; x < x1; ++x) {
            float fx = ((float)x + 0.5f - q->tl.pos.x) * inv_w;
            float u_top = q->tl.uv.x + (q->tr.uv.x - q->tl.uv.x) * fx;
            float u_bottom = q->bl.uv.x + (q->br.uv.x - q->bl.uv.x) * fx;
            float u = u_top + (u_bottom - u_top) * fy;
            float v = v_left + (v_right - v_left) * fx;
            unsigned char tr, tg, tb, ta;
            if (panel)
                fb_sample_panel(panel, u, v, &tr, &tg, &tb, &ta);
            else
                fb_sample_font(u, v, &tr, &tg, &tb, &ta);
            if (ta == 0 || ca == 0)
                continue;
            unsigned char rr = (unsigned char)(((unsigned int)cr * tr) / 255U);
            unsigned char gg = (unsigned char)(((unsigned int)cg * tg) / 255U);
            unsigned char bb = (unsigned char)(((unsigned int)cb * tb) / 255U);
            unsigned char aa = (unsigned char)(((unsigned int)ca * ta) / 255U);
            fb_blend_pixel(g_fb, x, y, rr, gg, bb, aa);
        }
    }
}


static bool fb_update_font_texture(const char *source_label)
{
    unsigned char *pixels = 0;
    int w = 0, h = 0, bpp = 0;
    if (!ImGui::GetCurrentContext())
        return false;
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->GetTexDataAsAlpha8(&pixels, &w, &h, &bpp);
    if (!pixels || w <= 0 || h <= 0)
        return false;
    if (bpp <= 0) bpp = 1;
    io.Fonts->SetTexID(g_font_tex_id);
    if (io.Fonts->TexData) {
        io.Fonts->TexData->SetTexID(g_font_tex_id);
        io.Fonts->TexData->SetStatus(ImTextureStatus_OK);
    }
    g_font_texture.pixels = pixels;
    g_font_texture.width = w;
    g_font_texture.height = h;
    g_font_texture.pitch = w * bpp;
    g_font_texture.bytes_per_pixel = bpp;
    g_font_texture.is_panel = 0;
    if (source_label && source_label[0]) {
        snprintf(g_font_source, sizeof(g_font_source), "%s", source_label);
    }
    g_full_clear_request = 2;
    return true;
}

static ImFont *fb_add_builtin_bitmap_font(float size_pixels)
{
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 1;
    font_cfg.OversampleV = 1;
    font_cfg.PixelSnapH = true;
    font_cfg.SizePixels = size_pixels > 0.0f ? size_pixels : 13.0f;
    font_cfg.GlyphMaxAdvanceX = 64.0f;
    ImFont *font = io.Fonts->AddFontDefaultBitmap(&font_cfg);
    io.FontDefault = font;
    ImGui::GetStyle().FontSizeBase = font_cfg.SizePixels;
    return font;
}

static bool fb_restore_builtin_font(void)
{
    if (!ImGui::GetCurrentContext())
        return false;
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFont *font = fb_add_builtin_bitmap_font(13.0f);
    if (!font)
        return false;
    return fb_update_font_texture("builtin-bitmap");
}

int ImGuiFramebuffer_LoadFontFromMemory(const unsigned char *data, unsigned int size, float size_pixels, const char *source_label)
{
    if (!ImGui::GetCurrentContext() || !data || size < 512U)
        return 0;
    if (size > (4U * 1024U * 1024U))
        return 0;
    unsigned char *copy = (unsigned char *)GuiOs_Alloc(size);
    if (!copy)
        return 0;
    memcpy(copy, data, size);

    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    cfg.SizePixels = size_pixels > 0.0f ? size_pixels : 15.0f;
    cfg.GlyphMaxAdvanceX = 96.0f;
    cfg.FontDataOwnedByAtlas = false;

    const ImWchar *ranges = io.Fonts->GetGlyphRangesCyrillic();
    ImFont *font = io.Fonts->AddFontFromMemoryTTF(copy, (int)size, cfg.SizePixels, &cfg, ranges);
    if (!font) {
        GuiOs_Free(copy);
        (void)fb_restore_builtin_font();
        return 0;
    }
    io.FontDefault = font;
    ImGui::GetStyle().FontSizeBase = cfg.SizePixels;
    if (!fb_update_font_texture(source_label && source_label[0] ? source_label : "flash-ttf")) {
        GuiOs_Free(copy);
        (void)fb_restore_builtin_font();
        return 0;
    }
    if (g_external_font_data)
        GuiOs_Free(g_external_font_data);
    g_external_font_data = copy;
    g_external_font_data_size = size;
    return 1;
}

const char *ImGuiFramebuffer_GetFontSource(void)
{
    return g_font_source;
}

bool ImGuiFramebuffer_Init(gui_framebuffer_t *fb)
{
    unsigned char *pixels = 0;
    int w = 0, h = 0, bpp = 0;
    if (!fb || !GuiFramebuffer_IsValid(fb))
        return false;
    memset(&g_stats, 0, sizeof(g_stats));
    g_fb = fb;
    g_cursor_overlay_enabled = (fb && fb_is_rgb888_32_direct(fb) && GuiFramebuffer_GetFrontBase(fb)) ? 1 : 0;
    g_cursor_visible = 0;
    g_prev_mouse_x = -4096;
    g_prev_mouse_y = -4096;
    g_prev_mouse_buttons = 0;
    ImGui::SetAllocatorFunctions(imgui_fb_alloc, imgui_fb_free, 0);
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = 0;
    io.LogFilename = 0;
    io.BackendRendererName = "bitos_imgui_framebuffer_fast";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    /* ARCH-33/R0.9: never ask Dear ImGui to draw its own software cursor in
     * freestanding UEFI.  If MousePos is temporarily unavailable it may emit
     * -FLT_MAX cursor geometry.  MiniOS draws the cursor through the framebuffer
     * overlay path when possible. */
    io.MouseDrawCursor = false;
    io.DisplaySize = ImVec2((float)fb->width, (float)fb->height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    fb_sync_main_viewport();
    /* ARCH-70/R0.46: start with the deterministic built-in bitmap font; after
     * USB/FAT is mounted the console may replace it with a TTF from flash. */
    (void)fb_add_builtin_bitmap_font(13.0f);
    (void)fb_update_font_texture("builtin-bitmap");
    pixels = (unsigned char *)g_font_texture.pixels;
    w = g_font_texture.width;
    h = g_font_texture.height;
    bpp = g_font_texture.bytes_per_pixel;
    {
        float flt_max_probe = FLT_MAX;
        unsigned int flt_max_bits = fb_float_bits(flt_max_probe);
        unsigned int flt_roundtrip_ok = (flt_max_probe == FLT_MAX) ? 1U : 0U;
        printf("\nImGui framebuffer ABI: sizeof(ImDrawVert)=%u sizeof(ImDrawIdx)=%u sizeof(ImDrawCmd)=%u pos_off=%u uv_off=%u col_off=%u cmd_clip_off=%u cmd_texref_off=%u cmd_idx_off=%u cmd_vtx_off=%u viewport=%ux%u font=%p atlas=%dx%d bpp=%d tex_id=0x%llx tex_status=%u flt_max_bits=0x%08x flt_rt=%u",
               (unsigned)sizeof(ImDrawVert), (unsigned)sizeof(ImDrawIdx), (unsigned)sizeof(ImDrawCmd),
               (unsigned)((uintptr_t)&(((ImDrawVert *)0)->pos)),
               (unsigned)((uintptr_t)&(((ImDrawVert *)0)->uv)),
               (unsigned)((uintptr_t)&(((ImDrawVert *)0)->col)),
               (unsigned)((uintptr_t)&(((ImDrawCmd *)0)->ClipRect)),
               (unsigned)((uintptr_t)&(((ImDrawCmd *)0)->TexRef)),
               (unsigned)((uintptr_t)&(((ImDrawCmd *)0)->IdxOffset)),
               (unsigned)((uintptr_t)&(((ImDrawCmd *)0)->VtxOffset)),
               (unsigned)fb->width, (unsigned)fb->height, (void *)io.FontDefault, w, h, bpp,
               (unsigned long long)(uintptr_t)g_font_tex_id,
               io.Fonts->TexData ? (unsigned)io.Fonts->TexData->Status : 0xffffffffU,
               flt_max_bits, flt_roundtrip_ok);
    }
    ImGui::StyleColorsDark();
    {
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.AntiAliasedLines = false;
        style.AntiAliasedLinesUseTex = false;
        style.AntiAliasedFill = false;
        for (int i = 0; i < ImGuiCol_COUNT; ++i)
            style.Colors[i].w = 1.0f;
    }
    return true;
}

void ImGuiFramebuffer_Shutdown(void)
{
    fb_cursor_restore();
    if (ImGui::GetCurrentContext())
        ImGui::DestroyContext();
    g_fb = 0;
    g_cursor_overlay_enabled = 0;
    if (g_external_font_data) { GuiOs_Free(g_external_font_data); g_external_font_data = 0; g_external_font_data_size = 0; }
    snprintf(g_font_source, sizeof(g_font_source), "%s", "builtin-bitmap");
    memset(&g_font_texture, 0, sizeof(g_font_texture));
}

void ImGuiFramebuffer_NewFrame(float delta_seconds, int mouse_x, int mouse_y, unsigned int mouse_buttons, float mouse_wheel)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!g_fb)
        return;
    io.DisplaySize = ImVec2((float)g_fb->width, (float)g_fb->height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = delta_seconds > 0.0f ? delta_seconds : (1.0f / 20.0f);
    /* ARCH-33/R0.9: never ask Dear ImGui to draw its own software cursor in
     * freestanding UEFI.  If MousePos is temporarily unavailable it may emit
     * -FLT_MAX cursor geometry.  MiniOS draws the cursor through the framebuffer
     * overlay path when possible. */
    io.MouseDrawCursor = false;
    fb_sync_main_viewport();
    g_mouse_x = mouse_x;
    g_mouse_y = mouse_y;
    g_mouse_buttons = mouse_buttons;
    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
    io.MouseDown[0] = (mouse_buttons & 1U) != 0U;
    io.MouseDown[1] = (mouse_buttons & 2U) != 0U;
    io.MouseDown[2] = (mouse_buttons & 4U) != 0U;
    io.AddMousePosEvent((float)mouse_x, (float)mouse_y);
    io.AddMouseButtonEvent(0, (mouse_buttons & 1U) != 0U);
    io.AddMouseButtonEvent(1, (mouse_buttons & 2U) != 0U);
    io.AddMouseButtonEvent(2, (mouse_buttons & 4U) != 0U);
    if (mouse_wheel != 0.0f)
        io.AddMouseWheelEvent(0.0f, mouse_wheel);
    ImGui::NewFrame();
    fb_sync_main_viewport();
}


static void fb_clear_cursor_rect(int mx, int my)
{
    int pad = BITOS_IMGUI_CURSOR_ERASE_PAD;
    if (!g_fb)
        return;
    fb_fill_rect_fast(g_fb, mx - pad, my - pad, mx + pad, my + pad, g_clear_r, g_clear_g, g_clear_b, 255);
    fb_dirty_add(mx - pad, my - pad, mx + pad, my + pad);
    g_stats.dirty_clears++;
}

static int fb_should_full_clear(unsigned int frame_index)
{
#if BITOS_IMGUI_SKIP_FULL_CLEAR
    if (g_full_clear_request)
        return 1;
    if (frame_index <= 2U)
        return 1;
    if (g_mouse_buttons || g_prev_mouse_buttons)
        return 1;
#if BITOS_IMGUI_FULL_CLEAR_INTERVAL
    if ((frame_index % BITOS_IMGUI_FULL_CLEAR_INTERVAL) == 0U)
        return 1;
#endif
    return 0;
#else
    (void)frame_index;
    return 1;
#endif
}

void ImGuiFramebuffer_Render(void)
{
    ImDrawData *draw_data;
    u32 t0, t1, t2;
    unsigned int next_frame;
    int full_clear;
    if (!g_fb || !ImGui::GetCurrentContext())
        return;
    t0 = GuiOs_GetTicks();
    next_frame = g_stats.frame_index + 1U;
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.frame_index = next_frame;
    g_stats.cursor_overlay_moves = g_cursor_overlay_moves_total;
    g_stats.cursor_overlay_pixels = g_cursor_overlay_pixels_total;
    g_stats.full_ui_frames = g_full_ui_frames_total + 1U;
    g_stats.cached_cursor_frames = g_cached_cursor_frames_total;
    fb_dirty_reset();
    full_clear = fb_should_full_clear(next_frame);
    fb_sync_main_viewport();
    ImGui::Render();
    if (full_clear) {
        fb_clear_fast(g_fb);
        g_stats.full_clears++;
        if (g_full_clear_request)
            g_full_clear_request--;
    } else if (!g_cursor_overlay_enabled) {
        fb_clear_cursor_rect(g_prev_mouse_x, g_prev_mouse_y);
        fb_clear_cursor_rect(g_mouse_x, g_mouse_y);
    }
    /* ARCH-50/R0.26: external FB is a fixed pixel plane.  Compose it before
     * Dear ImGui draw lists; the oConsole layouts reserve this rectangle and do
     * not place widgets there.  This matches real video/MPEG overlay semantics:
     * source pixels are refreshed first, then UI chrome is drawn around them. */
    if (g_external_plane_enabled) {
        if (fb_blit_panel_rect_fast(&g_external_plane_panel,
                                    g_external_plane_x, g_external_plane_y,
                                    g_external_plane_w, g_external_plane_h)) {
            fb_dirty_add(g_external_plane_x, g_external_plane_y,
                         g_external_plane_x + g_external_plane_w,
                         g_external_plane_y + g_external_plane_h);
        }
    }
    draw_data = ImGui::GetDrawData();
    if (draw_data) {
        g_stats.cmd_lists = (unsigned int)draw_data->CmdListsCount;
        for (int n = 0; n < draw_data->CmdListsCount; ++n) {
            const ImDrawList *cmd_list = draw_data->CmdLists[n];
            if (!cmd_list)
                continue;
            const ImDrawVert *vtx_buffer = cmd_list->VtxBuffer.Data;
            const ImDrawIdx *idx_buffer = cmd_list->IdxBuffer.Data;
            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
                const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
                g_stats.draw_cmds++;
                g_stats.elem_count += pcmd->ElemCount;
                if (pcmd->UserCallback) {
                    pcmd->UserCallback(cmd_list, pcmd);
                    continue;
                }
                int clip_x1 = 0, clip_y1 = 0, clip_x2 = 0, clip_y2 = 0;
                int clip_status;
                unsigned int vtx_size = (cmd_list->VtxBuffer.Size > 0) ? (unsigned int)cmd_list->VtxBuffer.Size : 0U;
                unsigned int idx_size = (cmd_list->IdxBuffer.Size > 0) ? (unsigned int)cmd_list->IdxBuffer.Size : 0U;
                ImTextureID tex_id;
                if (!pcmd->ElemCount) {
                    g_stats.zero_elem_cmds++;
                    continue;
                }
                if (!vtx_buffer || !idx_buffer || !vtx_size || !idx_size) {
                    g_stats.invalid_index_primitives++;
                    continue;
                }
                if (!g_stats.first_vtx_bits_x && !g_stats.first_vtx_bits_y) {
                    unsigned int first_i;
                    if (fb_draw_index_valid(idx_buffer, idx_size, pcmd->IdxOffset, 0U, pcmd->VtxOffset, vtx_size, &first_i)) {
                        int vx = 0, vy = 0;
                        g_stats.first_vtx_bits_x = fb_float_bits(vtx_buffer[first_i].pos.x);
                        g_stats.first_vtx_bits_y = fb_float_bits(vtx_buffer[first_i].pos.y);
                        (void)fb_trunc_i_checked(vtx_buffer[first_i].pos.x, &vx);
                        (void)fb_trunc_i_checked(vtx_buffer[first_i].pos.y, &vy);
                        g_stats.first_vtx_x = vx;
                        g_stats.first_vtx_y = vy;
                    }
                }
                clip_status = fb_make_clip_rect_checked(pcmd, draw_data, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
                if (clip_status != FB_CLIP_OK) {
                    if (clip_status == FB_CLIP_INVALID)
                        g_stats.invalid_clip_cmds++;
                    else
                        g_stats.clip_reject_cmds++;
                    if (!g_stats.clip_first_raw_x && !g_stats.clip_first_raw_y &&
                        !g_stats.clip_first_raw_z && !g_stats.clip_first_raw_w) {
                        g_stats.clip_first_raw_x = fb_float_bits(pcmd->ClipRect.x);
                        g_stats.clip_first_raw_y = fb_float_bits(pcmd->ClipRect.y);
                        g_stats.clip_first_raw_z = fb_float_bits(pcmd->ClipRect.z);
                        g_stats.clip_first_raw_w = fb_float_bits(pcmd->ClipRect.w);
                        g_stats.clip_first_x1 = (unsigned int)clip_x1;
                        g_stats.clip_first_y1 = (unsigned int)clip_y1;
                        g_stats.clip_first_x2 = (unsigned int)clip_x2;
                        g_stats.clip_first_y2 = (unsigned int)clip_y2;
                    }
                    /* UEFI/x64 hardening: never cast invalid clip floats to int.
                     * If the first active ImGui command only lost its clip state,
                     * recover with a full-frame scissor; otherwise skip it and keep
                     * the frame deterministic. */
                    if (g_stats.cmd_lists && g_stats.elem_count &&
                        !g_stats.fast_quads && !g_stats.fallback_triangles) {
                        clip_x1 = 0;
                        clip_y1 = 0;
                        clip_x2 = (int)g_fb->width;
                        clip_y2 = (int)g_fb->height;
                        g_stats.forced_fullclip_cmds++;
                        g_stats.render_recoveries++;
                    } else {
                        continue;
                    }
                }
                tex_id = pcmd->GetTexID();
                for (unsigned int i = 0; i + 2U < pcmd->ElemCount;) {
                    FbQuad q;
                    unsigned int dummy;
                    int have_six_indices = (i + 5U < pcmd->ElemCount) &&
                                           fb_draw_index_valid(idx_buffer, idx_size, pcmd->IdxOffset, i + 5U, pcmd->VtxOffset, vtx_size, &dummy);
                    if (i + 5U < pcmd->ElemCount)
                        g_stats.quad_candidates++;
                    if (have_six_indices &&
                        (fb_try_make_quad_direct(vtx_buffer, vtx_size, &idx_buffer[pcmd->IdxOffset + i], pcmd->VtxOffset, &q) ||
                         fb_try_make_quad(vtx_buffer, vtx_size, &idx_buffer[pcmd->IdxOffset + i], pcmd->VtxOffset, &q))) {
                        fb_render_quad_textured(&q, clip_x1, clip_y1, clip_x2, clip_y2, tex_id);
                        i += 6U;
                    } else {
                        unsigned int i0, i1, i2;
                        if (i + 5U < pcmd->ElemCount)
                            g_stats.quad_rejects++;
                        if (!fb_draw_index_valid(idx_buffer, idx_size, pcmd->IdxOffset, i + 0U, pcmd->VtxOffset, vtx_size, &i0) ||
                            !fb_draw_index_valid(idx_buffer, idx_size, pcmd->IdxOffset, i + 1U, pcmd->VtxOffset, vtx_size, &i1) ||
                            !fb_draw_index_valid(idx_buffer, idx_size, pcmd->IdxOffset, i + 2U, pcmd->VtxOffset, vtx_size, &i2)) {
                            g_stats.invalid_index_primitives++;
                            i += 3U;
                            continue;
                        }
                        g_stats.triangle_candidates++;
                        fb_render_triangle(vtx_buffer[i0], vtx_buffer[i1], vtx_buffer[i2], clip_x1, clip_y1, clip_x2, clip_y2, tex_id);
                        i += 3U;
                    }
                }
            }
        }
    }
    if (g_stats.draw_cmds && g_stats.elem_count && !g_stats.fast_quads && !g_stats.fallback_triangles) {
        g_stats.render_recoveries++;
        GuiFramebuffer_FillRect(g_fb, 8, 8, 760, 84, 64, 24, 24);
        GuiFramebuffer_Rect(g_fb, 8, 8, 760, 84, 255, 220, 80);
        GuiFramebuffer_DrawText(g_fb, 18, 20, "UEFI ImGui renderer recovery: draw commands had elements but no primitives were rasterized", 255, 255, 255);
        GuiFramebuffer_DrawText(g_fb, 18, 42, "Check serial: IMGUI FPS elems/clip/forceclip/q/tri and ImGui framebuffer detail", 255, 255, 160);
        fb_dirty_add(8, 8, 768, 92);
    }
    if (g_cursor_visible) {
        fb_dirty_add(g_cursor_saved_x0, g_cursor_saved_y0, g_cursor_saved_x1, g_cursor_saved_y1);
        fb_cursor_restore();
    }
    t1 = GuiOs_GetTicks();
    g_stats.present_pixels = fb_dirty_area();
    if (!full_clear && g_stats.present_pixels && g_stats.present_pixels < (g_fb->width * g_fb->height * 3U) / 4U) {
        GuiFramebuffer_PresentRect(g_fb, (u32)g_dirty_x0, (u32)g_dirty_y0, (u32)(g_dirty_x1 - g_dirty_x0), (u32)(g_dirty_y1 - g_dirty_y0));
    } else {
        GuiFramebuffer_Present(g_fb);
        g_stats.present_pixels = g_fb->width * g_fb->height;
    }
    fb_cursor_show(g_mouse_x, g_mouse_y);
    t2 = GuiOs_GetTicks();
    g_full_ui_frames_total++;
    g_stats.cursor_overlay_moves = g_cursor_overlay_moves_total;
    g_stats.cursor_overlay_pixels = g_cursor_overlay_pixels_total;
    g_stats.full_ui_frames = g_full_ui_frames_total;
    g_stats.cached_cursor_frames = g_cached_cursor_frames_total;
    g_prev_mouse_x = g_mouse_x;
    g_prev_mouse_y = g_mouse_y;
    g_prev_mouse_buttons = g_mouse_buttons;
    g_stats.render_ms = (u32)(t1 - t0);
    g_stats.present_ms = (u32)(t2 - t1);
    g_stats.total_ms = (u32)(t2 - t0);
    g_stats.last_ticks = t2;
    if ((g_profile_enabled && g_stats.frame_index <= 4U) ||
        (g_stats.draw_cmds && g_stats.elem_count && !g_stats.fast_quads && !g_stats.fallback_triangles && g_stats.frame_index <= 120U)) {
        GuiOs_Logf(GUI_OS_LOG_INFO,
                   "ImGui framebuffer detail: frame=%u lists=%u cmds=%u elems=%u zero=%u clip=%u forceclip=%u invalid_clip=%u invalid_idx=%u invalid_vtx=%u recover=%u first_clip=%u,%u-%u,%u raw_clip=%08x,%08x,%08x,%08x first_vtx=%d,%d raw_vtx=%08x,%08x qcand=%u qrej=%u tcand=%u q=%u tri=%u present=%u full=%u dirty=%u",
                   g_stats.frame_index, g_stats.cmd_lists, g_stats.draw_cmds, g_stats.elem_count,
                   g_stats.zero_elem_cmds, g_stats.clip_reject_cmds, g_stats.forced_fullclip_cmds,
                   g_stats.invalid_clip_cmds, g_stats.invalid_index_primitives, g_stats.invalid_vertex_primitives,
                   g_stats.render_recoveries,
                   g_stats.clip_first_x1, g_stats.clip_first_y1, g_stats.clip_first_x2, g_stats.clip_first_y2,
                   g_stats.clip_first_raw_x, g_stats.clip_first_raw_y, g_stats.clip_first_raw_z, g_stats.clip_first_raw_w,
                   g_stats.first_vtx_x, g_stats.first_vtx_y,
                   g_stats.first_vtx_bits_x, g_stats.first_vtx_bits_y,
                   g_stats.quad_candidates, g_stats.quad_rejects, g_stats.triangle_candidates,
                   g_stats.fast_quads, g_stats.fallback_triangles, g_stats.present_pixels,
                   g_stats.full_clears, g_stats.dirty_clears);
    }
    if (g_profile_enabled && (g_stats.frame_index % 60U) == 0U) {
        GuiOs_Logf(GUI_OS_LOG_INFO,
                   "ImGui framebuffer stats: frame=%u total=%ums render=%ums present=%ums lists=%u cmds=%u elems=%u zero=%u clip=%u forceclip=%u invalid_clip=%u invalid_idx=%u invalid_vtx=%u recover=%u qcand=%u qrej=%u tcand=%u quads=%u solid=%u tex=%u fallback_tri=%u fallback_px=%u blend_px=%u clear_px=%u full=%u dirty=%u present_px=%u cursor=%u cached=%u",
                   g_stats.frame_index, g_stats.total_ms, g_stats.render_ms, g_stats.present_ms,
                   g_stats.cmd_lists, g_stats.draw_cmds, g_stats.elem_count,
                   g_stats.zero_elem_cmds, g_stats.clip_reject_cmds, g_stats.forced_fullclip_cmds,
                   g_stats.invalid_clip_cmds, g_stats.invalid_index_primitives, g_stats.invalid_vertex_primitives,
                   g_stats.render_recoveries, g_stats.quad_candidates,
                   g_stats.quad_rejects, g_stats.triangle_candidates,
                   g_stats.fast_quads, g_stats.solid_rects, g_stats.textured_quads,
                   g_stats.fallback_triangles, g_stats.fallback_pixels, g_stats.blended_pixels,
                   g_stats.clear_pixels, g_stats.full_clears, g_stats.dirty_clears, g_stats.present_pixels,
                   g_stats.cursor_overlay_moves, g_stats.cached_cursor_frames);
    }
}


int ImGuiFramebuffer_MoveCursorOnly(int mouse_x, int mouse_y, unsigned int mouse_buttons)
{
    (void)mouse_buttons;
    if (!fb_cursor_can_overlay())
        return 0;
    g_mouse_x = mouse_x;
    g_mouse_y = mouse_y;
    g_mouse_buttons = mouse_buttons;
    fb_cursor_restore();
    fb_cursor_show(mouse_x, mouse_y);
    g_cached_cursor_frames_total++;
    g_stats.cursor_overlay_moves = g_cursor_overlay_moves_total;
    g_stats.cursor_overlay_pixels = g_cursor_overlay_pixels_total;
    g_stats.cached_cursor_frames = g_cached_cursor_frames_total;
    g_stats.full_ui_frames = g_full_ui_frames_total;
    g_stats.last_ticks = GuiOs_GetTicks();
    return 1;
}


static int fb_blit_panel_rect_fast(const gui_pixel_panel_desc_t *panel, int x, int y, int w, int h)
{
    if (!g_fb || !panel || !panel->pixels || w <= 0 || h <= 0)
        return 0;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= (int)g_fb->width || y >= (int)g_fb->height || w <= 0 || h <= 0)
        return 0;
    if (x + w > (int)g_fb->width) w = (int)g_fb->width - x;
    if (y + h > (int)g_fb->height) h = (int)g_fb->height - y;
    if (w <= 0 || h <= 0)
        return 0;

    if (fb_is_rgb888_32_direct(g_fb) && panel->format == GUI_PIXEL_FORMAT_BGRX8888 && panel->pitch >= panel->width * 4U) {
        unsigned char *dst_base = (unsigned char *)g_fb->base;
        const unsigned char *src_base = (const unsigned char *)panel->pixels;
        for (int dy = 0; dy < h; ++dy) {
            unsigned int sy = ((unsigned int)dy * panel->height) / (unsigned int)h;
            unsigned int *dst = (unsigned int *)(dst_base + (unsigned int)(y + dy) * g_fb->pitch + (unsigned int)x * 4U);
            const unsigned int *src = (const unsigned int *)(src_base + sy * panel->pitch);
            if ((unsigned int)w == panel->width) {
                memcpy(dst, src, (unsigned int)w * 4U);
            } else {
                for (int dx = 0; dx < w; ++dx) {
                    unsigned int sx = ((unsigned int)dx * panel->width) / (unsigned int)w;
                    dst[dx] = src[sx];
                }
            }
        }
        return 1;
    }

    GuiFramebuffer_BlitPixelPanel(g_fb, (u32)x, (u32)y, (u32)w, (u32)h, panel);
    return 1;
}

void ImGuiFramebuffer_SetExternalPanelRect(const gui_pixel_panel_desc_t *panel, int x, int y, int w, int h)
{
    if (!panel || !panel->pixels || panel->width == 0U || panel->height == 0U || w <= 0 || h <= 0) {
        g_external_plane_enabled = 0;
        memset(&g_external_plane_panel, 0, sizeof(g_external_plane_panel));
        g_external_plane_x = g_external_plane_y = g_external_plane_w = g_external_plane_h = 0;
        return;
    }
    g_external_plane_panel = *panel;
    g_external_plane_x = x;
    g_external_plane_y = y;
    g_external_plane_w = w;
    g_external_plane_h = h;
    g_external_plane_enabled = 1;
}

int ImGuiFramebuffer_BlitPixelPanelRect(const gui_pixel_panel_desc_t *panel, int x, int y, int w, int h)
{
    u32 t0, t1, t2;
    int had_cursor;
    int cursor_overlap = 0;
    if (!g_fb || !panel || !panel->pixels || w <= 0 || h <= 0)
        return 0;
    t0 = GuiOs_GetTicks();
    had_cursor = g_cursor_visible;
    if (had_cursor) {
        cursor_overlap = !(g_cursor_saved_x1 <= x || g_cursor_saved_x0 >= x + w ||
                           g_cursor_saved_y1 <= y || g_cursor_saved_y0 >= y + h);
        if (cursor_overlap)
            fb_cursor_restore();
    }
    if (!fb_blit_panel_rect_fast(panel, x, y, w, h))
        return 0;
    t1 = GuiOs_GetTicks();
    GuiFramebuffer_PresentRect(g_fb, (u32)x, (u32)y, (u32)w, (u32)h);
    if (fb_cursor_can_overlay() && (!had_cursor || cursor_overlap))
        fb_cursor_show(g_mouse_x, g_mouse_y);
    t2 = GuiOs_GetTicks();
    g_stats.render_ms = (u32)(t1 - t0);
    g_stats.present_ms = (u32)(t2 - t1);
    g_stats.total_ms = (u32)(t2 - t0);
    g_stats.present_pixels = (unsigned int)(w * h);
    g_stats.last_ticks = t2;
    return 1;
}

int ImGuiFramebuffer_EnsureCursorVisible(int mouse_x, int mouse_y, unsigned int mouse_buttons)
{
    int expected_x0;
    int expected_y0;
    (void)mouse_buttons;
    if (!fb_cursor_can_overlay())
        return 0;
    g_mouse_x = mouse_x;
    g_mouse_y = mouse_y;
    g_mouse_buttons = mouse_buttons;
    expected_x0 = fb_clamp_i(mouse_x, 0, (int)g_fb->width);
    expected_y0 = fb_clamp_i(mouse_y, 0, (int)g_fb->height);
    if (!g_cursor_visible || g_cursor_saved_x0 != expected_x0 || g_cursor_saved_y0 != expected_y0)
        fb_cursor_show(mouse_x, mouse_y);
    g_stats.cursor_overlay_moves = g_cursor_overlay_moves_total;
    g_stats.cursor_overlay_pixels = g_cursor_overlay_pixels_total;
    g_stats.cached_cursor_frames = g_cached_cursor_frames_total;
    g_stats.last_ticks = GuiOs_GetTicks();
    return 1;
}

int ImGuiFramebuffer_CursorOverlayAvailable(void)
{
    return fb_cursor_can_overlay();
}

void ImGuiFramebuffer_SetClearColor(unsigned char r, unsigned char g, unsigned char b)
{
    g_clear_r = r;
    g_clear_g = g;
    g_clear_b = b;
}

void ImGuiFramebuffer_GetStats(imgui_framebuffer_stats_t *out)
{
    if (out)
        *out = g_stats;
}

void ImGuiFramebuffer_SetProfiling(int enabled)
{
    g_profile_enabled = enabled != 0;
}

void ImGuiFramebuffer_RequestFullRedraw(void)
{
    g_full_clear_request = 2;
}

void ImGuiFramebuffer_RequestFullClear(void)
{
    ImGuiFramebuffer_RequestFullRedraw();
}

void ImGuiFramebuffer_AddDirtyRect(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    fb_dirty_add(x, y, x + w, y + h);
}

void ImGuiFramebuffer_EnableDirtyRects(int enabled)
{
    g_dirty_rects_enabled = enabled != 0;
    if (!g_dirty_rects_enabled)
        g_full_clear_request = 2;
}
