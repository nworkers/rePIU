#include "repiu/engine/gl_renderer_identity.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace repiu::engine
{
namespace
{

// Lower case, compared against a lower-cased renderer string.
constexpr std::array<std::string_view, 7> kSoftwareRendererMarkers = {
    "llvmpipe",
    "softpipe",
    "swrast",
    "software rasterizer",
    "gdi generic",
    "microsoft basic render",
    "swiftshader",
};

std::string ToLower(std::string_view text)
{
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return lowered;
}

std::string OrUnknown(const char* value)
{
    return value != nullptr && value[0] != '\0' ? std::string(value)
                                                : std::string("unknown");
}

}  // namespace

bool IsSoftwareGlRenderer(std::string_view renderer)
{
    const std::string lowered = ToLower(renderer);
    return std::any_of(kSoftwareRendererMarkers.begin(),
                       kSoftwareRendererMarkers.end(),
                       [&lowered](std::string_view marker) {
                           return lowered.find(marker) != std::string::npos;
                       });
}

GlRendererIdentity MakeGlRendererIdentity(const char* renderer,
                                          const char* vendor,
                                          const char* version,
                                          const char* video_driver,
                                          bool wsl_d3d12)
{
    GlRendererIdentity identity;
    identity.renderer = OrUnknown(renderer);
    identity.vendor = OrUnknown(vendor);
    identity.version = OrUnknown(version);
    identity.video_driver = OrUnknown(video_driver);
    identity.software =
        renderer != nullptr && IsSoftwareGlRenderer(renderer);
    identity.wsl_d3d12 = wsl_d3d12;
    return identity;
}

}  // namespace repiu::engine
