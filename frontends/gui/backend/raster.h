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

void quanta_raster_clear(quanta_surface *, uint32_t rgb);
void quanta_raster_triangle(quanta_surface *, const quanta_clip *, const quanta_vertex vertices[3],
                            const quanta_texture *);
#ifdef __cplusplus
}
#endif
#endif
