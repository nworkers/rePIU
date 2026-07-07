#include "repiu/platform/win32/runtime_memory_policy.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

std::string MemoryStateName(DWORD state)
{
    switch (state)
    {
        case MEM_COMMIT:
            return "MEM_COMMIT";
        case MEM_FREE:
            return "MEM_FREE";
        case MEM_RESERVE:
            return "MEM_RESERVE";
        default:
            return "UNKNOWN";
    }
}

std::uint32_t ClampToUint32(std::uintptr_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
    {
        return std::numeric_limits<std::uint32_t>::max();
    }

    return static_cast<std::uint32_t>(value);
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

bool BuildWin32RuntimeMemoryPolicyFromFixedRange(
    std::uint32_t preferred_allocation_base,
    std::uint32_t required_reserve_size,
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
    policy->preferred_allocation_base = preferred_allocation_base;
    policy->required_reserve_size = required_reserve_size;
    policy->hle_reserve_base =
        preferred_allocation_base + required_reserve_size;

    if (required_reserve_size == 0 ||
        policy->hle_reserve_base <= preferred_allocation_base)
    {
        policy->message = "fixed runtime memory range is not valid";
        return false;
    }

    if (policy->direct_x86_execution_supported)
    {
        policy->message =
            "32-bit host process can reserve original x86 runtime range";
    }
    else
    {
        policy->message =
            "fixed runtime range reservation is intended for 32-bit hosts";
    }

    policy->valid = true;
    return true;
}

bool ProbeWin32RuntimeAddressRange(
    const Win32RuntimeMemoryPolicy& policy,
    Win32AddressRangeProbe* probe)
{
    if (probe == nullptr)
    {
        return false;
    }

    *probe = Win32AddressRangeProbe{};
    probe->checked_base = policy.preferred_allocation_base;
    probe->checked_size = policy.required_reserve_size;

    if (!policy.valid)
    {
        probe->message = "Win32 runtime memory policy is not valid";
        return false;
    }

    if (policy.required_reserve_size == 0)
    {
        probe->message = "required reserve size is zero";
        return false;
    }

    const std::uintptr_t base =
        static_cast<std::uintptr_t>(policy.preferred_allocation_base);
    const std::uintptr_t size =
        static_cast<std::uintptr_t>(policy.required_reserve_size);
    const std::uintptr_t end = base + size;

    if (end <= base)
    {
        probe->message = "address range overflows host pointer size";
        return false;
    }

    std::uintptr_t current = base;
    while (current < end)
    {
        MEMORY_BASIC_INFORMATION memory_info{};
        const SIZE_T query_size = VirtualQuery(
            reinterpret_cast<LPCVOID>(current),
            &memory_info,
            sizeof(memory_info));
        if (query_size == 0)
        {
            probe->message = "VirtualQuery failed while probing address range";
            return false;
        }

        const std::uintptr_t region_base =
            reinterpret_cast<std::uintptr_t>(memory_info.BaseAddress);
        const std::uintptr_t region_size =
            static_cast<std::uintptr_t>(memory_info.RegionSize);
        const std::uintptr_t region_end = region_base + region_size;

        if (memory_info.State != MEM_FREE)
        {
            probe->valid = true;
            probe->range_available = false;
            probe->first_block_base = ClampToUint32(region_base);
            probe->first_block_size = ClampToUint32(region_size);
            probe->first_block_state = MemoryStateName(memory_info.State);
            probe->message = "target address range is not fully free";
            return true;
        }

        if (region_end <= current)
        {
            probe->message = "VirtualQuery returned a non-advancing region";
            return false;
        }

        current = region_end;
    }

    probe->valid = true;
    probe->range_available = true;
    probe->message = "target address range is fully free";
    return true;
}

bool ReserveWin32RuntimeAddressRange(
    const Win32RuntimeMemoryPolicy& policy,
    Win32AddressRangeReservation* reservation)
{
    if (reservation == nullptr)
    {
        return false;
    }

    *reservation = Win32AddressRangeReservation{};
    reservation->requested_base = policy.preferred_allocation_base;
    reservation->requested_size = policy.required_reserve_size;

    if (!policy.valid)
    {
        reservation->message = "Win32 runtime memory policy is not valid";
        return false;
    }

    void* requested_address = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(policy.preferred_allocation_base));
    void* reserved_address = VirtualAlloc(
        requested_address,
        static_cast<SIZE_T>(policy.required_reserve_size),
        MEM_RESERVE,
        PAGE_READWRITE);

    reservation->valid = true;
    if (reserved_address == nullptr)
    {
        reservation->windows_error = GetLastError();
        std::ostringstream stream;
        stream << "VirtualAlloc MEM_RESERVE failed with error "
               << reservation->windows_error;
        reservation->message = stream.str();
        return true;
    }

    reservation->reserved = true;
    reservation->reserved_base = ClampToUint32(
        reinterpret_cast<std::uintptr_t>(reserved_address));
    reservation->reserved_size = policy.required_reserve_size;

    if (reserved_address == requested_address)
    {
        reservation->message = "target address range reserved";
    }
    else
    {
        reservation->message =
            "VirtualAlloc reserved a different address than requested";
    }

    return true;
}

bool ReleaseWin32RuntimeAddressRange(
    const Win32AddressRangeReservation& reservation)
{
    if (!reservation.valid || !reservation.reserved)
    {
        return true;
    }

    void* reserved_address = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(reservation.reserved_base));
    return VirtualFree(reserved_address, 0, MEM_RELEASE) != 0;
}

}  // namespace repiu::platform::win32
