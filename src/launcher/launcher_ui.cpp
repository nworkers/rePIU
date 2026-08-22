#include "repiu/launcher/launcher_ui.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace repiu::launcher
{
namespace
{

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 640;
// The Glide backend creates its context without requesting a version, so the
// launcher does the same and targets the GLSL revision that compatibility
// contexts have offered since OpenGL 3.0.
constexpr const char* kGlslVersion = "#version 130";

struct SdlContext
{
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    bool video_initialized = false;
};

void DestroySdlContext(SdlContext* context)
{
    if (context->gl_context != nullptr)
    {
        SDL_GL_DestroyContext(context->gl_context);
        context->gl_context = nullptr;
    }
    if (context->window != nullptr)
    {
        SDL_DestroyWindow(context->window);
        context->window = nullptr;
    }
    if (context->video_initialized)
    {
        // Refcounted: the Glide backend initializes the subsystem again when
        // the guest opens its own window.
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        context->video_initialized = false;
    }
}

bool CreateSdlContext(SdlContext* context, std::string* message)
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        *message = std::string("SDL video init failed: ") + SDL_GetError();
        return false;
    }
    context->video_initialized = true;
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    context->window = SDL_CreateWindow("rePIU", kWindowWidth, kWindowHeight,
                                       SDL_WINDOW_OPENGL);
    if (context->window == nullptr)
    {
        *message = std::string("SDL window failed: ") + SDL_GetError();
        DestroySdlContext(context);
        return false;
    }
    context->gl_context = SDL_GL_CreateContext(context->window);
    if (context->gl_context == nullptr)
    {
        *message = std::string("GL context failed: ") + SDL_GetError();
        DestroySdlContext(context);
        return false;
    }
    SDL_GL_MakeCurrent(context->window, context->gl_context);
    // The launcher is idle most of the time, so it waits for the display
    // rather than spinning a core while an operator reads the list.
    SDL_GL_SetSwapInterval(1);
    return true;
}

std::size_t FindInitialSelection(const std::vector<RomSetEntry>& catalog,
                                 const std::string& last_rom_set)
{
    for (std::size_t index = 0; index < catalog.size(); ++index)
    {
        if (catalog[index].id == last_rom_set &&
            IsRomSetRunnable(catalog[index]))
        {
            return index;
        }
    }
    for (std::size_t index = 0; index < catalog.size(); ++index)
    {
        if (IsRomSetRunnable(catalog[index]))
        {
            return index;
        }
    }
    return catalog.size();
}

bool SettingsDiffer(const LauncherSettings& left, const LauncherSettings& right)
{
    if (left.has_swap_interval != right.has_swap_interval ||
        left.has_ymz_volume != right.has_ymz_volume ||
        left.last_rom_set != right.last_rom_set)
    {
        return true;
    }
    if (left.has_swap_interval && left.swap_interval != right.swap_interval)
    {
        return true;
    }
    if (left.has_ymz_volume &&
        std::fabs(left.ymz_volume - right.ymz_volume) > 0.001F)
    {
        return true;
    }
    return false;
}

void DrawRomSetTable(const std::vector<RomSetEntry>& catalog,
                     std::size_t* selection, bool* start_requested,
                     bool* focus_pending)
{
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;
    const ImVec2 size(0.0F, ImGui::GetContentRegionAvail().y - 132.0F);
    if (!ImGui::BeginTable("rom_sets", 3, kFlags, size))
    {
        return;
    }
    ImGui::TableSetupColumn("ROM set", ImGuiTableColumnFlags_WidthFixed,
                            110.0F);
    ImGui::TableSetupColumn("Title");
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                            260.0F);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < catalog.size(); ++index)
    {
        const RomSetEntry& entry = catalog[index];
        const bool runnable = IsRomSetRunnable(entry);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushID(static_cast<int>(index));
        if (!runnable)
        {
            // Unavailable rows stay visible with their reason: hiding them
            // would leave an operator guessing why a disc they own is absent.
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(
                                      ImGuiCol_TextDisabled));
        }
        const bool selected = *selection == index;
        ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns |
            ImGuiSelectableFlags_AllowDoubleClick;
        if (!runnable)
        {
            // Marking the row disabled also takes it out of keyboard and
            // gamepad navigation, so the arrows step between the ROM sets that
            // can actually start instead of stopping on ones that cannot.
            flags |= ImGuiSelectableFlags_Disabled;
        }
        // A cabinet has no mouse, so the first frame plants keyboard focus on
        // the current selection. Without it navigation has nowhere to start
        // and the arrows appear dead.
        if (*focus_pending && selected)
        {
            ImGui::SetKeyboardFocusHere();
        }
        if (ImGui::Selectable(entry.id.c_str(), selected, flags))
        {
            *selection = index;
            // Enter plays the focused row the way double-clicking does; space
            // only moves the selection, which is what a list is expected to do.
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ||
                ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                *start_requested = true;
            }
        }
        // Navigation moves focus without activating anything, so the selection
        // follows focus. Otherwise the highlight would stay where the mouse
        // left it and Start would run a different ROM set than the one the
        // arrows are pointing at.
        if (ImGui::IsItemFocused())
        {
            *selection = index;
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.display_name.c_str());
        ImGui::TableSetColumnIndex(2);
        if (runnable)
        {
            const std::string status = entry.parent_id.empty()
                ? std::string("ready")
                : "ready (parent " + entry.parent_id + ")";
            ImGui::TextUnformatted(status.c_str());
        }
        else
        {
            ImGui::TextUnformatted(entry.reason.c_str());
        }
        if (!runnable)
        {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawOptions(LauncherSettings* settings)
{
    ImGui::SeparatorText("Options");
    bool vsync = settings->has_swap_interval && settings->swap_interval != 0;
    if (ImGui::Checkbox("Wait for vertical sync", &vsync))
    {
        settings->has_swap_interval = true;
        settings->swap_interval = vsync ? 1 : 0;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted(
            "Off uncaps the frame rate. Performance measurements are taken "
            "with it off.");
        ImGui::EndTooltip();
    }

    float volume = settings->has_ymz_volume ? settings->ymz_volume : 1.0F;
    if (ImGui::SliderFloat("Sound gain", &volume, 0.0F, 2.0F, "%.2f"))
    {
        settings->has_ymz_volume = true;
        settings->ymz_volume = volume;
    }
    ImGui::TextDisabled(
        "Stored in cfg/repiu.ini. An environment variable of the same meaning "
        "always wins.");
}

}  // namespace

LauncherUiResult RunLauncherUi(const std::vector<RomSetEntry>& catalog,
                               const LauncherSettings& initial_settings)
{
    LauncherUiResult result;
    result.settings = initial_settings;

    SdlContext context;
    std::string message;
    if (!CreateSdlContext(&context, &message))
    {
        result.unavailable = true;
        result.message = message;
        return result;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // A cabinet may have no mouse, so keyboard and gamepad navigation are on
    // from the start.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // The launcher owns no writable directory of its own, and window layout is
    // fixed, so ImGui's own ini file is disabled.
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(context.window, context.gl_context);
    ImGui_ImplOpenGL3_Init(kGlslVersion);

    std::size_t selection =
        FindInitialSelection(catalog, initial_settings.last_rom_set);
    bool focus_pending = true;
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID ==
                     SDL_GetWindowID(context.window)))
            {
                running = false;
            }
        }
        if (!running)
        {
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
        bool start_requested = false;
        if (ImGui::Begin("rePIU", nullptr, kWindowFlags))
        {
            ImGui::TextUnformatted("Select a ROM set");
            ImGui::SameLine();
            ImGui::TextDisabled(
                "- arrows move, Enter starts, Esc quits; discs are read from "
                "roms/");
            DrawRomSetTable(catalog, &selection, &start_requested,
                            &focus_pending);
            focus_pending = false;
            DrawOptions(&result.settings);
            ImGui::Separator();

            const bool has_selection = selection < catalog.size() &&
                IsRomSetRunnable(catalog[selection]);
            ImGui::BeginDisabled(!has_selection);
            if (ImGui::Button("Start", ImVec2(120.0F, 0.0F)))
            {
                start_requested = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Quit", ImVec2(120.0F, 0.0F)) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                running = false;
            }
            if (!has_selection)
            {
                ImGui::SameLine();
                ImGui::TextDisabled(
                    "No runnable ROM set is selected. Place <id>.zip and "
                    "roms/<id>/<disc>.chd to enable one.");
            }
            if (start_requested && has_selection)
            {
                result.launch = true;
                result.rom_set_id = catalog[selection].id;
                result.settings.last_rom_set = result.rom_set_id;
                running = false;
            }
        }
        ImGui::End();

        ImGui::Render();
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(context.window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.09F, 0.09F, 0.11F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(context.window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    DestroySdlContext(&context);

    result.settings_changed = SettingsDiffer(result.settings, initial_settings);
    return result;
}

}  // namespace repiu::launcher
