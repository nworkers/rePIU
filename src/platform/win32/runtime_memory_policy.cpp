#include "repiu/platform/win32/runtime_memory_policy.h"

#include <algorithm>
#include <cstring>
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

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment)
{
    const std::uint32_t remainder = value % alignment;
    if (remainder == 0)
    {
        return value;
    }

    return value + (alignment - remainder);
}

DWORD ProtectionFromObjectFlags(std::uint32_t flags)
{
    const bool writable = (flags & 0x00000002) != 0;
    const bool executable = (flags & 0x00000004) != 0;

    if (executable && writable)
    {
        return PAGE_EXECUTE_READWRITE;
    }

    if (executable)
    {
        return PAGE_EXECUTE_READ;
    }

    if (writable)
    {
        return PAGE_READWRITE;
    }

    return PAGE_READONLY;
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

bool PlaceWin32RelocatedImage(
    const runtime::RelocatedRuntimeImage& image,
    Win32RelocatedImagePlacement* placement)
{
    if (placement == nullptr)
    {
        return false;
    }

    *placement = Win32RelocatedImagePlacement{};
    placement->requested_base = image.relocated_image_base;

    if (!image.valid || image.objects.empty())
    {
        placement->message = "relocated runtime image is not valid";
        return false;
    }

    std::uint32_t min_base = image.objects.front().relocated_base_address;
    std::uint64_t max_end = 0;
    for (const runtime::RelocatedRuntimeObject& object : image.objects)
    {
        min_base = std::min(min_base, object.relocated_base_address);
        const std::uint64_t object_end =
            static_cast<std::uint64_t>(object.relocated_base_address) +
            object.virtual_size;
        max_end = std::max(max_end, object_end);
    }

    if (max_end > std::numeric_limits<std::uint32_t>::max() ||
        max_end <= min_base)
    {
        placement->message = "relocated image range is not valid";
        return false;
    }

    const std::uint32_t reserve_size =
        AlignUp(static_cast<std::uint32_t>(max_end) - min_base, 4096);
    placement->requested_base = min_base;
    placement->requested_size = reserve_size;

    void* requested_address = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(min_base));
    void* placed_address = VirtualAlloc(
        requested_address,
        static_cast<SIZE_T>(reserve_size),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);

    placement->valid = true;
    if (placed_address == nullptr)
    {
        placement->windows_error = GetLastError();
        std::ostringstream stream;
        stream << "VirtualAlloc relocated image failed with error "
               << placement->windows_error;
        placement->message = stream.str();
        return true;
    }

    placement->placed_base = ClampToUint32(
        reinterpret_cast<std::uintptr_t>(placed_address));
    placement->placed_size = reserve_size;

    if (placement->placed_base != min_base)
    {
        placement->windows_error = ERROR_INVALID_ADDRESS;
        placement->message =
            "VirtualAlloc returned a different relocated image address";
        VirtualFree(placed_address, 0, MEM_RELEASE);
        placement->placed_base = 0;
        placement->placed_size = 0;
        return true;
    }

    for (const runtime::RelocatedRuntimeObject& object : image.objects)
    {
        void* destination = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(object.relocated_base_address));
        if (!object.memory.empty())
        {
            std::memcpy(destination, object.memory.data(),
                        object.memory.size());
        }
        ++placement->copied_object_count;
    }

    for (const runtime::RelocatedRuntimeObject& object : image.objects)
    {
        void* object_address = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(object.relocated_base_address));
        const SIZE_T object_size =
            static_cast<SIZE_T>(AlignUp(object.virtual_size, 4096));
        DWORD old_protection = 0;
        if (VirtualProtect(object_address,
                           object_size,
                           ProtectionFromObjectFlags(object.flags),
                           &old_protection) == 0)
        {
            placement->windows_error = GetLastError();
            std::ostringstream stream;
            stream << "VirtualProtect relocated object failed with error "
                   << placement->windows_error;
            placement->message = stream.str();
            VirtualFree(placed_address, 0, MEM_RELEASE);
            placement->placed_base = 0;
            placement->placed_size = 0;
            return true;
        }
        ++placement->protected_object_count;
    }

    FlushInstructionCache(GetCurrentProcess(), placed_address, reserve_size);
    placement->placed = true;
    placement->message = "relocated image placed in Win32 process memory";
    return true;
}

bool ReleaseWin32RelocatedImage(
    const Win32RelocatedImagePlacement& placement)
{
    if (!placement.valid || !placement.placed)
    {
        return true;
    }

    void* placed_address = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement.placed_base));
    return VirtualFree(placed_address, 0, MEM_RELEASE) != 0;
}

}  // namespace repiu::platform::win32
