#include "repiu/engine/glide_osd.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include <SDL3/SDL.h>

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

void GlideOsd::ProcessEvent(const void* sdl_event)
{
    if (!initialized_ || sdl_event == nullptr)
    {
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdl_event));
}

void GlideOsd::Render(std::atomic<bool>* lfb_high_precision)
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
        ImGui::TextDisabled("Tab closes this overlay");
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace repiu::engine
