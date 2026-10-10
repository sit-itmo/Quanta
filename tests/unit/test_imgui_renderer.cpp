/**
 * @file tests/unit/test_imgui_renderer.cpp
 *
 * Regression tests for real ImGui textures, pixels, fast quads, clipping and lifetime
 * management.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
// SPDX-License-Identifier: MIT
// Real Dear ImGui, without a window server. Validate pixels, not only exit codes.
#include "imgui_renderer.h"
#include "portable_quad.h"
#include "quanta/osal.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <limits>

static unsigned checks, failures;
#define CHECK(test) do { ++checks; if (!(test)) { ++failures; \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); } } while (0)

static void begin_frame()
{
    ImGui::GetIO().DisplaySize = ImVec2(128, 128);
    ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
}

static bool draw(quanta_surface *surface)
{
    ImGui::Render();
    return quanta_imgui_renderer_render(ImGui::GetDrawData(), surface);
}

int main()
{
    ImTextureData colored, alpha, unused;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    CHECK(quanta_imgui_renderer_init());
    CHECK(!quanta_imgui_renderer_init());
    CHECK((ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0);
    std::vector<uint32_t> pixels(128 * 128);
    quanta_surface surface = {pixels.data(), 128, 128, 128};

    colored.Create(ImTextureFormat_RGBA32, 2, 2);
    for (int i = 0; i < 4; ++i)
    {
        const unsigned char red[] = {255, 0, 0, 255};
        std::memcpy(colored.Pixels + i * 4, red, 4);
    }
    colored.SetStatus(ImTextureStatus_WantCreate);
    CHECK(quanta_imgui_renderer_update_texture(&colored));
    CHECK(colored.Status == ImTextureStatus_OK && colored.GetTexID() != ImTextureID_Invalid);
    begin_frame();
    ImGui::GetForegroundDrawList()->AddImage(colored.GetTexRef(), ImVec2(8, 8), ImVec2(40, 40));
    CHECK(draw(&surface));
    CHECK(pixels[16 * 128 + 16] == 0xff0000u);
    CHECK(quanta_imgui_renderer_quad_count() > 0);

    // A partial update must retain untouched rows and use the full source pitch.
    const unsigned char green[] = {0, 255, 0, 255};
    std::memcpy(colored.GetPixelsAt(1, 0), green, 4);
    colored.UpdateRect = {1, 0, 1, 1};
    colored.SetStatus(ImTextureStatus_WantUpdates);
    const ImTextureID before_update = colored.GetTexID();
    CHECK(quanta_imgui_renderer_update_texture(&colored));
    CHECK(colored.GetTexID() == before_update);
    begin_frame();
    ImDrawList *list = ImGui::GetForegroundDrawList();
    list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest);
    list->AddImage(colored.GetTexRef(), ImVec2(8, 8), ImVec2(40, 40));
    list->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState);
    CHECK(draw(&surface));
    CHECK(pixels[12 * 128 + 12] == 0xff0000u);
    CHECK(pixels[12 * 128 + 36] == 0x00ff00u);
    CHECK(pixels[36 * 128 + 36] == 0xff0000u);
    colored.UpdateRect = {1, 0, 2, 1};
    colored.SetStatus(ImTextureStatus_WantUpdates);
    CHECK(!quanta_imgui_renderer_update_texture(&colored));
    colored.UpdateRect = {1, 0, 1, 1};
    CHECK(quanta_imgui_renderer_update_texture(&colored));

    alpha.Create(ImTextureFormat_Alpha8, 1, 1);
    alpha.Pixels[0] = 128;
    alpha.SetStatus(ImTextureStatus_WantCreate);
    CHECK(quanta_imgui_renderer_update_texture(&alpha));
    begin_frame();
    list = ImGui::GetForegroundDrawList();
    list->AddImage(alpha.GetTexRef(), ImVec2(8, 8), ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1),
                   IM_COL32(255, 255, 255, 255));
    CHECK(draw(&surface));
    const uint32_t blended = pixels[16 * 128 + 16];
    CHECK(((blended >> 16) & 255) >= 135 && ((blended >> 16) & 255) <= 137);
    CHECK(blended != 0xffffffu && blended != 0x101721u);

    // The six-index fast path recognizes only a real axis-aligned rectangle.
    // A crossed pair with all four corners must fall back to two triangles.
    {
        ImDrawList quad_list(ImGui::GetDrawListSharedData());
        quad_list.VtxBuffer.resize(4);
        quad_list.IdxBuffer.resize(6);
        const ImVec2 positions[] = {{8, 8}, {40, 8}, {40, 40}, {8, 40}};
        const ImDrawIdx indices[] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 4; ++i)
        {
            quad_list.VtxBuffer[i].pos = positions[i];
            quad_list.VtxBuffer[i].uv = ImVec2(0, 0);
            quad_list.VtxBuffer[i].col = IM_COL32_WHITE;
        }
        for (int i = 0; i < 6; ++i) quad_list.IdxBuffer[i] = indices[i];
        ImDrawCmd command;
        command.ElemCount = 6;
        const unsigned char white[] = {255, 255, 255, 255};
        const quanta_texture texture = {white, 1, 1, 4, 1};
        const quanta_clip clip = {0, 0, 128, 128};
        quanta_raster_clear(&surface, 0);
        CHECK(quanta_portable_quad(&surface, clip, &quad_list, command, 0,
                                  ImVec2(0, 0), ImVec2(1, 1), texture));
        CHECK(pixels[20 * 128 + 20] == 0xffffffu);
        CHECK(pixels[7 * 128 + 20] == 0);
        CHECK(pixels[40 * 128 + 20] == 0);
        quad_list.VtxBuffer[1].col = IM_COL32(255, 0, 0, 255);
        CHECK(!quanta_portable_quad(&surface, clip, &quad_list, command, 0,
                                   ImVec2(0, 0), ImVec2(1, 1), texture));
        quad_list.VtxBuffer[1].col = IM_COL32_WHITE;
        quad_list.IdxBuffer[1] = 2;
        quad_list.IdxBuffer[2] = 1;
        quad_list.IdxBuffer[4] = 1;
        CHECK(!quanta_portable_quad(&surface, clip, &quad_list, command, 0,
                                   ImVec2(0, 0), ImVec2(1, 1), texture));
        for (int i = 0; i < 6; ++i) quad_list.IdxBuffer[i] = indices[i];
        command.VtxOffset = ~0u;
        CHECK(!quanta_portable_quad(&surface, clip, &quad_list, command, 0,
                                   ImVec2(0, 0), ImVec2(1, 1), texture));
        command.VtxOffset = 0;
        quad_list.VtxBuffer[0].pos.x = std::numeric_limits<float>::infinity();
        CHECK(!quanta_portable_quad(&surface, clip, &quad_list, command, 0,
                                   ImVec2(0, 0), ImVec2(1, 1), texture));
    }

    // The active reset callback and invalid clipping bounds are checked explicitly.
    begin_frame();
    list = ImGui::GetForegroundDrawList();
    list->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState);
    list->AddRectFilled(ImVec2(8, 8), ImVec2(40, 40), IM_COL32_WHITE);
    CHECK(draw(&surface));
    CHECK(pixels[20 * 128 + 20] == 0xffffffu);
    ImDrawCmd *bad_clip = nullptr;
    for (ImDrawCmd &command : list->CmdBuffer)
        if (!command.UserCallback && command.ElemCount) { bad_clip = &command; break; }
    CHECK(bad_clip != nullptr);
    if (bad_clip)
    {
        const ImVec4 saved_clip = bad_clip->ClipRect;
        bad_clip->ClipRect.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!quanta_imgui_renderer_render(ImGui::GetDrawData(), &surface));
        bad_clip->ClipRect = saved_clip;
    }

    // Font atlas texture creation/update at two sizes. No obsolete atlas API.
    for (int frame = 0; frame < 3; ++frame)
    {
        begin_frame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(128, 128));
        ImGui::Begin("Font test", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushFont(nullptr, frame == 0 ? 13.0f : 23.0f);
        ImGui::TextUnformatted(frame == 0 ? "ASCII" : "Dynamic atlas 123");
        ImGui::PopFont();
        ImGui::End();
        CHECK(draw(&surface));
        unsigned lit = 0;
        for (uint32_t color : pixels)
        {
            if ((color & 255) > 150 && ((color >> 8) & 255) > 150) ++lit;
        }
        CHECK(lit > 50);
        CHECK(ImGui::GetIO().Fonts->TexData->Status == ImTextureStatus_OK);
    }

    // Force >64K vertices: 16-bit indices must work via command VtxOffset.
    begin_frame();
    list = ImGui::GetForegroundDrawList();
    for (int i = 0; i < 17000; ++i)
    {
        list->AddRectFilled(ImVec2(-100, -100), ImVec2(-90, -90), IM_COL32_WHITE);
    }
    list->AddRectFilled(ImVec2(12, 12), ImVec2(32, 32), IM_COL32_WHITE);
    CHECK(draw(&surface));
    CHECK(ImGui::GetDrawData()->TotalVtxCount > 65536);
    CHECK(pixels[20 * 128 + 20] == 0xffffffu);

    unused.SetStatus(ImTextureStatus_WantDestroy);
    CHECK(quanta_imgui_renderer_update_texture(&unused));
    CHECK(unused.Status == ImTextureStatus_Destroyed);

    colored.WantDestroyNextFrame = true;
    colored.UnusedFrames = 1;
    colored.SetStatus(ImTextureStatus_WantDestroy);
    CHECK(quanta_imgui_renderer_update_texture(&colored));
    CHECK(colored.GetTexID() == ImTextureID_Invalid && colored.BackendUserData == nullptr);
    CHECK(colored.Status == ImTextureStatus_Destroyed);
    quanta_imgui_renderer_shutdown();
    CHECK(alpha.BackendUserData == nullptr);
    CHECK(ImGui::GetIO().BackendRendererUserData == nullptr);
    quanta_imgui_renderer_shutdown(); // idempotent cleanup, including failed setup
    ImGui::DestroyContext();
    std::printf("ImGui 1.92 renderer: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
