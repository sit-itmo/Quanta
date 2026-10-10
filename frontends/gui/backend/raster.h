/**
 * @file frontends/gui/backend/raster.h
 *
 * Surface, texture, clipping rectangle and vertex formats for the portable CPU rasterizer.
 *
 * Framebuffer: an in-memory pixel array. Pixels use 0x00RRGGBB and stride is measured in pixels;
 * presentation on a display is a separate operation.
 *
 * Texture: a registered Alpha8 or RGBA32 image with a validated identifier. Creation, updates
 * and destruction follow the ImGui 1.92 protocol.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_RASTER_H
#define QUANTA_RASTER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct quanta_vertex
{
    float x, y, u, v;
    uint32_t rgba;
} quanta_vertex;

typedef struct quanta_surface
{
    uint32_t *pixels;
    int width, height, stride;
} quanta_surface;

typedef struct quanta_texture
{
    const uint8_t *pixels;
    int width, height;
    int channels; /* 1: Alpha8 (white RGB); 4: RGBA32 bytes. */
    int nearest;  /* 0: bilinear; 1: nearest. */
} quanta_texture;

typedef struct quanta_clip
{
    int left, top, right, bottom;
} quanta_clip;

float quanta_raster_texel(const quanta_texture *, float u, float v, int channel);
void quanta_raster_clear(quanta_surface *, uint32_t rgb);
void quanta_raster_triangle(quanta_surface *, const quanta_clip *, const quanta_vertex vertices[3],
                            const quanta_texture *);
#ifdef __cplusplus
}
#endif
#endif
