#include "repiu/exe/dos4gw_loader.h"
#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/target/target_profile.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

std::string Hex32(std::uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(8)
           << std::setfill('0') << value;
    return stream.str();
}

bool ReadBinaryFile(const std::filesystem::path& path,
                    std::vector<std::uint8_t>* data,
                    std::string* error_message)
{
    if (data == nullptr)
    {
        if (error_message != nullptr)
        {
            *error_message = "output buffer is null";
        }
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        if (error_message != nullptr)
        {
            *error_message = "failed to open file";
        }
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0)
    {
        if (error_message != nullptr)
        {
            *error_message = "failed to determine file size";
        }
        return false;
    }

    file.seekg(0, std::ios::beg);
    data->resize(static_cast<std::size_t>(size));
    if (!data->empty())
    {
        file.read(reinterpret_cast<char*>(data->data()), size);
        if (!file)
        {
            if (error_message != nullptr)
            {
                *error_message = "failed to read file contents";
            }
            return false;
        }
    }

    return true;
}

void PrintParseError(const repiu::exe::ParseError& error)
{
    std::cerr << "Parse error at " << Hex32(error.file_offset) << ": "
              << error.message << "\n";
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

void PrintPlacement(
    const repiu::platform::win32::Win32RelocatedImagePlacement& placement)
{
    std::cout << "Win32 relocated image placement: "
              << (placement.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 relocated image placement result: "
              << (placement.placed ? "placed" : "not placed") << "\n";
    std::cout << "Win32 relocated image requested base: "
              << Hex32(placement.requested_base) << "\n";
    std::cout << "Win32 relocated image requested size: "
              << Hex32(placement.requested_size) << "\n";
    if (placement.placed)
    {
        std::cout << "Win32 relocated image placed base: "
                  << Hex32(placement.placed_base) << "\n";
        std::cout << "Win32 relocated image placed size: "
                  << Hex32(placement.placed_size) << "\n";
    }
    std::cout << "Win32 relocated image copied objects: "
              << placement.copied_object_count << "\n";
    std::cout << "Win32 relocated image protected objects: "
              << placement.protected_object_count << "\n";
    if (placement.windows_error != 0)
    {
        std::cout << "Win32 relocated image placement error: "
                  << placement.windows_error << "\n";
    }
    std::cout << "Win32 relocated image placement message: "
              << placement.message << "\n";
}

bool SelectRelocatedImageBase(std::uint32_t reserve_size,
                              std::uint32_t* selected_base)
{
    if (selected_base == nullptr)
    {
        return false;
    }

    const std::vector<std::uint32_t> candidates = {
        0x01000000,
        0x02000000,
        0x03000000,
        0x04000000,
        0x05000000,
        0x06000000,
        0x07000000,
        0x08000000,
        0x09000000,
    };

    for (std::uint32_t candidate : candidates)
    {
        repiu::platform::win32::Win32RuntimeMemoryPolicy policy;
        if (!repiu::platform::win32::BuildWin32RuntimeMemoryPolicyFromFixedRange(
                candidate, reserve_size, &policy))
        {
            continue;
        }

        repiu::platform::win32::Win32AddressRangeProbe probe;
        if (!repiu::platform::win32::ProbeWin32RuntimeAddressRange(
                policy, &probe))
        {
            continue;
        }

        std::cout << "Win32 relocated base candidate "
                  << Hex32(candidate) << ": "
                  << (probe.range_available ? "available" : "occupied")
                  << "\n";
        if (probe.range_available)
        {
            *selected_base = candidate;
            return true;
        }
    }

    return false;
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

    std::vector<std::uint8_t> data;
    std::string read_error;
    if (!ReadBinaryFile(profile->executable_path, &data, &read_error))
    {
        std::cerr << "Failed to read "
                  << profile->executable_path.string()
                  << ": " << read_error << "\n";
        return 1;
    }

    repiu::exe::ParseError error;
    repiu::exe::Dos4gwLoadResult load_result;
    if (!repiu::exe::LoadDos4gwExecutable(data, *profile, &load_result,
                                          &error))
    {
        PrintParseError(error);
        return 1;
    }

    std::uint32_t relocated_image_base = 0;
    if (!SelectRelocatedImageBase(profile->runtime_reservation_hint.reserve_size,
                                  &relocated_image_base))
    {
        std::cerr << "Failed to find an available relocated image base\n";
        return 1;
    }

    std::cout << "Win32 selected relocated image base: "
              << Hex32(relocated_image_base) << "\n";

    repiu::runtime::RelocatableRuntimeImagePlan relocatable_plan;
    if (!repiu::runtime::BuildRelocatableRuntimeImagePlan(
            load_result, relocated_image_base, &relocatable_plan, &error))
    {
        PrintParseError(error);
        return 1;
    }

    repiu::runtime::RelocatedRuntimeImage relocated_image;
    if (!repiu::runtime::BuildRelocatedRuntimeImage(
            load_result, relocatable_plan, &relocated_image, &error))
    {
        PrintParseError(error);
        return 1;
    }

    repiu::platform::win32::Win32RelocatedImagePlacement placement;
    if (!repiu::platform::win32::PlaceWin32RelocatedImage(
            relocated_image, &placement))
    {
        std::cerr << "Failed to place relocated image\n";
        return 1;
    }

    PrintPlacement(placement);
    repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
    return 0;
}
