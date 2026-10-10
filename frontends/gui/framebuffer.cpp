/**
 * @file frontends/gui/framebuffer.cpp
 *
 * Connect shared ImGui widgets to the CPU renderer: pixel buffer, input, resizing and native-
 * window presentation.
 *
 * Backend: the windowing/graphics implementation of an interface. The frontend builds widgets
 * without changing how a metric is measured.
 *
 * Framebuffer: an in-memory pixel array. Pixels use 0x00RRGGBB and stride is measured in pixels;
 * presentation on a display is a separate operation.
 */
// SPDX-License-Identifier: MIT
#include "backend.h"
#include "backend/window.h"
#include "backend/imgui_renderer.h"
#include "quanta/osal.h"
#include <algorithm>
#include <new>
#include <vector>

struct quanta_gui_backend
{
    quanta_window *window = nullptr;
    std::vector<uint32_t> pixels;
    int width = 0, height = 0;
    float dpi = 1.0f;
    bool renderer = false;
};
namespace
{
ImGuiKey key(uint32_t k)
{
    if (k >= 'a' && k <= 'z')
    {
        return (ImGuiKey)(ImGuiKey_A + (k - 'a'));
    }
    if (k >= 'A' && k <= 'Z')
    {
        return (ImGuiKey)(ImGuiKey_A + (k - 'A'));
    }
    if (k >= '0' && k <= '9')
    {
        return (ImGuiKey)(ImGuiKey_0 + (k - '0'));
    }
    switch (k)
    {
        case 0xff09:
            return ImGuiKey_Tab;
        case 0xff0d:
            return ImGuiKey_Enter;
        case 0xff1b:
            return ImGuiKey_Escape;
        case 0xff08:
            return ImGuiKey_Backspace;
        case 0xffff:
            return ImGuiKey_Delete;
        case 0xff63:
            return ImGuiKey_Insert;
        case 0xff50:
            return ImGuiKey_Home;
        case 0xff57:
            return ImGuiKey_End;
        case 0xff55:
            return ImGuiKey_PageUp;
        case 0xff56:
            return ImGuiKey_PageDown;
        case 0xff51:
            return ImGuiKey_LeftArrow;
        case 0xff52:
            return ImGuiKey_UpArrow;
        case 0xff53:
            return ImGuiKey_RightArrow;
        case 0xff54:
            return ImGuiKey_DownArrow;
        case 0xffe1:
            return ImGuiKey_LeftShift;
        case 0xffe2:
            return ImGuiKey_RightShift;
        case 0xffe3:
            return ImGuiKey_LeftCtrl;
        case 0xffe4:
            return ImGuiKey_RightCtrl;
        case 0xffe9:
            return ImGuiKey_LeftAlt;
        case 0xffea:
            return ImGuiKey_RightAlt;
        case 0xffeb:
            return ImGuiKey_LeftSuper;
        case 0xffec:
            return ImGuiKey_RightSuper;
        case ' ':
            return ImGuiKey_Space;
        case '-':
            return ImGuiKey_Minus;
        case '=':
            return ImGuiKey_Equal;
        case ',':
            return ImGuiKey_Comma;
        case '.':
            return ImGuiKey_Period;
        case '/':
            return ImGuiKey_Slash;
        case ';':
            return ImGuiKey_Semicolon;
        case '\'':
            return ImGuiKey_Apostrophe;
        case '[':
            return ImGuiKey_LeftBracket;
        case ']':
            return ImGuiKey_RightBracket;
        case '\\':
            return ImGuiKey_Backslash;
        case '`':
            return ImGuiKey_GraveAccent;
        default:
            return ImGuiKey_None;
    }
}

void input(ImGuiIO &io, const quanta_window_event &e)
{
    switch (e.kind)
    {
        case QUANTA_X_MOTION:
            io.AddMousePosEvent((float)e.x, (float)e.y);
            break;
        case QUANTA_X_BUTTON:
            io.AddMousePosEvent((float)e.x, (float)e.y);
            io.AddMouseButtonEvent(e.button, e.pressed != 0);
            break;
        case QUANTA_X_WHEEL:
            io.AddMouseWheelEvent((float)e.x, (float)e.y);
            break;
        case QUANTA_X_FOCUS:
            io.AddFocusEvent(e.pressed != 0);
            break;
        case QUANTA_X_KEY:
        {
            io.AddKeyEvent(ImGuiMod_Shift, (e.mods & QUANTA_X_SHIFT) != 0);
            io.AddKeyEvent(ImGuiMod_Ctrl, (e.mods & QUANTA_X_CTRL) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (e.mods & QUANTA_X_ALT) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (e.mods & QUANTA_X_SUPER) != 0);
            ImGuiKey k = key(e.keysym);
            if (k != ImGuiKey_None)
            {
                io.AddKeyEvent(k, e.pressed != 0);
            }
            if (e.text[0])
            {
                io.AddInputCharactersUTF8(e.text);
            }
            else if (e.codepoint)
            {
                io.AddInputCharacter(e.codepoint);
            }
            break;
        }
        default:
            break;
    }
}

}

bool quanta_gui_open(const char *title, int width, int height,
                     quanta_gui_backend **out, char *error, size_t capacity)
{
    if (!out) return false;
    *out = nullptr;
    auto *backend = new (std::nothrow) quanta_gui_backend;
    if (!backend)
    {
        quanta_snprintf(error, capacity, "cannot allocate framebuffer backend");
        return false;
    }
    backend->width = width;
    backend->height = height;
    if (!quanta_window_open(title, width, height, &backend->window, error, capacity))
    {
        delete backend;
        return false;
    }
    backend->dpi = std::max(1.0f, quanta_window_scale(backend->window));
    if (!quanta_imgui_renderer_init())
    {
        quanta_snprintf(error, capacity, "cannot initialize framebuffer renderer");
        quanta_gui_close(backend);
        return false;
    }
    backend->renderer = true;
    ImGui::GetIO().BackendPlatformName = "quanta_framebuffer_window";
    ImGui::GetMainViewport()->PlatformHandleRaw = quanta_window_native(backend->window);
    *out = backend;
    return true;
}

void quanta_gui_close(quanta_gui_backend *backend)
{
    if (!backend) return;
    if (backend->renderer) quanta_imgui_renderer_shutdown();
    ImGui::GetIO().BackendPlatformName = nullptr;
    ImGui::GetMainViewport()->PlatformHandleRaw = nullptr;
    if (backend->window) quanta_window_close(backend->window);
    delete backend;
}

int quanta_gui_poll(quanta_gui_backend *backend)
{
    quanta_window_event event;
    int available;
    while ((available = quanta_window_poll(backend->window, &event)) > 0)
    {
        if (event.kind == QUANTA_X_CLOSE) return 0;
        if (event.kind == QUANTA_X_RESIZE)
        {
            backend->width = event.width;
            backend->height = event.height;
        }
        else
        {
            if (event.kind == QUANTA_X_MOTION || event.kind == QUANTA_X_BUTTON)
            {
                event.x = (int)(event.x / backend->dpi);
                event.y = (int)(event.y / backend->dpi);
            }
            input(ImGui::GetIO(), event);
        }
    }
    if (available < 0 || backend->width > 4096 || backend->height > 4096) return -1;
    return 1;
}

bool quanta_gui_new_frame(quanta_gui_backend *backend, float delta_seconds)
{
    if (backend->width <= 0 || backend->height <= 0) return false;
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(backend->width / backend->dpi, backend->height / backend->dpi);
    io.DisplayFramebufferScale = ImVec2(backend->dpi, backend->dpi);
    io.DeltaTime = delta_seconds;
    return true;
}

bool quanta_gui_render(quanta_gui_backend *backend, ImDrawData *data)
{
    if (backend->width <= 0 || backend->height <= 0 ||
        backend->width > 4096 || backend->height > 4096) return false;
    backend->pixels.resize((size_t)backend->width * (size_t)backend->height);
    quanta_surface surface{backend->pixels.data(), backend->width, backend->height, backend->width};
    return quanta_imgui_renderer_render(data, &surface) &&
           quanta_window_present(backend->window, surface.pixels, surface.width, surface.height);
}

const char *quanta_gui_backend_name() { return "Framebuffer / CPU (portable_gui)"; }
