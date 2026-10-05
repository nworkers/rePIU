#include "gl_renderer_identity_probe.h"

#include "repiu/engine/gl_renderer_identity.h"

#include <iostream>
#include <string_view>

namespace repiu::tools
{
namespace
{

struct RendererCase
{
    std::string_view renderer;
    bool software;
};

}  // namespace

// #5. Which GL_RENDERER strings the OSD marks as software. The software names
// are the ones seen on the hosts this project runs on (WSL's llvmpipe and
// D3D12 over WARP, Windows without a driver); the GPU names are the machines
// the work logs measured on.
bool RunGlRendererIdentityProbe()
{
    constexpr RendererCase kCases[] = {
        {"llvmpipe (LLVM 17.0.6, 256 bits)", true},
        {"LLVMPIPE (LLVM 19.1.1, 128 bits)", true},
        {"softpipe", true},
        {"Software Rasterizer", true},
        {"GDI Generic", true},
        {"D3D12 (Microsoft Basic Render Driver)", true},
        {"Google SwiftShader", true},
        {"NVIDIA GeForce RTX 4090/PCIe/SSE2", false},
        {"D3D12 (NVIDIA GeForce RTX 4090)", false},
        {"Intel(R) Arc(TM) A770 Graphics", false},
        {"AMD Radeon RX 7900 XTX (radeonsi, navi31, LLVM 17.0.6, DRM 3.54)",
         false},
        {"unknown", false},
        {"", false},
    };
    bool classify_ok = true;
    for (const RendererCase& entry : kCases)
    {
        if (engine::IsSoftwareGlRenderer(entry.renderer) != entry.software)
        {
            classify_ok = false;
            std::cout << "gl_renderer_identity_mismatch=" << entry.renderer
                      << '\n';
        }
    }

    // What the backend builds: empty or missing strings read "unknown", and
    // the software flag follows the renderer.
    const engine::GlRendererIdentity software = engine::MakeGlRendererIdentity(
        "llvmpipe (LLVM 17.0.6, 256 bits)", "Mesa", "4.5 (Compatibility Profile) Mesa 24.0.9",
        "x11", false);
    const engine::GlRendererIdentity missing = engine::MakeGlRendererIdentity(
        nullptr, "", nullptr, nullptr, true);
    const bool make_ok = software.software && software.vendor == "Mesa" &&
        software.video_driver == "x11" && !software.wsl_d3d12 &&
        !missing.software && missing.renderer == "unknown" &&
        missing.vendor == "unknown" && missing.version == "unknown" &&
        missing.video_driver == "unknown" && missing.wsl_d3d12;

    const bool all = classify_ok && make_ok;
    std::cout << "gl_renderer_identity_classify="
              << (classify_ok ? "true" : "false")
              << "\ngl_renderer_identity_make=" << (make_ok ? "true" : "false")
              << "\ngl_renderer_identity_all=" << (all ? "true" : "false")
              << '\n';
    return all;
}

}  // namespace repiu::tools
