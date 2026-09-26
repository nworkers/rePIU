#include "build_identity_probe.h"

#include "repiu/platform/build_identity.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace repiu::tools
{

// Task 738. The label the window title carries. Checked against what this
// probe's own compiler knows, so a wrong macro test shows up as a mismatch
// rather than as a title nobody reads closely.
bool RunBuildIdentityProbe()
{
    using repiu::platform::BuildArchitectureName;
    using repiu::platform::BuildConfigurationName;
    using repiu::platform::BuildIdentityLabel;
    using repiu::platform::BuildPlatformName;

    const std::string_view platform = BuildPlatformName();
    const std::string_view architecture = BuildArchitectureName();
    const std::string_view configuration = BuildConfigurationName();
    const std::string label = BuildIdentityLabel();

#if defined(__EMSCRIPTEN__)
    const bool platform_ok = platform == "Web";
#elif defined(_WIN32)
    const bool platform_ok = platform == "Win";
#elif defined(__linux__)
    const bool platform_ok = platform == "Linux";
#else
    const bool platform_ok = platform == "Unknown";
#endif

    // The architecture name must agree with the pointer width, whichever
    // macro chose it.
    const bool architecture_ok =
        (sizeof(void*) == 8U &&
         (architecture == "x64" || architecture == "arm64" ||
          architecture == "ptr64")) ||
        (sizeof(void*) == 4U &&
         (architecture == "x86" || architecture == "wasm32" ||
          architecture == "ptr32"));

    // The configuration is never empty, and with no CMake configuration
    // it follows NDEBUG.
#if defined(REPIU_BUILD_CONFIG)
    constexpr std::string_view configured = REPIU_BUILD_CONFIG;
#else
    constexpr std::string_view configured = "";
#endif
#if defined(NDEBUG)
    constexpr std::string_view fallback = "Release";
#else
    constexpr std::string_view fallback = "Debug";
#endif
    const bool configuration_ok = !configuration.empty() &&
        configuration == (configured.empty() ? fallback : configured);

    const std::string expected = std::string(platform) + "/" +
        std::string(architecture) + " " + std::string(configuration);
    const bool label_ok = label == expected;

    const bool ok = platform_ok && architecture_ok && configuration_ok &&
        label_ok;
    std::cout << "build_identity=" << (ok ? "true" : "false")
              << ",label=\"" << label << "\""
              << ",platform=" << (platform_ok ? "true" : "false")
              << ",architecture=" << (architecture_ok ? "true" : "false")
              << ",configuration=" << (configuration_ok ? "true" : "false")
              << "\n";
    return ok;
}

}  // namespace repiu::tools
