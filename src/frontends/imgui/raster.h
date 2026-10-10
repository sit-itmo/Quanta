/* SPDX-License-Identifier: MIT */
#ifndef INV_RASTER_H
#define INV_RASTER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct inv_vertex {float x,y,u,v;uint32_t rgba;} inv_vertex;
typedef struct inv_surface {uint32_t *pixels;int width,height,stride;} inv_surface;
typedef struct inv_texture {const uint8_t *alpha;int width,height;} inv_texture;
typedef struct inv_clip {int left,top,right,bottom;} inv_clip;
void inv_raster_clear(inv_surface *,uint32_t rgb);
void inv_raster_triangle(inv_surface *,const inv_clip *,const inv_vertex vertices[3],const inv_texture *);
#ifdef __cplusplus
}
#endif
#endif
