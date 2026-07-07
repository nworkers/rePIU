#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/target/target_profile.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

std::string Hex32(std::uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(8)
           << std::setfill('0') << value;
    return stream.str();
}

void PrintPolicy(
    const repiu::platform::win32::Win32RuntimeMemoryPolicy& policy)
{
    std::cout << "Win32 execution host policy: "
              << (policy.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 host pointer bits: "
              << policy.host_pointer_bits << "\n";
    std::cout << "Win32 direct x86 execution: "
              << (policy.direct_x86_execution_supported ? "supported"
                                                        : "unsupported")
              << "\n";
    std::cout << "Win32 fixed reserve base: "
              << Hex32(policy.preferred_allocation_base) << "\n";
    std::cout << "Win32 fixed reserve size: "
              << Hex32(policy.required_reserve_size) << "\n";
    std::cout << "Win32 fixed reserve end: "
              << Hex32(policy.hle_reserve_base) << "\n";
    std::cout << "Win32 execution host policy message: "
              << policy.message << "\n";
}

void PrintReservation(
    const repiu::platform::win32::Win32AddressRangeReservation& reservation)
{
    std::cout << "Win32 early reservation attempt: "
              << (reservation.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 early reservation result: "
              << (reservation.reserved ? "reserved" : "not reserved")
              << "\n";
    std::cout << "Win32 requested reserve base: "
              << Hex32(reservation.requested_base) << "\n";
    std::cout << "Win32 requested reserve size: "
              << Hex32(reservation.requested_size) << "\n";
    if (reservation.reserved)
    {
        std::cout << "Win32 reserved base: "
                  << Hex32(reservation.reserved_base) << "\n";
        std::cout << "Win32 reserved size: "
                  << Hex32(reservation.reserved_size) << "\n";
    }
    if (reservation.windows_error != 0)
    {
        std::cout << "Win32 reservation error: "
                  << reservation.windows_error << "\n";
    }
    std::cout << "Win32 early reservation message: "
              << reservation.message << "\n";
}

void PrintProbe(
    const repiu::platform::win32::Win32AddressRangeProbe& probe)
{
    std::cout << "Win32 host range probe: "
              << (probe.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 host range available: "
              << (probe.range_available ? "true" : "false") << "\n";
    std::cout << "Win32 host probe base: "
              << Hex32(probe.checked_base) << "\n";
    std::cout << "Win32 host probe size: "
              << Hex32(probe.checked_size) << "\n";
    if (probe.valid && !probe.range_available)
    {
        std::cout << "Win32 host first blocking block base: "
                  << Hex32(probe.first_block_base) << "\n";
        std::cout << "Win32 host first blocking block size: "
                  << Hex32(probe.first_block_size) << "\n";
        std::cout << "Win32 host first blocking block state: "
                  << probe.first_block_state << "\n";
    }
    std::cout << "Win32 host range probe message: "
              << probe.message << "\n";
}

}  // namespace

int main()
{
    const repiu::target::TargetProfile* profile =
        repiu::target::FindTargetProfileById("piu_1st");
    if (profile == nullptr)
    {
        std::cerr << "Target profile was not found: piu_1st\n";
        return 1;
    }

    std::cout << "Execution host target: " << profile->id << "\n";
#if defined(REPIU_WIN32_HOST_IMAGE_BASE)
    std::cout << "Win32 host image base policy: "
              << Hex32(REPIU_WIN32_HOST_IMAGE_BASE) << "\n";
#endif

    if (!profile->runtime_reservation_hint.valid)
    {
        std::cerr << "Target has no runtime reservation hint\n";
        return 1;
    }

    repiu::platform::win32::Win32RuntimeMemoryPolicy policy;
    if (!repiu::platform::win32::BuildWin32RuntimeMemoryPolicyFromFixedRange(
            profile->runtime_reservation_hint.base_address,
            profile->runtime_reservation_hint.reserve_size,
            &policy))
    {
        std::cerr << "Failed to build fixed range policy\n";
        return 1;
    }

    PrintPolicy(policy);

    repiu::platform::win32::Win32AddressRangeProbe probe;
    if (!repiu::platform::win32::ProbeWin32RuntimeAddressRange(
            policy, &probe))
    {
        std::cerr << "Failed to probe fixed runtime range: "
                  << probe.message << "\n";
        return 1;
    }

    PrintProbe(probe);

    repiu::platform::win32::Win32AddressRangeReservation reservation;
    if (!repiu::platform::win32::ReserveWin32RuntimeAddressRange(
            policy, &reservation))
    {
        std::cerr << "Failed to run early reservation attempt\n";
        return 1;
    }

    PrintReservation(reservation);
    repiu::platform::win32::ReleaseWin32RuntimeAddressRange(reservation);
    return 0;
}
