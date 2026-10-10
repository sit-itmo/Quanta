/**
 * @file frontends/gui/opengl.cpp
 *
 * Default GUI backend: a GLFW window, an OpenGL 3.3 context and the official Dear ImGui
 * adapters.
 *
 * Backend: the windowing/graphics implementation of an interface. The frontend builds widgets
 * without changing how a metric is measured.
 *
 * Texture: a registered Alpha8 or RGBA32 image with a validated identifier. Creation, updates
 * and destruction follow the ImGui 1.92 protocol.
 */
// SPDX-License-Identifier: MIT
#include "backend.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "quanta/osal.h"
#include <GLFW/glfw3.h>
#include <new>

struct quanta_gui_backend
{
    GLFWwindow *window = nullptr;
    bool platform = false;
    bool renderer = false;
};

namespace
{
void glfw_error(int code, const char *description)
{
    quanta_fprintf(quanta_stderr, "quanta: GLFW error %d: %s\n", code,
                   description ? description : "unknown error");
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
        quanta_snprintf(error, capacity, "cannot allocate OpenGL backend");
        return false;
    }
    glfwSetErrorCallback(glfw_error);
    if (!glfwInit())
    {
        quanta_snprintf(error, capacity, "GLFW initialization failed; check display and graphics driver");
        delete backend;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    backend->window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!backend->window)
    {
        quanta_snprintf(error, capacity,
            "cannot create an OpenGL 3.3 window; use a graphics driver or build QUANTA_GUI_BACKEND=FRAMEBUFFER");
        quanta_gui_close(backend);
        return false;
    }
    glfwMakeContextCurrent(backend->window);
    glfwSwapInterval(1);
    backend->platform = ImGui_ImplGlfw_InitForOpenGL(backend->window, true);
    if (backend->platform) backend->renderer = ImGui_ImplOpenGL3_Init("#version 330");
    if (!backend->platform || !backend->renderer || !ImGui_ImplOpenGL3_CreateDeviceObjects())
    {
        quanta_snprintf(error, capacity, "ImGui GLFW/OpenGL initialization or shader creation failed");
        quanta_gui_close(backend);
        return false;
    }
    *out = backend;
    return true;
}

void quanta_gui_close(quanta_gui_backend *backend)
{
    if (!backend) return;
    if (backend->window) glfwMakeContextCurrent(backend->window);
    if (backend->renderer) ImGui_ImplOpenGL3_Shutdown();
    if (backend->platform) ImGui_ImplGlfw_Shutdown();
    if (backend->window) glfwDestroyWindow(backend->window);
    // Quanta has one product window and owns GLFW's process-level lifetime.
    glfwTerminate();
    delete backend;
}

int quanta_gui_poll(quanta_gui_backend *backend)
{
    glfwPollEvents();
    return glfwWindowShouldClose(backend->window) ? 0 : 1;
}

bool quanta_gui_new_frame(quanta_gui_backend *backend, float delta_seconds)
{
    int width, height;
    (void)delta_seconds; // The official GLFW backend uses its own monotonic clock.
    glfwGetFramebufferSize(backend->window, &width, &height);
    if (width <= 0 || height <= 0) return false;
    glfwMakeContextCurrent(backend->window);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    return true;
}

bool quanta_gui_render(quanta_gui_backend *backend, ImDrawData *data)
{
    int width, height;
    glfwGetFramebufferSize(backend->window, &width, &height);
    if (width <= 0 || height <= 0) return true;
    glfwMakeContextCurrent(backend->window);
    glViewport(0, 0, width, height);
    glClearColor(0.065f, 0.085f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(data);
    glfwSwapBuffers(backend->window);
    return true;
}

const char *quanta_gui_backend_name() { return "OpenGL 3.3 / GLFW"; }
