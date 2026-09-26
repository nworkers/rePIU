#ifndef REPIU_PLATFORM_BUILD_IDENTITY_H_
#define REPIU_PLATFORM_BUILD_IDENTITY_H_

#include <string>
#include <string_view>

// Task 738. What this binary is: host platform, architecture and build
// configuration, as a label for the window title and the logs.
//
// A run's log often has to be matched with the binary that wrote it, and the
// version alone does not say whether it was the Win32 x86 Debug build, the WSL
// x64 build or a Release. The three names are decided at compile time -- the
// platform and architecture from the compiler's predefined macros, the
// configuration from `REPIU_BUILD_CONFIG`, which CMake sets to `$<CONFIG>` so a
// multi-config generator gets the right one -- and read back here so no caller
// spells the macros again.

namespace repiu::platform
{

// "Win", "Linux", "Web" or "Unknown".
std::string_view BuildPlatformName();

// "x86", "x64", "arm64", "wasm32", or "ptr32"/"ptr64" when the compiler says
// nothing more specific.
std::string_view BuildArchitectureName();

// The CMake configuration ("Debug", "Release", ...). When the build defined
// none, "Debug" or "Release" from `NDEBUG`.
std::string_view BuildConfigurationName();

// "<platform>/<architecture> <configuration>", e.g. "Win/x86 Debug".
std::string BuildIdentityLabel();

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_BUILD_IDENTITY_H_
