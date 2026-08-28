#include "repiu/engine/runtime_memory_policy.h"

#include "repiu/platform/virtual_memory.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <limits>
#include <sstream>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
using DWORD = unsigned long;
constexpr DWORD MEM_COMMIT = 0x00001000;
constexpr DWORD MEM_FREE = 0x00010000;
constexpr DWORD MEM_RESERVE = 0x00002000;
constexpr DWORD PAGE_READONLY = 0x02;
constexpr DWORD PAGE_READWRITE = 0x04;
constexpr DWORD PAGE_EXECUTE_READ = 0x20;
constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40;
#endif

namespace repiu::engine
{
namespace
{

bool IsDirectX86ExecutionSupported()
{
// Task 503d-19: the same answer the trampoline gives, and for the same reason.
// Running the guest's code in this process needs a 32-bit x86 host; the Win32
// APIs it used to also need are in the platform layer.
#if defined(_M_IX86) || defined(__i386__)
    return true;
#else
    return false;
#endif
}

// Task 503d-19. The three states a region can be in, as the diagnostic that
// reads them has always spelled them.
//
// The Windows names are kept rather than renamed. What this string ends up in
// is a loader log line that a person reads next to a Windows address map, and
// the layer's `claimed` and `committed` reproduce the same three states exactly
// -- so the spelling costs nothing and the familiarity is worth keeping.
std::string RegionStateName(const repiu::platform::MemoryRegion& region)
{
    if (!region.claimed)
    {
        return "MEM_FREE";
    }
    return region.committed ? "MEM_COMMIT" : "MEM_RESERVE";
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

// Task 503d-19: the object's own flags become the layer's protection rather
// than a Win32 constant. The four cases are the same four.
repiu::platform::MemoryProtection ProtectionFromObjectFlags(std::uint32_t flags)
{
    const bool writable = (flags & 0x00000002) != 0;
    const bool executable = (flags & 0x00000004) != 0;

    if (executable && writable)
    {
        return repiu::platform::MemoryProtection::kExecuteReadWrite;
    }

    if (executable)
    {
        return repiu::platform::MemoryProtection::kExecuteRead;
    }

    if (writable)
    {
        return repiu::platform::MemoryProtection::kReadWrite;
    }

    return repiu::platform::MemoryProtection::kReadOnly;
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
        // Task 503d-19: onto 3b, which answers `claimed` directly. What stood
        // here compared MEM_FREE against the other two states, and this is the
        // question that comparison was asking.
        const repiu::platform::MemoryRegion region =
            repiu::platform::QueryMemory(
                reinterpret_cast<const void*>(current));
        if (!region.valid)
        {
            probe->message = "memory query failed while probing address range";
            return false;
        }

        const std::uintptr_t region_base =
            reinterpret_cast<std::uintptr_t>(region.base);
        const std::uintptr_t region_size =
            static_cast<std::uintptr_t>(region.size);
        const std::uintptr_t region_end = region_base + region_size;

        if (region.claimed)
        {
            probe->valid = true;
            probe->range_available = false;
            probe->first_block_base = ClampToUint32(region_base);
            probe->first_block_size = ClampToUint32(region_size);
            probe->first_block_state = RegionStateName(region);
            probe->message = "target address range is not fully free";
            return true;
        }

        if (region_end <= current)
        {
            probe->message = "memory query returned a non-advancing region";
            return false;
        }

        current = region_end;
    }

    probe->valid = true;
    probe->range_available = true;
    probe->message = "target address range is fully free";
    return true;
}

bool ReserveWin32RuntimeAddressRangeWithType(
    const Win32RuntimeMemoryPolicy& policy,
    bool commit,
    const char* operation_name,
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
    // Task 503d-19: onto 3b. The allocation type collapses into `commit`,
    // which is the only thing the two callers differ in.
    const repiu::platform::MemoryReservation reserved =
        repiu::platform::ReserveMemory(
            requested_address,
            static_cast<std::size_t>(policy.required_reserve_size),
            commit,
            repiu::platform::MemoryProtection::kReadWrite);

    reservation->valid = true;
    if (!reserved.valid || reserved.base == nullptr)
    {
        reservation->windows_error = reserved.error;
        std::ostringstream stream;
        stream << "reservation " << operation_name
               << " failed with host error " << reservation->windows_error;
        reservation->message = stream.str();
        return true;
    }

    reservation->reserved = true;
    reservation->reserved_base = ClampToUint32(
        reinterpret_cast<std::uintptr_t>(reserved.base));
    reservation->reserved_size = policy.required_reserve_size;

    if (reserved.base == requested_address)
    {
        reservation->message = operation_name;
        reservation->message += " target address range";
    }
    else
    {
        // 3b's header states this outright: a preferred base is a request and
        // the hosts fail it differently, so every caller compares.
        reservation->message =
            "reservation returned a different address than requested";
    }

    return true;
}

bool ReserveWin32RuntimeAddressRange(
    const Win32RuntimeMemoryPolicy& policy,
    Win32AddressRangeReservation* reservation)
{
    return ReserveWin32RuntimeAddressRangeWithType(
        policy, false, "reserve", reservation);
}

bool ReserveAndCommitWin32RuntimeAddressRange(
    const Win32RuntimeMemoryPolicy& policy,
    Win32AddressRangeReservation* reservation)
{
    return ReserveWin32RuntimeAddressRangeWithType(
        policy, true, "reserve and commit", reservation);
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
    return repiu::platform::ReleaseMemory(
        reserved_address, static_cast<std::size_t>(reservation.reserved_size));
}

bool PlaceWin32RelocatedImage(
    const runtime::RelocatedRuntimeImage& image,
    Win32RelocatedImagePlacement* placement)
{
    return PlaceWin32RelocatedImage(image, 0, placement);
}

bool PlaceWin32RelocatedImage(
    const runtime::RelocatedRuntimeImage& image,
    std::uint32_t minimum_reserve_size,
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

    const std::uint32_t image_reserve_size =
        AlignUp(static_cast<std::uint32_t>(max_end) - min_base, 4096);
    const std::uint32_t reserve_size =
        std::max(image_reserve_size, AlignUp(minimum_reserve_size, 4096));
    placement->requested_base = min_base;
    placement->requested_size = reserve_size;

    void* requested_address = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(min_base));
    const repiu::platform::MemoryReservation reserved =
        repiu::platform::ReserveMemory(
            requested_address,
            static_cast<std::size_t>(reserve_size),
            true,
            repiu::platform::MemoryProtection::kReadWrite);
    void* placed_address = reserved.valid ? reserved.base : nullptr;

    placement->valid = true;
    if (placed_address == nullptr)
    {
        placement->windows_error = reserved.error;
        std::ostringstream stream;
        stream << "relocated image reservation failed with host error "
               << placement->windows_error;
        placement->message = stream.str();
        return true;
    }

    placement->placed_base = ClampToUint32(
        reinterpret_cast<std::uintptr_t>(placed_address));
    placement->placed_size = reserve_size;
    placement->selector_bindings = image.selector_bindings;

    if (placement->placed_base != min_base)
    {
        placement->windows_error = 0;
        placement->message =
            "reservation returned a different relocated image address";
        repiu::platform::ReleaseMemory(placed_address, reserve_size);
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
        const std::size_t object_size =
            static_cast<std::size_t>(AlignUp(object.virtual_size, 4096));
        if (!repiu::platform::ProtectMemory(
                object_address, object_size,
                ProtectionFromObjectFlags(object.flags), nullptr))
        {
            placement->windows_error = 0;
            placement->message =
                "protecting a relocated object failed";
            repiu::platform::ReleaseMemory(placed_address, reserve_size);
            placement->placed_base = 0;
            placement->placed_size = 0;
            return true;
        }
        ++placement->protected_object_count;
    }

    repiu::platform::FlushInstructionCacheRange(placed_address, reserve_size);
    placement->placed = true;
    placement->message = "relocated image placed in host process memory";
    return true;
}

bool PlaceWin32RelocatedImageInReservedRange(
    const runtime::RelocatedRuntimeImage& image,
    const Win32AddressRangeReservation& reservation,
    Win32RelocatedImagePlacement* placement)
{

    if (placement == nullptr)
    {
        return false;
    }

    *placement = Win32RelocatedImagePlacement{};
    placement->requested_base = reservation.requested_base;
    placement->requested_size = reservation.requested_size;

    if (!reservation.valid || !reservation.reserved)
    {
        placement->message = "relocated image placement requires a reserved range";
        return false;
    }

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

    const std::uint32_t image_reserve_size =
        AlignUp(static_cast<std::uint32_t>(max_end) - min_base, 4096);
    if (reservation.reserved_base != min_base ||
        reservation.reserved_size < image_reserve_size)
    {
        placement->message =
            "reserved range does not cover relocated image range";
        return false;
    }

    placement->valid = true;
    placement->placed_base = reservation.reserved_base;
    placement->placed_size = reservation.reserved_size;
    placement->hle_reserve_base = image.relocated_hle_reserve_base;
    placement->arena_end_address =
        reservation.reserved_base + reservation.reserved_size;
    placement->selector_bindings = image.selector_bindings;

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
        const std::size_t object_size =
            static_cast<std::size_t>(AlignUp(object.virtual_size, 4096));
        if (!repiu::platform::ProtectMemory(
                object_address, object_size,
                ProtectionFromObjectFlags(object.flags), nullptr))
        {
            placement->windows_error = 0;
            placement->message = "protecting a relocated object failed";
            placement->placed_base = 0;
            placement->placed_size = 0;
            return true;
        }
        ++placement->protected_object_count;
    }

    repiu::platform::FlushInstructionCacheRange(
        reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(placement->placed_base)),
        placement->placed_size);
    placement->placed = true;
    placement->message =
        "relocated image placed in precommitted host process memory";
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
    return repiu::platform::ReleaseMemory(
        placed_address, static_cast<std::size_t>(placement.placed_size));
}

}  // namespace repiu::engine
