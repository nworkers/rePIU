#ifndef REPIU_PLATFORM_WIN32_RUNTIME_MEMORY_POLICY_H_
#define REPIU_PLATFORM_WIN32_RUNTIME_MEMORY_POLICY_H_

#include "repiu/runtime/runtime_memory.h"

#include <cstdint>
#include <string>

namespace repiu::platform::win32
{

struct Win32RuntimeMemoryPolicy
{
    bool valid = false;
    std::uint32_t host_pointer_bits = 0;
    bool direct_x86_execution_supported = false;
    std::uint32_t preferred_allocation_base = 0;
    std::uint32_t required_reserve_size = 0;
    std::uint32_t hle_reserve_base = 0;
    std::string message;
};

bool BuildWin32RuntimeMemoryPolicy(
    const runtime::RuntimeMemoryPlan& memory_plan,
    Win32RuntimeMemoryPolicy* policy);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_RUNTIME_MEMORY_POLICY_H_
