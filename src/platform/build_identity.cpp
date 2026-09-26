#include "repiu/platform/build_identity.h"

namespace repiu::platform
{

std::string_view BuildPlatformName()
{
#if defined(__EMSCRIPTEN__)
    return "Web";
#elif defined(_WIN32)
    return "Win";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string_view BuildArchitectureName()
{
#if defined(__wasm32__)
    return "wasm32";
#elif defined(_M_X64) || defined(__x86_64__)
    return "x64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif SIZE_MAX > 0xFFFFFFFFu
    return "ptr64";
#else
    return "ptr32";
#endif
}

std::string_view BuildConfigurationName()
{
#if defined(REPIU_BUILD_CONFIG)
    // A single-config generator with no CMAKE_BUILD_TYPE expands `$<CONFIG>`
    // to nothing; the NDEBUG answer below covers that case.
    constexpr std::string_view configured = REPIU_BUILD_CONFIG;
    if (!configured.empty())
    {
        return configured;
    }
#endif
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

std::string BuildIdentityLabel()
{
    std::string label(BuildPlatformName());
    label += '/';
    label += BuildArchitectureName();
    label += ' ';
    label += BuildConfigurationName();
    return label;
}

}  // namespace repiu::platform
