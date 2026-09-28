#ifndef REPIU_PLATFORM_WIN32_GUEST_CPU_CONTEXT_WIN32_H_
#define REPIU_PLATFORM_WIN32_GUEST_CPU_CONTEXT_WIN32_H_

// Task 758. The Win32 half of guest_cpu_context.h. Include
// guest_cpu_context.h, not this.

#include <cstdint>

#include <windows.h>

namespace repiu::platform
{

using GuestCpuContext = CONTEXT;

inline constexpr bool HardwareDebugRegistersAvailable()
{
    return true;
}

// The host's own macros rather than their values, so this cannot drift from
// what the API actually expects.
inline constexpr std::uint32_t kGuestCpuContextIntegerControlSegments =
    static_cast<std::uint32_t>(
        CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_WIN32_GUEST_CPU_CONTEXT_WIN32_H_
