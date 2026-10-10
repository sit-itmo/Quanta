/**
 * @file frontends/gui/backend/imgui_renderer.h
 *
 * Private ImGui CPU-renderer interface: lifetime, textures, frame rendering and a quad counter
 * for tests.
 *
 * Texture: a registered Alpha8 or RGBA32 image with a validated identifier. Creation, updates
 * and destruction follow the ImGui 1.92 protocol.
 *
 * Framebuffer: an in-memory pixel array. Pixels use 0x00RRGGBB and stride is measured in pixels;
 * presentation on a display is a separate operation.
 */
// SPDX-License-Identifier: MIT
#ifndef QUANTA_IMGUI_RENDERER_H
#define QUANTA_IMGUI_RENDERER_H
#include "imgui.h"
#include "raster.h"

// One renderer per current ImGui context, used on its UI thread only.
// Owns CPU copies of Alpha8/RGBA32 textures; no GPU/window dependency.
bool quanta_imgui_renderer_init();
void quanta_imgui_renderer_shutdown();
bool quanta_imgui_renderer_update_texture(ImTextureData *texture);
// Number of six-index quads handled by the portable fast path in the last frame.
size_t quanta_imgui_renderer_quad_count();
bool quanta_imgui_renderer_render(ImDrawData *data, quanta_surface *surface);
#endif
