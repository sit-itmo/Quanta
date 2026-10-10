/* SPDX-License-Identifier: MIT
 * CPU rasterizer for Dear ImGui indexed, clipped, Alpha8/RGBA32-textured triangles.
 * Surface pixels are 0x00RRGGBB. Vertex color is ImGui's default 0xAABBGGRR.
 * Top-left coverage prevents double blending along shared triangle edges.
 */
#include "quanta/osal.h"
#include "raster.h"
#include <math.h>

static float edge(const quanta_vertex *a, const quanta_vertex *b, float x, float y)
{
    return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x);
}

static int top_left(const quanta_vertex *a, const quanta_vertex *b)
{
    float dy = b->y - a->y, dx = b->x - a->x;
    return dy < 0 || (dy == 0 && dx > 0);
}

static int maxi(int a, int b)
{
    return a > b ? a : b;
}

static int mini(int a, int b)
{
    return a < b ? a : b;
}

static unsigned component(uint32_t c, unsigned shift)
{
    return (c >> shift) & 255u;
}

static float texel(const quanta_texture *t, float u, float v, int channel)
{
    float x, y, fx, fy;
    int ix, iy, x1, y1;
    if (!t || !t->pixels || (t->channels == 1 && channel != 3))
    {
        return 255;
    }
    if (t->channels == 1)
    {
        channel = 0;
    }
    x = u * t->width - 0.5f;
    y = v * t->height - 0.5f;
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }
    if (x > t->width - 1)
    {
        x = (float)t->width - 1;
    }
    if (y > t->height - 1)
    {
        y = (float)t->height - 1;
    }
    if (t->nearest)
    {
        ix = mini((int)(x + 0.5f), t->width - 1);
        iy = mini((int)(y + 0.5f), t->height - 1);
        return t->pixels[((size_t)iy * (size_t)t->width + (size_t)ix) *
                         (size_t)t->channels + (size_t)channel];
    }
    ix = (int)x;
    iy = (int)y;
    fx = x - ix;
    fy = y - iy;
    x1 = mini(ix + 1, t->width - 1);
    y1 = mini(iy + 1, t->height - 1);
    return (1 - fy) *
               ((1 - fx) * t->pixels[((size_t)iy * (size_t)t->width + (size_t)ix) * (size_t)t->channels + (size_t)channel] + fx * t->pixels[((size_t)iy * (size_t)t->width + (size_t)x1) * (size_t)t->channels + (size_t)channel]) +
           fy * ((1 - fx) * t->pixels[((size_t)y1 * (size_t)t->width + (size_t)ix) * (size_t)t->channels + (size_t)channel] + fx * t->pixels[((size_t)y1 * (size_t)t->width + (size_t)x1) * (size_t)t->channels + (size_t)channel]);
}

void quanta_raster_clear(quanta_surface *s, uint32_t color)
{
    int y, x;
    if (!s || !s->pixels)
    {
        return;
    }
    for (y = 0; y < s->height; y++)
    {
        for (x = 0; x < s->width; x++)
        {
            s->pixels[y * s->stride + x] = color;
        }
    }
}

void quanta_raster_triangle(quanta_surface *s, const quanta_clip *clip, const quanta_vertex vv[3],
                            const quanta_texture *tex)
{
    quanta_vertex a = vv[0], b = vv[1], c = vv[2], tmp;
    float area, invarea;
    int x0, x1, y0, y1, x, y, tl0, tl1, tl2, flat;
    if (!s || !s->pixels || s->width <= 0 || s->height <= 0 || s->stride < s->width)
    {
        return;
    }
    if (tex && tex->pixels && (tex->width <= 0 || tex->height <= 0 ||
        (tex->channels != 1 && tex->channels != 4)))
    {
        return;
    }
    if (!quanta_isfinite(a.x) || !quanta_isfinite(a.y) || !quanta_isfinite(b.x) || !quanta_isfinite(b.y) || !quanta_isfinite(c.x) ||
        !quanta_isfinite(c.y) || !quanta_isfinite(a.u) || !quanta_isfinite(a.v) || !quanta_isfinite(b.u) || !quanta_isfinite(b.v) ||
        !quanta_isfinite(c.u) || !quanta_isfinite(c.v))
    {
        return;
    }
    area = edge(&a, &b, c.x, c.y);
    if (!quanta_isfinite(area) || fabsf(area) < 0.00001f)
    {
        return;
    }
    if (area < 0)
    {
        tmp = b;
        b = c;
        c = tmp;
        area = -area;
    }
    invarea = 1 / area;
    /* Bound in floating point before integer conversion, including hostile data.
     */
    x0 = (int)fmaxf(0, fminf((float)s->width, floorf(fminf(a.x, fminf(b.x, c.x)))));
    y0 = (int)fmaxf(0, fminf((float)s->height, floorf(fminf(a.y, fminf(b.y, c.y)))));
    x1 = (int)fmaxf(0, fminf((float)s->width, ceilf(fmaxf(a.x, fmaxf(b.x, c.x)))));
    y1 = (int)fmaxf(0, fminf((float)s->height, ceilf(fmaxf(a.y, fmaxf(b.y, c.y)))));
    if (clip)
    {
        x0 = maxi(x0, clip->left);
        y0 = maxi(y0, clip->top);
        x1 = mini(x1, clip->right);
        y1 = mini(y1, clip->bottom);
    }
    tl0 = top_left(&b, &c);
    tl1 = top_left(&c, &a);
    tl2 = top_left(&a, &b);
    flat = a.rgba == b.rgba && a.rgba == c.rgba;
    for (y = y0; y < y1; y++)
    {
        for (x = x0; x < x1; x++)
        {
            float e0 = edge(&b, &c, x + 0.5f, y + 0.5f), e1 = edge(&c, &a, x + 0.5f, y + 0.5f),
                  e2 = edge(&a, &b, x + 0.5f, y + 0.5f);
            float w0, w1, w2, u, v, alpha;
            unsigned r, g, blue, ai, ia;
            uint32_t old;
            if (e0 < 0 || (e0 == 0 && !tl0) || e1 < 0 || (e1 == 0 && !tl1) || e2 < 0 ||
                (e2 == 0 && !tl2))
            {
                continue;
            }
            w0 = e0 * invarea;
            w1 = e1 * invarea;
            w2 = e2 * invarea;
            u = w0 * a.u + w1 * b.u + w2 * c.u;
            v = w0 * a.v + w1 * b.v + w2 * c.v;
            if (flat)
            {
                r = component(a.rgba, 0);
                g = component(a.rgba, 8);
                blue = component(a.rgba, 16);
                alpha = (float)component(a.rgba, 24);
            }
            else
            {
                r = (unsigned)(w0 * component(a.rgba, 0) + w1 * component(b.rgba, 0) +
                               w2 * component(c.rgba, 0));
                g = (unsigned)(w0 * component(a.rgba, 8) + w1 * component(b.rgba, 8) +
                               w2 * component(c.rgba, 8));
                blue = (unsigned)(w0 * component(a.rgba, 16) + w1 * component(b.rgba, 16) +
                                  w2 * component(c.rgba, 16));
                alpha = w0 * component(a.rgba, 24) + w1 * component(b.rgba, 24) +
                        w2 * component(c.rgba, 24);
            }
            if (tex && tex->pixels && tex->channels == 4)
            {
                r = (unsigned)(r * texel(tex, u, v, 0) / 255.0f + 0.5f);
                g = (unsigned)(g * texel(tex, u, v, 1) / 255.0f + 0.5f);
                blue = (unsigned)(blue * texel(tex, u, v, 2) / 255.0f + 0.5f);
            }
            ai = (unsigned)(alpha * texel(tex, u, v, 3) / 255.0f + 0.5f);
            if (ai > 255)
            {
                ai = 255;
            }
            if (!ai)
            {
                continue;
            }
            old = s->pixels[y * s->stride + x];
            ia = 255 - ai;
            r = (r * ai + component(old, 16) * ia + 127) / 255;
            g = (g * ai + component(old, 8) * ia + 127) / 255;
            blue = (blue * ai + component(old, 0) * ia + 127) / 255;
            s->pixels[y * s->stride + x] = (r << 16) | (g << 8) | blue;
        }
    }
}
