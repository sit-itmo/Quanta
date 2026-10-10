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
bool quanta_imgui_renderer_render(ImDrawData *data, quanta_surface *surface);
#endif
