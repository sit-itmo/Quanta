/**
 * @file frontends/gui/backend/portable_quad.h
 *
 * Fast-quad contract: true means six indices were handled; false requests the standard triangle
 * fallback.
 *
 * Framebuffer: an in-memory pixel array. Pixels use 0x00RRGGBB and stride is measured in pixels;
 * presentation on a display is a separate operation.
 *
 * Texture: a registered Alpha8 or RGBA32 image with a validated identifier. Creation, updates
 * and destruction follow the ImGui 1.92 protocol.
 */
// Quanta adaptation of the supplied portable_gui framebuffer algorithms.
#ifndef QUANTA_PORTABLE_QUAD_H
#define QUANTA_PORTABLE_QUAD_H
#include "imgui.h"
#include "raster.h"
// true: six indices were handled (including fully clipped primitives).
// false: caller renders the same indices as individually clipped triangles.
bool quanta_portable_quad(quanta_surface *, const quanta_clip &, const ImDrawList *,
                          const ImDrawCmd &, unsigned, ImVec2, ImVec2, const quanta_texture &);
#endif
