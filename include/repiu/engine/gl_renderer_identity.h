#ifndef REPIU_ENGINE_GL_RENDERER_IDENTITY_H_
#define REPIU_ENGINE_GL_RENDERER_IDENTITY_H_

#include <string>
#include <string_view>

namespace repiu::engine
{

// #5. What draws the picture, as the in-game OSD shows it: the strings the GL
// context reports and the SDL video driver it was created through. Filled by
// the Glide OpenGL backend once its context is current; plain values so the
// OSD and the probe need neither GL nor SDL headers.
// See docs/design/20261005-i005-osd-gl-renderer.md.
struct GlRendererIdentity
{
    std::string renderer;      // GL_RENDERER
    std::string vendor;        // GL_VENDOR
    std::string version;       // GL_VERSION
    std::string video_driver;  // SDL_GetCurrentVideoDriver(): windows, x11, wayland
    bool software = false;     // IsSoftwareGlRenderer(renderer)
    bool wsl_d3d12 = false;    // Task 752: the engine chose Mesa's D3D12 driver
};

// Whether a GL_RENDERER string names a software rasterizer: Mesa's llvmpipe,
// softpipe and swrast, Windows' driverless "GDI Generic", WARP ("Microsoft
// Basic Render Driver", reached through D3D12 on WSL) and SwiftShader.
// Case-insensitive substring match. GPU names vary without end, so the list
// names the software side; an unknown or empty string is not software.
[[nodiscard]] bool IsSoftwareGlRenderer(std::string_view renderer);

// Fills `software` from `renderer`, and stands in "unknown" for an empty
// string so the OSD never shows a blank line.
[[nodiscard]] GlRendererIdentity MakeGlRendererIdentity(
    const char* renderer, const char* vendor, const char* version,
    const char* video_driver, bool wsl_d3d12);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GL_RENDERER_IDENTITY_H_
