#include "repiu/platform/win32/runtime_memory_policy.h"

#include <algorithm>

namespace repiu::platform::win32
{
namespace
{

bool IsDirectX86ExecutionSupported()
{
#if defined(_M_IX86) || defined(__i386__)
    return true;
#else
    return false;
#endif
}

}  // namespace

bool BuildWin32RuntimeMemoryPolicy(
    const runtime::RuntimeMemoryPlan& memory_plan,
    Win32RuntimeMemoryPolicy* policy)
{
    if (policy == nullptr)
    {
        return false;
    }

    *policy = Win32RuntimeMemoryPolicy{};
    policy->host_pointer_bits = static_cast<std::uint32_t>(sizeof(void*) * 8);
    policy->direct_x86_execution_supported =
        IsDirectX86ExecutionSupported();
    policy->hle_reserve_base = memory_plan.hle_reserve_base;

    if (!memory_plan.valid || memory_plan.object_regions.empty())
    {
        policy->message = "runtime memory plan is not valid";
        return false;
    }

    std::uint32_t preferred_base =
        memory_plan.object_regions.front().base_address;
    for (const runtime::RuntimeObjectRegion& region :
         memory_plan.object_regions)
    {
        preferred_base = std::min(preferred_base, region.base_address);
    }

    policy->preferred_allocation_base = preferred_base;
    policy->required_reserve_size =
        memory_plan.hle_reserve_base - preferred_base;

    if (policy->direct_x86_execution_supported)
    {
        policy->message =
            "32-bit host process can transfer control to original x86 code";
    }
    else
    {
        policy->message =
            "direct original x86 execution requires a 32-bit host process";
    }

    policy->valid = true;
    return true;
}

}  // namespace repiu::platform::win32
