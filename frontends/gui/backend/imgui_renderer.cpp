// SPDX-License-Identifier: MIT
// Dear ImGui 1.92 texture protocol + indexed software drawing. Textures are
// copied, so atlas growth/pixel reallocation cannot leave a dangling pointer.
#include "imgui_renderer.h"
#include "quanta/osal.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace
{
const int texture_limit = 8192;

struct Texture
{
    quanta_texture view;
    ImTextureData *owner;
    Texture *next;
};

struct Renderer
{
    Texture *textures;
    bool nearest;
};

Renderer *renderer()
{
    return static_cast<Renderer *>(ImGui::GetIO().BackendRendererUserData);
}

void reset_state(const ImDrawList *, const ImDrawCmd *)
{
    renderer()->nearest = false;
}

void sampler_nearest(const ImDrawList *, const ImDrawCmd *)
{
    renderer()->nearest = true;
}

ImTextureID texture_id(Texture *texture)
{
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture));
}

Texture *find_texture(ImTextureID id)
{
    Renderer *state = renderer();
    for (Texture *texture = state ? state->textures : nullptr; texture; texture = texture->next)
    {
        if (texture_id(texture) == id)
        {
            return texture;
        }
    }
    return nullptr;
}

void destroy_texture(Texture *texture)
{
    Texture **link = &renderer()->textures;
    while (*link && *link != texture)
    {
        link = &(*link)->next;
    }
    if (*link)
    {
        *link = texture->next;
        texture->owner->BackendUserData = nullptr;
        texture->owner->SetTexID(ImTextureID_Invalid);
        texture->owner->SetStatus(ImTextureStatus_Destroyed);
        texture->~Texture();
        quanta_free(texture);
    }
}

int clip_coordinate(float value, int limit, bool upper)
{
    const double bounded = std::max(0.0, std::min(static_cast<double>(limit),
                                                static_cast<double>(value)));
    return static_cast<int>(upper ? std::ceil(bounded) : std::floor(bounded));
}
} // namespace

bool quanta_imgui_renderer_init()
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.BackendRendererUserData)
    {
        return false;
    }
    void *memory = quanta_malloc(sizeof(Renderer));
    if (!memory)
    {
        return false;
    }
    io.BackendRendererUserData = new (memory) Renderer{};
    io.BackendRendererName = "quanta_software_1.92";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth = texture_limit;
    ImGui::GetPlatformIO().Renderer_TextureMaxHeight = texture_limit;
    ImGui::GetPlatformIO().DrawCallback_ResetRenderState = reset_state;
    ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear = reset_state;
    ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest = sampler_nearest;
    // Glyphs are normally monochrome. Colored custom textures are also supported.
    io.Fonts->TexDesiredFormat = ImTextureFormat_Alpha8;
    return true;
}

void quanta_imgui_renderer_shutdown()
{
    Renderer *state = renderer();
    if (!state)
    {
        return;
    }
    while (state->textures)
    {
        destroy_texture(state->textures);
    }
    state->~Renderer();
    quanta_free(state);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::GetPlatformIO().DrawCallback_ResetRenderState = nullptr;
    ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear = nullptr;
    ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendRendererName = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
}

bool quanta_imgui_renderer_update_texture(ImTextureData *tex)
{
    if (!renderer() || !tex)
    {
        return false;
    }
    if (tex->Status == ImTextureStatus_OK || tex->Status == ImTextureStatus_Destroyed)
    {
        return true;
    }
    Texture *texture = find_texture(tex->GetTexID());
    if (tex->Status == ImTextureStatus_WantDestroy)
    {
        // A texture may be discarded before its first upload.
        if (!texture && tex->GetTexID() == ImTextureID_Invalid && !tex->BackendUserData)
        {
            tex->SetStatus(ImTextureStatus_Destroyed);
            return true;
        }
        if (!texture || texture->owner != tex)
        {
            return false;
        }
        if (tex->UnusedFrames > 0)
        {
            destroy_texture(texture);
        }
        return true;
    }
    const int channels = tex->Format == ImTextureFormat_Alpha8 ? 1 :
                         tex->Format == ImTextureFormat_RGBA32 ? 4 : 0;
    if (!channels || channels != tex->BytesPerPixel || !tex->Pixels || tex->Width <= 0 ||
        tex->Height <= 0 || tex->Width > texture_limit || tex->Height > texture_limit)
    {
        return false;
    }
    const size_t pitch = static_cast<size_t>(tex->Width) * static_cast<size_t>(channels);
    const size_t bytes = pitch * static_cast<size_t>(tex->Height);
    if (bytes > SIZE_MAX - sizeof(Texture))
    {
        return false;
    }
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        if (texture || tex->BackendUserData || tex->GetTexID() != ImTextureID_Invalid)
        {
            return false;
        }
        void *memory = quanta_malloc(sizeof(Texture) + bytes);
        if (!memory)
        {
            return false;
        }
        texture = new (memory) Texture{};
        unsigned char *pixels = reinterpret_cast<unsigned char *>(texture + 1);
        quanta_memcpy(pixels, tex->Pixels, bytes);
        texture->view = {pixels, tex->Width, tex->Height, channels, 0};
        texture->owner = tex;
        texture->next = renderer()->textures;
        renderer()->textures = texture;
        tex->BackendUserData = texture;
        tex->SetTexID(texture_id(texture));
        tex->SetStatus(ImTextureStatus_OK);
        return true;
    }
    if (tex->Status != ImTextureStatus_WantUpdates || !texture || texture->owner != tex ||
        texture->view.width != tex->Width || texture->view.height != tex->Height ||
        texture->view.channels != channels)
    {
        return false;
    }
    const ImTextureRect &rect = tex->UpdateRect;
    if (static_cast<int>(rect.x) + rect.w > tex->Width ||
        static_cast<int>(rect.y) + rect.h > tex->Height)
    {
        return false;
    }
    unsigned char *pixels = reinterpret_cast<unsigned char *>(texture + 1);
    for (unsigned y = rect.y; y < static_cast<unsigned>(rect.y) + rect.h; ++y)
    {
        const size_t offset = static_cast<size_t>(y) * pitch + static_cast<size_t>(rect.x) * channels;
        quanta_memcpy(pixels + offset, tex->Pixels + offset, static_cast<size_t>(rect.w) * channels);
    }
    tex->SetStatus(ImTextureStatus_OK);
    return true;
}

bool quanta_imgui_renderer_render(ImDrawData *data, quanta_surface *surface)
{
    if (!renderer() || !data || !surface || !surface->pixels || surface->width <= 0 ||
        surface->height <= 0 || surface->stride < surface->width)
    {
        return false;
    }
    if (data->Textures)
    {
        for (ImTextureData *tex : *data->Textures)
        {
            if (!quanta_imgui_renderer_update_texture(tex))
            {
                return false;
            }
        }
    }
    renderer()->nearest = false;
    quanta_raster_clear(surface, 0x101721);
    const ImVec2 pos = data->DisplayPos, scale = data->FramebufferScale;
    if (!quanta_isfinite(pos.x) || !quanta_isfinite(pos.y) || !quanta_isfinite(scale.x) ||
        !quanta_isfinite(scale.y) || scale.x <= 0 || scale.y <= 0)
    {
        return false;
    }
    for (const ImDrawList *list : data->CmdLists)
    {
        for (const ImDrawCmd &cmd : list->CmdBuffer)
        {
            if (cmd.UserCallback)
            {
                cmd.UserCallback(list, &cmd);
                continue;
            }
            if (!cmd.ElemCount)
            {
                continue;
            }
            Texture *texture = find_texture(cmd.GetTexID());
            if (!texture || cmd.IdxOffset > static_cast<unsigned>(list->IdxBuffer.Size) ||
                cmd.ElemCount > static_cast<unsigned>(list->IdxBuffer.Size) - cmd.IdxOffset ||
                cmd.ElemCount % 3)
            {
                return false;
            }
            quanta_texture view = texture->view;
            view.nearest = renderer()->nearest ? 1 : 0;
            quanta_clip clip = {
                clip_coordinate((cmd.ClipRect.x - pos.x) * scale.x, surface->width, false),
                clip_coordinate((cmd.ClipRect.y - pos.y) * scale.y, surface->height, false),
                clip_coordinate((cmd.ClipRect.z - pos.x) * scale.x, surface->width, true),
                clip_coordinate((cmd.ClipRect.w - pos.y) * scale.y, surface->height, true)};
            for (unsigned k = 0; k < cmd.ElemCount; k += 3)
            {
                quanta_vertex vertices[3];
                for (unsigned v = 0; v < 3; ++v)
                {
                    const size_t index = static_cast<size_t>(cmd.VtxOffset) +
                                         list->IdxBuffer[static_cast<int>(cmd.IdxOffset + k + v)];
                    if (index >= static_cast<size_t>(list->VtxBuffer.Size))
                    {
                        return false;
                    }
                    const ImDrawVert &src = list->VtxBuffer[static_cast<int>(index)];
                    vertices[v] = {(src.pos.x - pos.x) * scale.x, (src.pos.y - pos.y) * scale.y,
                                   src.uv.x, src.uv.y, src.col};
                }
                quanta_raster_triangle(surface, &clip, vertices, &view);
            }
        }
    }
    return true;
}
