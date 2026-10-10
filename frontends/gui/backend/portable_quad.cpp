/**
 * @file frontends/gui/backend/portable_quad.cpp
 *
 * Adapted algorithms from the supplied portable_gui: recognize rectangles formed by two
 * triangles, fill rows and draw textured quads.
 *
 * Framebuffer: an in-memory pixel array. Pixels use 0x00RRGGBB and stride is measured in pixels;
 * presentation on a display is a separate operation.
 *
 * Texture: a registered Alpha8 or RGBA32 image with a validated identifier. Creation, updates
 * and destruction follow the ImGui 1.92 protocol.
 */
/* Derived from user-supplied portable_gui/imgui_backend_framebuffer.cpp.
 * Original archive does not specify a license; see vendor/PORTABLE_GUI.md.
 * Quanta adaptation: no MiniOS/global framebuffer state; checked texture IDs,
 * exact triangle-equivalent pixel coverage, dynamic ImGui 1.92 textures.
 */
#include "portable_quad.h"
#include "quanta/osal.h"
#include <algorithm>
#include <cmath>

namespace
{
#define FB_EPSILON 0.00001f
struct FbQuad { ImDrawVert tl, tr, br, bl; };
static int fb_vertex_is_valid(const ImDrawVert &v)
{
    return quanta_isfinite(v.pos.x) && quanta_isfinite(v.pos.y) &&
           quanta_isfinite(v.uv.x) && quanta_isfinite(v.uv.y) &&
           std::fabs(v.pos.x) <= 16777216.0f && std::fabs(v.pos.y) <= 16777216.0f;
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
    /* Both triangles must meet on a diagonal, not on a rectangle edge.
     * Four distinct corners alone would also accept overlapping triangles. */
    if (v[0]->pos.x == v[2]->pos.x || v[0]->pos.y == v[2]->pos.y)
        return 0;
    return mask == 0x0fU;
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

static unsigned component(uint32_t value, unsigned shift) { return (value >> shift) & 255u; }
static uint32_t blend(uint32_t old, unsigned r, unsigned g, unsigned b, unsigned alpha)
{
    const unsigned inverse = 255u - alpha;
    r = (r * alpha + component(old, 16) * inverse + 127u) / 255u;
    g = (g * alpha + component(old, 8) * inverse + 127u) / 255u;
    b = (b * alpha + component(old, 0) * inverse + 127u) / 255u;
    return (r << 16) | (g << 8) | b;
}
static int bound(float edge, int limit)
{
    // Pixel centers follow the same top-left convention as the triangle fallback.
    return (int)std::max(0.0, std::min((double)limit, std::ceil((double)edge - 0.5)));
}
}

bool quanta_portable_quad(quanta_surface *surface, const quanta_clip &clip,
                          const ImDrawList *list, const ImDrawCmd &cmd, unsigned offset,
                          ImVec2 origin, ImVec2 scale, const quanta_texture &texture)
{
    if (!surface || !surface->pixels || surface->width <= 0 || surface->height <= 0 ||
        surface->stride < surface->width || !list || offset > cmd.ElemCount ||
        cmd.ElemCount - offset < 6 || cmd.IdxOffset > (unsigned)list->IdxBuffer.Size ||
        cmd.ElemCount > (unsigned)list->IdxBuffer.Size - cmd.IdxOffset) return false;
    FbQuad q;
    const ImDrawIdx *indices = list->IdxBuffer.Data + cmd.IdxOffset + offset;
    if (!fb_try_make_quad_direct(list->VtxBuffer.Data, (unsigned)list->VtxBuffer.Size,
                                indices, cmd.VtxOffset, &q) &&
        !fb_try_make_quad(list->VtxBuffer.Data, (unsigned)list->VtxBuffer.Size,
                         indices, cmd.VtxOffset, &q)) return false;
    // Do not flatten a warped UV quadrilateral or non-axis-aligned triangle pair.
    if (q.tl.pos.x != q.bl.pos.x || q.tr.pos.x != q.br.pos.x ||
        q.tl.pos.y != q.tr.pos.y || q.bl.pos.y != q.br.pos.y ||
        q.tl.uv.x != q.bl.uv.x || q.tr.uv.x != q.br.uv.x ||
        q.tl.uv.y != q.tr.uv.y || q.bl.uv.y != q.br.uv.y) return false;
    for (ImDrawVert *v : {&q.tl, &q.tr, &q.br, &q.bl})
    {
        v->pos.x = (v->pos.x - origin.x) * scale.x;
        v->pos.y = (v->pos.y - origin.y) * scale.y;
        if (!fb_vertex_is_valid(*v)) return false;
    }
    const float width = q.tr.pos.x - q.tl.pos.x, height = q.bl.pos.y - q.tl.pos.y;
    if (width <= 0 || height <= 0) return false;
    const int x0 = std::max(bound(q.tl.pos.x, surface->width), clip.left);
    const int x1 = std::min(bound(q.br.pos.x, surface->width), clip.right);
    const int y0 = std::max(bound(q.tl.pos.y, surface->height), clip.top);
    const int y1 = std::min(bound(q.br.pos.y, surface->height), clip.bottom);
    if (x0 >= x1 || y0 >= y1) return true;
    const unsigned cr = component(q.tl.col, IM_COL32_R_SHIFT);
    const unsigned cg = component(q.tl.col, IM_COL32_G_SHIFT);
    const unsigned cb = component(q.tl.col, IM_COL32_B_SHIFT);
    const unsigned ca = component(q.tl.col, IM_COL32_A_SHIFT);
    if (!ca) return true;
    if (q.tl.uv.x == q.br.uv.x && q.tl.uv.y == q.br.uv.y)
    {
        const float u = q.tl.uv.x, v = q.tl.uv.y;
        const unsigned r = (unsigned)(cr * quanta_raster_texel(&texture, u, v, 0) / 255.0f + 0.5f);
        const unsigned g = (unsigned)(cg * quanta_raster_texel(&texture, u, v, 1) / 255.0f + 0.5f);
        const unsigned b = (unsigned)(cb * quanta_raster_texel(&texture, u, v, 2) / 255.0f + 0.5f);
        const unsigned a = (unsigned)(ca * quanta_raster_texel(&texture, u, v, 3) / 255.0f + 0.5f);
        if (a == 255)
        {
            uint32_t *first = surface->pixels + (size_t)y0 * surface->stride + x0;
            fb_fill_u32_row(first, (r << 16) | (g << 8) | b, (unsigned)(x1 - x0));
            for (int y = y0 + 1; y < y1; ++y)
                quanta_memcpy(surface->pixels + (size_t)y * surface->stride + x0,
                               first, (size_t)(x1 - x0) * sizeof(uint32_t));
        }
        else if (a)
        {
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                {
                    uint32_t &pixel = surface->pixels[(size_t)y * surface->stride + x];
                    pixel = blend(pixel, r, g, b, a);
                }
        }
        return true;
    }
    // Atlas glyphs and image panels share one checked texture sampler. No alpha
    // threshold: unlike the firmware fast-mono option, antialiasing is preserved.
    for (int y = y0; y < y1; ++y)
    {
        const float fy = ((float)y + 0.5f - q.tl.pos.y) / height;
        const float v = q.tl.uv.y + (q.bl.uv.y - q.tl.uv.y) * fy;
        for (int x = x0; x < x1; ++x)
        {
            const float fx = ((float)x + 0.5f - q.tl.pos.x) / width;
            const float u = q.tl.uv.x + (q.tr.uv.x - q.tl.uv.x) * fx;
            const unsigned r = (unsigned)(cr * quanta_raster_texel(&texture, u, v, 0) / 255.0f + 0.5f);
            const unsigned g = (unsigned)(cg * quanta_raster_texel(&texture, u, v, 1) / 255.0f + 0.5f);
            const unsigned b = (unsigned)(cb * quanta_raster_texel(&texture, u, v, 2) / 255.0f + 0.5f);
            const unsigned a = (unsigned)(ca * quanta_raster_texel(&texture, u, v, 3) / 255.0f + 0.5f);
            uint32_t &pixel = surface->pixels[(size_t)y * surface->stride + x];
            if (a) pixel = blend(pixel, r, g, b, a);
        }
    }
    return true;
}
