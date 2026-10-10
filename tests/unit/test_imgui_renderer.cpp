// SPDX-License-Identifier: MIT
// Real Dear ImGui, without a window server. Validate pixels, not only exit codes.
#include "imgui_renderer.h"
#include "quanta/osal.h"
#include <cstdio>
#include <cstring>
#include <vector>

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
