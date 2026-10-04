#include "repiu/engine/glide_osd.h"

#include "repiu/engine/glide_post_process.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include <SDL3/SDL.h>

#include <string>

namespace repiu::engine
{
namespace
{

// The launcher targets the same revision for the same reason: the backend
// creates its context without requesting a version, and this GLSL level is
// what compatibility contexts have offered since OpenGL 3.0.
constexpr const char* kGlslVersion = "#version 130";

}  // namespace

GlideOsd::GlideOsd() = default;

GlideOsd::~GlideOsd()
{
    Shutdown();
}

bool GlideOsd::Initialize(void* sdl_window, void* gl_context,
                          std::string* message)
{
    if (initialized_)
    {
        return true;
    }
    if (sdl_window == nullptr || gl_context == nullptr)
    {
        if (message != nullptr)
        {
            *message = "OSD needs a window and a current GL context";
        }
        return false;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForOpenGL(
            static_cast<SDL_Window*>(sdl_window),
            static_cast<SDL_GLContext>(gl_context)))
    {
        ImGui::DestroyContext();
        if (message != nullptr)
        {
            *message = "ImGui SDL3 backend initialization failed";
        }
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(kGlslVersion))
    {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        if (message != nullptr)
        {
            *message = "ImGui OpenGL3 backend initialization failed";
        }
        return false;
    }
    initialized_ = true;
    return true;
}

void GlideOsd::Shutdown()
{
    if (!initialized_)
    {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
    visible_ = false;
}

bool GlideOsd::WantsMouse() const
{
    return initialized_ && visible_ && ImGui::GetIO().WantCaptureMouse;
}

void GlideOsd::ProcessEvent(const void* sdl_event)
{
    if (!initialized_ || sdl_event == nullptr)
    {
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdl_event));
}

namespace
{

// Task 768: the shader list, Reload, and the active shader's parameters.
void DrawPostProcessMenu(GlidePostProcess* post_process)
{
    ImGui::SeparatorText("Screen shader");
    const std::string current = post_process->active_id();
    std::string chosen;
    if (ImGui::BeginCombo("Shader", current.c_str()))
    {
        if (ImGui::Selectable(kPostShaderNoneId, current == kPostShaderNoneId))
        {
            chosen = kPostShaderNoneId;
        }
        for (const PostShaderEntry& entry : post_process->catalog())
        {
            const std::string label =
                entry.builtin ? entry.id + "  (built-in)" : entry.id;
            if (ImGui::Selectable(label.c_str(), current == entry.id))
            {
                chosen = entry.id;
            }
        }
        ImGui::EndCombo();
    }
    // Compiled after the combo closes so the menu never draws half a switch.
    if (!chosen.empty() && chosen != current)
    {
        post_process->Select(chosen);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        post_process->Reload();
    }
    if (!post_process->last_error().empty())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0F);
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.35F, 1.0F), "%s",
                           post_process->last_error().c_str());
        ImGui::PopTextWrapPos();
    }
    for (PostShaderParameter& parameter : post_process->parameters())
    {
        const std::string& label = parameter.description.empty()
            ? parameter.name
            : parameter.description;
        ImGui::PushID(parameter.name.c_str());
        ImGui::SliderFloat(label.c_str(), &parameter.value, parameter.minimum,
                           parameter.maximum, "%.2f");
        ImGui::PopID();
    }
    ImGui::TextDisabled("Files: %s/*.glsl. Changes last for this run;",
                        post_process->shader_directory().c_str());
    ImGui::TextDisabled("the launcher stores the default.");
}

}  // namespace

void GlideOsd::Render(std::atomic<bool>* lfb_high_precision,
                      GlidePostProcess* post_process)
{
    if (!initialized_ || !visible_)
    {
        return;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(16.0F, 16.0F), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("rePIU OSD", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoCollapse))
    {
        if (lfb_high_precision != nullptr)
        {
            bool enabled =
                lfb_high_precision->load(std::memory_order_relaxed);
            if (ImGui::Checkbox("LFB high precision (32-bit)", &enabled))
            {
                lfb_high_precision->store(enabled,
                                          std::memory_order_relaxed);
            }
        }
        if (post_process != nullptr)
        {
            DrawPostProcessMenu(post_process);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Tab closes this overlay");
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace repiu::engine
