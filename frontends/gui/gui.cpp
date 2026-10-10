// SPDX-License-Identifier: MIT
// The only C++ application layer: Dear ImGui view + software/native window
// adapter.
#include "imgui.h"
#include "quanta/osal.h"
#include "quanta/view.h"
#include "raster.h"
#include "window.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <inttypes.h>
#include <new>
#include <vector>
#if IMGUI_VERSION_NUM < 19100 || IMGUI_VERSION_NUM >= 19200
#error "This adapter targets the pinned ImGui 1.91 API. Use v1.91.9b."
#endif
namespace
{
struct App
{
    quanta_window *window = nullptr;
    ImGuiContext *imgui = nullptr;
    quanta_live *live = nullptr;
    quanta_model model{}, frozen{};
    std::vector<uint32_t> pixels;
    bool paused = false;
    int width = 1200, height = 800;

    ~App()
    {
        if (live)
        {
            quanta_live_finish(live, 1);
        }
        quanta_model_destroy(&frozen);
        quanta_model_destroy(&model);
        if (imgui)
        {
            ImGui::DestroyContext(imgui);
        }
        if (window)
        {
            quanta_window_close(window);
        }
    }
};

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
            if (e.codepoint)
            {
                io.AddInputCharacter(e.codepoint);
            }
            break;
        }
        default:
            break;
    }
}

bool contains(const char *s, const char *find)
{
    if (!*find)
    {
        return true;
    }
    for (; *s; s++)
    {
        const unsigned char *a = (const unsigned char *)s, *b = (const unsigned char *)find;
        while (*a && *b && std::tolower(*a) == std::tolower(*b))
        {
            a++;
            b++;
        }
        if (!*b)
        {
            return true;
        }
    }
    return false;
}

ImVec4 status_color(quanta_status s)
{
    if (s == QUANTA_OK)
    {
        return ImVec4(0.40f, 0.84f, 0.66f, 1);
    }
    if (s == QUANTA_WARMUP || s == QUANTA_NO_DATA)
    {
        return ImVec4(0.95f, 0.76f, 0.38f, 1);
    }
    return ImVec4(0.97f, 0.43f, 0.45f, 1);
}

void details(const quanta_item &p)
{
    ImGui::TextUnformatted(p.meta.key);
    ImGui::Separator();
    ImGui::TextWrapped("%s", p.meta.metric->description);
    ImGui::Text("Source: %s", quanta_metric_source(p.meta.metric));
    ImGui::Text("Type: %s   Unit: %s", quanta_type_name(p.meta.type), p.meta.unit);
    ImGui::Text("Poll period: %.3fs   Minimum: %.3fs", (double)p.meta.effective_period_ns / 1e9,
                (double)p.meta.metric->min_poll_ns / 1e9);
    ImGui::Text("Series: %" PRIu64 "   Source series: %" PRIu64 "   Generation: %u", p.meta.id,
                p.meta.source_id, p.meta.generation);
    ImGui::Text("Window: %.3fs", p.has_last ? (double)p.last.window_ns / 1e9 : 0.0);
    ImGui::TextWrapped("Documentation: %s", p.meta.metric->doc_path);
    if (p.meta.metric->flags & QUANTA_SENSITIVE)
    {
        ImGui::TextUnformatted("Sensitive value: use --redact before sharing a report.");
    }
    for (size_t i = 0; i < p.meta.label_count; i++)
    {
        ImGui::TextWrapped("%s: %s", p.meta.labels[i].key, p.meta.labels[i].value);
    }
    if (p.has_last && p.last.message[0])
    {
        ImGui::TextWrapped("Last result: %s", p.last.message);
    }
}

void plot(const quanta_item &p, uint64_t end, uint64_t history)
{
    ImVec2 at = ImGui::GetCursorScreenPos(),
           size(std::max(40.0f, ImGui::GetContentRegionAvail().x), 100.0f);
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(13, 19, 28, 255), 5);
    double lo = 0, hi = 1;
    bool found = false;
    uint64_t begin = end > history ? end - history : 0;
    if (p.history_count)
    {
        uint64_t oldest = quanta_history_at(&p, 0)->time_ns;
        if (oldest > begin)
        {
            begin = oldest;
        }
    }
    for (size_t i = 0; i < p.history_count; i++)
    {
        const quanta_point *pt = quanta_history_at(&p, i);
        if (pt->status != QUANTA_OK || pt->time_ns < begin)
        {
            continue;
        }
        if (!found)
        {
            lo = hi = pt->value;
            found = true;
        }
        else
        {
            lo = std::min(lo, pt->value);
            hi = std::max(hi, pt->value);
        }
    }
    if (p.meta.view.has_min)
    {
        lo = p.meta.view.min;
    }
    if (p.meta.view.has_max)
    {
        hi = p.meta.view.max;
    }
    if (hi <= lo)
    {
        double pad = std::max(1.0, std::fabs(lo) * 0.02);
        lo -= pad * 0.5;
        hi += pad * 0.5;
    }
    if (end <= begin)
    {
        end = begin + 1;
    }
    bool previous = false;
    ImVec2 prev;
    uint64_t prev_time = 0;
    for (size_t i = 0; i < p.history_count; i++)
    {
        const quanta_point *pt = quanta_history_at(&p, i);
        if (pt->time_ns < begin || pt->time_ns > end)
        {
            continue;
        }
        if (pt->status != QUANTA_OK)
        {
            previous = false;
            continue;
        }
        float x = at.x + 5 +
                  (float)((double)(pt->time_ns - begin) / (double)(end - begin)) * (size.x - 10);
        float y = at.y + size.y - 6 -
                  (float)std::max(0.0, std::min(1.0, (pt->value - lo) / (hi - lo))) * (size.y - 12);
        ImVec2 pos(x, y);
        if (previous && pt->time_ns - prev_time <= 3 * p.meta.effective_period_ns)
        {
            draw->AddLine(prev, pos, IM_COL32(92, 190, 244, 255), 1.75f);
        }
        else
        {
            draw->AddCircleFilled(pos, 2, IM_COL32(92, 190, 244, 255));
        }
        prev = pos;
        prev_time = pt->time_ns;
        previous = true;
    }
    ImGui::Dummy(size);
    double scale = 1;
    const char *unit = p.meta.view.unit[0] ? p.meta.view.unit : p.meta.unit;
    quanta_unit_scale(p.meta.unit, unit, &scale);
    ImGui::TextDisabled("%.3g .. %.3g %s | %.1fs retained | %zu points", lo * scale, hi * scale,
                        unit, (double)(end - begin) / 1e9, p.history_count);
}

void gauge(const quanta_item &p)
{
    double v = 0, lo = p.meta.view.has_min ? p.meta.view.min : 0,
           hi = p.meta.view.has_max ? p.meta.view.max : 1;
    if (!p.has_last || p.last.status != QUANTA_OK || !quanta_value_number(&p.last.value, &v) ||
        hi <= lo)
    {
        ImGui::TextDisabled("No current measurement");
        ImGui::Dummy(ImVec2(0, 65));
        return;
    }
    float frac = (float)std::max(0.0, std::min(1.0, (v - lo) / (hi - lo)));
    ImVec2 at = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 center(at.x + width * 0.5f, at.y + 53);
    ImDrawList *d = ImGui::GetWindowDrawList();
    const float start = 2.35619449f, finish = 7.06858347f;
    d->PathArcTo(center, 47, start, finish, 56);
    d->PathStroke(IM_COL32(48, 63, 81, 255), 0, 10);
    if (frac > 0)
    {
        d->PathArcTo(center, 47, start, start + (finish - start) * frac, 56);
        d->PathStroke(IM_COL32(94, 205, 171, 255), 0, 10);
    }
    char label[32];
    std::snprintf(label, sizeof(label), "%.0f%%", frac * 100);
    ImVec2 sz = ImGui::CalcTextSize(label);
    d->AddText(ImVec2(center.x - sz.x * .5f, center.y - sz.y * .5f), IM_COL32(231, 238, 247, 255),
               label);
    ImGui::Dummy(ImVec2(width, 104));
}

void card(const quanta_item &p, bool redact, uint64_t now, uint64_t history)
{
    char value[1024], status[128], name[2048];
    quanta_item_value(value, sizeof(value), &p, redact);
    quanta_item_status(status, sizeof(status), &p, now);
    quanta_series_name(name, sizeof(name), &p.meta);
    float height = p.meta.view.widget == QUANTA_WIDGET_PLOT    ? 248.0f
                   : p.meta.view.widget == QUANTA_WIDGET_GAUGE ? 258.0f
                                                               : 164.0f;
    ImGui::PushID((void *)(uintptr_t)p.meta.id);
    ImGui::BeginChild("card", ImVec2(0, height), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(p.meta.view.title[0] ? p.meta.view.title : p.meta.metric->title);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        details(p);
        ImGui::EndTooltip();
    }
    ImGui::TextDisabled("%s", p.meta.key);
    if (p.meta.label_count)
    {
        ImGui::TextDisabled("%s", p.meta.labels[0].value);
    }
    ImGui::TextColored(p.has_last ? status_color(p.last.status) : status_color(QUANTA_NO_DATA),
                       "%s", value);
    if (p.meta.view.widget == QUANTA_WIDGET_PLOT)
    {
        plot(p, now, history);
    }
    else if (p.meta.view.widget == QUANTA_WIDGET_GAUGE)
    {
        gauge(p);
    }
    else if (p.meta.view.widget == QUANTA_WIDGET_BAR)
    {
        double v, lo = p.meta.view.has_min ? p.meta.view.min : 0,
                  hi = p.meta.view.has_max ? p.meta.view.max : 1;
        if (p.has_last && p.last.status == QUANTA_OK && quanta_value_number(&p.last.value, &v) &&
            hi > lo)
        {
            ImGui::ProgressBar((float)std::max(0.0, std::min(1.0, (v - lo) / (hi - lo))),
                               ImVec2(-1, 16), "");
        }
    }
    ImGui::TextDisabled("%s", status);
    ImGui::SameLine();
    if (ImGui::SmallButton("Details"))
    {
        ImGui::OpenPopup("Details");
    }
    if (ImGui::BeginPopup("Details"))
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35);
        details(p);
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void table(quanta_model &model, std::vector<size_t> &visible, bool redact)
{
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable;
    if (!ImGui::BeginTable("metrics", 5, flags, ImVec2(0, 0)))
    {
        return;
    }
    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_DefaultSort, 0, 0);
    ImGui::TableSetupColumn("Value", 0, 0, 1);
    ImGui::TableSetupColumn("Status", 0, 0, 2);
    ImGui::TableSetupColumn("Age (s)", 0, 0, 3);
    ImGui::TableSetupColumn("Source", 0, 0, 4);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs();
    if (specs && specs->SpecsCount)
    {
        auto sort = specs->Specs[0];
        std::stable_sort(visible.begin(), visible.end(),
                         [&](size_t a, size_t b)
                         {
                             const quanta_item &x = model.items[a], &y = model.items[b];
                             int cmp = 0;
                             if (sort.ColumnUserID == 1)
                             {
                                 double vx = 0, vy = 0;
                                 bool ox = x.has_last && x.last.status == QUANTA_OK &&
                                           quanta_value_number(&x.last.value, &vx),
                                      oy = y.has_last && y.last.status == QUANTA_OK &&
                                           quanta_value_number(&y.last.value, &vy);
                                 if (ox != oy)
                                 {
                                     cmp = ox ? -1 : 1;
                                 }
                                 else if (ox)
                                 {
                                     cmp = vx < vy ? -1 : vx > vy ? 1 : 0;
                                 }
                                 else
                                 {
                                     char sx[1024], sy[1024];
                                     quanta_item_value(sx, sizeof(sx), &x, redact);
                                     quanta_item_value(sy, sizeof(sy), &y, redact);
                                     cmp = std::strcmp(sx, sy);
                                 }
                             }
                             else if (sort.ColumnUserID == 2)
                             {
                                 cmp = (int)x.last.status - (int)y.last.status;
                             }
                             else if (sort.ColumnUserID == 3)
                             {
                                 cmp = x.last.measured_at_ns > y.last.measured_at_ns   ? -1
                                       : x.last.measured_at_ns < y.last.measured_at_ns ? 1
                                                                                       : 0;
                             }
                             else if (sort.ColumnUserID == 4)
                             {
                                 cmp = std::strcmp(quanta_metric_source(x.meta.metric),
                                                   quanta_metric_source(y.meta.metric));
                             }
                             else
                             {
                                 char sx[2048], sy[2048];
                                 quanta_series_name(sx, sizeof(sx), &x.meta);
                                 quanta_series_name(sy, sizeof(sy), &y.meta);
                                 cmp = std::strcmp(sx, sy);
                             }
                             if (!cmp)
                             {
                                 cmp = x.meta.id < y.meta.id ? -1 : x.meta.id > y.meta.id ? 1 : 0;
                             }
                             return sort.SortDirection == ImGuiSortDirection_Ascending ? cmp < 0
                                                                                       : cmp > 0;
                         });
        specs->SpecsDirty = false;
    }
    ImGuiListClipper clip;
    clip.Begin((int)visible.size());
    while (clip.Step())
    {
        for (int j = clip.DisplayStart; j < clip.DisplayEnd; j++)
        {
            quanta_item &p = model.items[visible[(size_t)j]];
            char name[2048], value[1024], status[128];
            quanta_series_name(name, sizeof(name), &p.meta);
            quanta_item_value(value, sizeof(value), &p, redact);
            quanta_item_status(status, sizeof(status), &p, model.frame.monotonic_ns);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                details(p);
                ImGui::EndTooltip();
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(status_color(p.last.status), "%s", status);
            ImGui::TableSetColumnIndex(3);
            uint64_t measured = p.has_last && p.last.status != QUANTA_OK && p.has_good
                                    ? p.good.measured_at_ns
                                    : p.last.measured_at_ns;
            double age = p.has_last && quanta_now_ns() > measured
                             ? (double)(quanta_now_ns() - measured) / 1e9
                             : 0;
            ImGui::Text("%.2f", age);
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(quanta_metric_source(p.meta.metric));
        }
    }
    ImGui::EndTable();
}

void render_draw_data(ImDrawData *data, quanta_surface *surface, const quanta_texture *texture)
{
    quanta_raster_clear(surface, 0x101721);
    ImVec2 pos = data->DisplayPos, scale = data->FramebufferScale;
    for (int n = 0; n < data->CmdListsCount; n++)
    {
        const ImDrawList *list = data->CmdLists[n];
        for (int j = 0; j < list->CmdBuffer.Size; j++)
        {
            const ImDrawCmd &cmd = list->CmdBuffer[j];
            if (cmd.UserCallback)
            {
                if (cmd.UserCallback != ImDrawCallback_ResetRenderState)
                {
                    cmd.UserCallback(list, &cmd);
                }
                continue;
            }
            if (cmd.GetTexID() != (ImTextureID)1)
            {
                continue;
            }
            quanta_clip clip{(int)std::floor((cmd.ClipRect.x - pos.x) * scale.x),
                             (int)std::floor((cmd.ClipRect.y - pos.y) * scale.y),
                             (int)std::ceil((cmd.ClipRect.z - pos.x) * scale.x),
                             (int)std::ceil((cmd.ClipRect.w - pos.y) * scale.y)};
            for (unsigned k = 0; k + 2 < cmd.ElemCount; k += 3)
            {
                quanta_vertex vertices[3];
                for (unsigned v = 0; v < 3; v++)
                {
                    const ImDrawVert &src = list->VtxBuffer[(
                        int)(cmd.VtxOffset + list->IdxBuffer[(int)(cmd.IdxOffset + k + v)])];
                    vertices[v] = {(src.pos.x - pos.x) * scale.x, (src.pos.y - pos.y) * scale.y,
                                   src.uv.x, src.uv.y, src.col};
                }
                quanta_raster_triangle(surface, &clip, vertices, texture);
            }
        }
    }
}

int run(quanta_engine *engine, const quanta_plan *plan)
{
    App app;
    char error[512];
    if (!quanta_window_open("Quanta | System Observatory", app.width, app.height, &app.window,
                            error, sizeof(error)))
    {
        std::fprintf(stderr, "quanta: %s\n", error);
        return 1;
    }
    IMGUI_CHECKVERSION();
    app.imgui = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendPlatformName = "quanta_native_window";
    io.BackendRendererName = "quanta_software";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const float dpi = quanta_window_scale(app.window);
    ImGui::GetMainViewport()->PlatformHandleRaw = quanta_window_native(app.window);
    if (plan->font_path[0])
    {
        FILE *font = quanta_fopen_utf8(plan->font_path, "rb");
        if (!font)
        {
            std::fprintf(stderr, "quanta: cannot read GUI font: %s\n", plan->font_path);
            return 1;
        }
        if (std::fseek(font, 0, SEEK_END) != 0)
        {
            std::fclose(font);
            return 1;
        }
        long bytes = std::ftell(font);
        if (bytes <= 0 || bytes > 32 * 1024 * 1024 || std::fseek(font, 0, SEEK_SET) != 0)
        {
            std::fclose(font);
            return 1;
        }
        void *data = ImGui::MemAlloc((size_t)bytes);
        if (!data)
        {
            std::fclose(font);
            return 1;
        }
        size_t got = std::fread(data, 1, (size_t)bytes, font);
        int io_error = std::ferror(font);
        std::fclose(font);
        if (got != (size_t)bytes || io_error)
        {
            ImGui::MemFree(data);
            return 1;
        }
        /* Atlas owns this memory, including on a failed font build. */
        if (!io.Fonts->AddFontFromMemoryTTF(data, (int)bytes, 17.0f * dpi, nullptr,
                                            io.Fonts->GetGlyphRangesCyrillic()))
        {
            std::fprintf(stderr, "quanta: cannot decode GUI font\n");
            return 1;
        }
    }
    else
    {
        ImFontConfig font_config;
        font_config.SizePixels = 13.0f * dpi;
        io.Fonts->AddFontDefault(&font_config);
    }
    io.FontGlobalScale = 1.0f / dpi;
    unsigned char *alpha = nullptr;
    int tw = 0, th = 0;
    io.Fonts->GetTexDataAsAlpha8(&alpha, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);
    quanta_texture texture{alpha, tw, th};
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.ChildRounding = 8;
    style.FrameRounding = 5;
    style.WindowPadding = ImVec2(16, 14);
    style.ItemSpacing = ImVec2(10, 8);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.065f, .085f, .12f, 1);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(.09f, .12f, .17f, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(.16f, .25f, .36f, 1);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(.30f, .76f, .61f, 1);
    quanta_model_init(&app.model, plan);
    quanta_sink sink = quanta_model_sink(&app.model);
    quanta_status result = quanta_live_start(engine, plan, &app.live);
    if (result != QUANTA_OK)
    {
        std::fprintf(stderr, "quanta: cannot start collector: %s\n", quanta_status_name(result));
        return 1;
    }
    bool running = true, table_view = false;
    for (size_t i = 0; i < plan->select_count; i++)
    {
        if (plan->selects[i].view.widget == QUANTA_WIDGET_TABLE)
        {
            table_view = true;
        }
    }
    char filter[256] = {0};
    uint64_t last = quanta_now_ns();
    int final_frames = 0;
    while (running && !quanta_interrupted())
    {
        uint64_t frame_start = quanta_now_ns();
        quanta_window_event ev;
        int available;
        while ((available = quanta_window_poll(app.window, &ev)) > 0)
        {
            if (ev.kind == QUANTA_X_CLOSE)
            {
                running = false;
            }
            else if (ev.kind == QUANTA_X_RESIZE)
            {
                app.width = ev.width;
                app.height = ev.height;
            }
            else
            {
                if (ev.kind == QUANTA_X_MOTION || ev.kind == QUANTA_X_BUTTON)
                {
                    ev.x = (int)(ev.x / dpi);
                    ev.y = (int)(ev.y / dpi);
                }
                input(io, ev);
            }
        }
        if (available < 0)
        {
            std::fprintf(stderr, "quanta: window event transport failed\n");
            return 1;
        }
        if (!running)
        {
            break;
        }
        if (app.width <= 0 || app.height <= 0 || app.width > 4096 || app.height > 4096)
        {
            std::fprintf(stderr, "quanta: GUI surface limit is 4096x4096\n");
            return 1;
        }
        result = quanta_live_pump(app.live, &sink);
        if (result != QUANTA_OK)
        {
            std::fprintf(stderr, "quanta: GUI model: %s\n", quanta_status_name(result));
            return 1;
        }
        bool done = quanta_live_done(app.live);
        if (done && ++final_frames > 2)
        {
            break;
        }
        io.DisplaySize = ImVec2((float)app.width / dpi, (float)app.height / dpi);
        io.DisplayFramebufferScale = ImVec2(dpi, dpi);
        io.DeltaTime = std::max(0.001f, (float)((double)(frame_start - last) / 1e9));
        last = frame_start;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("INV", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("Quanta / SYSTEM OBSERVATORY");
        ImGui::SameLine();
        ImGui::TextDisabled("v%s  |  C99 collectors  |  software renderer", QUANTA_VERSION);
        ImGui::Separator();
        ImGui::SetNextItemWidth(std::max(180.0f, io.DisplaySize.x * .30f));
        ImGui::InputTextWithHint("##filter", "Filter metric, label or title", filter,
                                 sizeof(filter));
        ImGui::SameLine();
        if (ImGui::Button(app.paused ? "Resume display" : "Pause display"))
        {
            if (!app.paused)
            {
                result = quanta_model_clone(&app.frozen, &app.model);
                if (result != QUANTA_OK)
                {
                    return 1;
                }
                app.paused = true;
            }
            else
            {
                quanta_model_destroy(&app.frozen);
                app.paused = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh properties"))
        {
            quanta_engine_refresh(engine);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Table", &table_view);
        quanta_model &model = app.paused ? app.frozen : app.model;
        std::vector<size_t> visible;
        for (size_t i = 0; i < model.count; i++)
        {
            if (model.items[i].active)
            {
                char name[2048];
                quanta_series_name(name, sizeof(name), &model.items[i].meta);
                if (contains(name, filter) || contains(model.items[i].meta.metric->title, filter) ||
                    contains(model.items[i].meta.view.title, filter))
                {
                    visible.push_back(i);
                }
            }
        }
        ImGui::TextColored(app.paused ? ImVec4(.97f, .78f, .42f, 1) : ImVec4(.4f, .84f, .66f, 1),
                           "%s", app.paused ? "DISPLAY PAUSED / COLLECTION ACTIVE" : "LIVE");
        ImGui::SameLine();
        ImGui::TextDisabled("%zu series  |  %" PRIu64 " samples  |  %.2f MiB history",
                            visible.size(), app.model.samples,
                            (double)model.history_bytes / (1024 * 1024));
        if (app.model.diagnostic[0])
        {
            ImGui::TextWrapped("%s", app.model.diagnostic);
        }
        ImGui::Separator();
        if (table_view)
        {
            table(model, visible, plan->redact != 0);
        }
        else
        {
            int columns = std::min((int)plan->columns,
                                   std::max(1, (int)(ImGui::GetContentRegionAvail().x / 280.0f)));
            if (ImGui::BeginTable("cards", columns, ImGuiTableFlags_SizingStretchSame))
            {
                for (size_t i : visible)
                {
                    ImGui::TableNextColumn();
                    card(model.items[i], plan->redact != 0, model.frame.monotonic_ns,
                         model.history_ns);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
        ImGui::Render();
        size_t count = (size_t)app.width * (size_t)app.height;
        if (app.pixels.size() != count)
        {
            app.pixels.resize(count);
        }
        quanta_surface surface{app.pixels.data(), app.width, app.height, app.width};
        render_draw_data(ImGui::GetDrawData(), &surface, &texture);
        if (!quanta_window_present(app.window, app.pixels.data(), app.width, app.height))
        {
            std::fprintf(stderr, "quanta: window presentation failed\n");
            return 1;
        }
        uint64_t elapsed = quanta_now_ns() - frame_start;
        if (elapsed < 33333333u)
        {
            quanta_sleep_ns(33333333u - elapsed);
        }
    }
    result = quanta_live_finish(app.live, 1);
    app.live = nullptr;
    return result == QUANTA_OK || result == QUANTA_CANCELLED ? 0 : 1;
}
} // namespace

// namespace
extern "C" int quanta_run_gui(quanta_engine *engine, const quanta_plan *plan)
{
    try
    {
        return run(engine, plan);
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "quanta: GUI exception: %s\n", e.what());
        return 1;
    }
    catch (...)
    {
        std::fprintf(stderr, "quanta: GUI failure\n");
        return 1;
    }
}
