/**
 * @file frontends/gui/backend.h
 *
 * Private GUI-backend contract: open/close a window, receive input, begin a frame and render it.
 * All calls run on the UI thread.
 *
 * Backend: the windowing/graphics implementation of an interface. The frontend builds widgets
 * without changing how a metric is measured.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
// SPDX-License-Identifier: MIT
#ifndef QUANTA_GUI_BACKEND_H
#define QUANTA_GUI_BACKEND_H
#include <stddef.h>
struct ImDrawData;
struct quanta_gui_backend;
// All operations run on the UI thread with the owning ImGui context current.
// open/close manage window/platform/renderer resources, never the ImGui context.
bool quanta_gui_open(const char *title, int width, int height,
                     quanta_gui_backend **out, char *error, size_t capacity);
void quanta_gui_close(quanta_gui_backend *backend);
int quanta_gui_poll(quanta_gui_backend *backend); // 1 active, 0 close, -1 error
bool quanta_gui_new_frame(quanta_gui_backend *backend, float delta_seconds);
bool quanta_gui_render(quanta_gui_backend *backend, ImDrawData *data);
const char *quanta_gui_backend_name();
#endif
