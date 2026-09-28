#include "repiu/platform/build_identity.h"

// Task 758. The configuration part of the build identity, shared by every host.
// BuildPlatformName and BuildArchitectureName are defined per platform and
// architecture (win32/, linux/, linux/x86/, linux/x64/, web/).

namespace repiu::platform
{

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
