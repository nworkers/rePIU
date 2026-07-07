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

struct Win32AddressRangeProbe
{
    bool valid = false;
    bool range_available = false;
    std::uint32_t checked_base = 0;
    std::uint32_t checked_size = 0;
    std::uint32_t first_block_base = 0;
    std::uint32_t first_block_size = 0;
    std::string first_block_state;
    std::string message;
};

struct Win32AddressRangeReservation
{
    bool valid = false;
    bool reserved = false;
    std::uint32_t requested_base = 0;
    std::uint32_t requested_size = 0;
    std::uint32_t reserved_base = 0;
    std::uint32_t reserved_size = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

struct Win32RelocatedImagePlacement
{
    bool valid = false;
    bool placed = false;
    std::uint32_t requested_base = 0;
    std::uint32_t requested_size = 0;
    std::uint32_t placed_base = 0;
    std::uint32_t placed_size = 0;
    std::uint32_t copied_object_count = 0;
    std::uint32_t protected_object_count = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

bool BuildWin32RuntimeMemoryPolicy(
    const runtime::RuntimeMemoryPlan& memory_plan,
    Win32RuntimeMemoryPolicy* policy);

bool BuildWin32RuntimeMemoryPolicyFromFixedRange(
    std::uint32_t preferred_allocation_base,
    std::uint32_t required_reserve_size,
    Win32RuntimeMemoryPolicy* policy);

bool ProbeWin32RuntimeAddressRange(
    const Win32RuntimeMemoryPolicy& policy,
    Win32AddressRangeProbe* probe);

bool ReserveWin32RuntimeAddressRange(
    const Win32RuntimeMemoryPolicy& policy,
    Win32AddressRangeReservation* reservation);

bool ReleaseWin32RuntimeAddressRange(
    const Win32AddressRangeReservation& reservation);

bool PlaceWin32RelocatedImage(
    const runtime::RelocatedRuntimeImage& image,
    Win32RelocatedImagePlacement* placement);

bool ReleaseWin32RelocatedImage(
    const Win32RelocatedImagePlacement& placement);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_RUNTIME_MEMORY_POLICY_H_
