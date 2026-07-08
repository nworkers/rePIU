#include "repiu/exe/dos4gw_loader.h"
#include "repiu/hle/hle_dispatcher.h"
#include "repiu/platform/win32/execution_trampoline.h"
#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/guest_context.h"
#include "repiu/runtime/image_address.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/runtime/selector_table.h"
#include "repiu/target/target_profile.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
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


void PrintByteWindow(
    const repiu::runtime::RelocatedImageByteWindow& window)
{
    std::cout << "Relocated exception byte window: "
              << (window.valid ? "valid" : "invalid") << "\n";
    std::cout << "Relocated exception byte window message: "
              << window.message << "\n";
    if (!window.valid)
    {
        return;
    }

    std::cout << "Relocated exception byte object: "
              << window.object_index << "\n";
    std::cout << "Relocated exception byte base: "
              << Hex32(window.window_base) << "\n";
    std::cout << "Relocated exception byte focus offset: "
              << Hex32(window.focus_offset) << "\n";
    std::cout << "Relocated exception bytes:";
    for (std::size_t index = 0; index < window.bytes.size(); ++index)
    {
        if (index == window.focus_offset)
        {
            std::cout << " [";
        }
        else
        {
            std::cout << " ";
        }
        std::cout << std::uppercase << std::hex << std::setw(2)
                  << std::setfill('0')
                  << static_cast<unsigned>(window.bytes[index])
                  << std::dec << std::setfill(' ');
        if (index == window.focus_offset)
        {
            std::cout << "]";
        }
    }
    std::cout << "\n";
}

void PrintGuestStackPlan(
    const repiu::runtime::GuestStackSwitchPlan& plan)
{
    std::cout << "Guest stack switch plan: "
              << (plan.valid ? "valid" : "invalid") << "\n";
    std::cout << "Guest stack switch entry: "
              << Hex32(plan.entry_eip) << "\n";
    std::cout << "Guest stack switch stack base: "
              << Hex32(plan.stack_base) << "\n";
    std::cout << "Guest stack switch stack limit: "
              << Hex32(plan.stack_limit) << "\n";
    std::cout << "Guest stack switch initial ESP: "
              << Hex32(plan.initial_esp) << "\n";
    std::cout << "Guest stack switch message: "
              << plan.message << "\n";
}

void PrintHleDispatcherTable(
    const repiu::hle::HleDispatcherTable& table)
{
    std::cout << "HLE dispatcher table: "
              << (table.valid ? "valid" : "invalid") << "\n";
    std::cout << "HLE dispatcher trap count: "
              << table.traps.size() << "\n";
    std::cout << "HLE dispatcher message: "
              << table.message << "\n";
}

void PrintParseError(const repiu::exe::ParseError& error)
{
    std::cerr << "Parse error at " << Hex32(error.file_offset) << ": "
              << error.message << "\n";
}

void PrintPolicy(
    const repiu::platform::win32::Win32RuntimeMemoryPolicy& policy)
{
    std::cout << "Win32 loader policy: "
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
    std::cout << "Win32 loader policy message: "
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

void PrintExecutionAttempt(
    const repiu::platform::win32::Win32MinimalExecutionAttempt& attempt)
{
    std::cout << "Win32 minimal execution attempt: "
              << (attempt.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 minimal execution supported: "
              << (attempt.supported ? "true" : "false") << "\n";
    std::cout << "Win32 minimal execution attempted: "
              << (attempt.attempted ? "true" : "false") << "\n";
    std::cout << "Win32 minimal execution entry: "
              << Hex32(attempt.entry_address) << "\n";
    std::cout << "Win32 minimal execution returned: "
              << (attempt.returned ? "true" : "false") << "\n";
    std::cout << "Win32 minimal execution exception caught: "
              << (attempt.exception_caught ? "true" : "false") << "\n";
    if (attempt.exception_caught)
    {
        std::cout << "Win32 minimal execution exception code: "
                  << Hex32(attempt.seh_exception_code) << "\n";
        std::cout << "Win32 minimal execution exception address: "
                  << Hex32(attempt.seh_exception_address) << "\n";
        std::cout << "Win32 minimal execution exception EAX: "
                  << Hex32(attempt.exception_eax) << "\n";
        std::cout << "Win32 minimal execution exception EBX: "
                  << Hex32(attempt.exception_ebx) << "\n";
        std::cout << "Win32 minimal execution exception ECX: "
                  << Hex32(attempt.exception_ecx) << "\n";
        std::cout << "Win32 minimal execution exception EDX: "
                  << Hex32(attempt.exception_edx) << "\n";
        std::cout << "Win32 minimal execution exception ESI: "
                  << Hex32(attempt.exception_esi) << "\n";
        std::cout << "Win32 minimal execution exception EDI: "
                  << Hex32(attempt.exception_edi) << "\n";
    }
    std::cout << "Win32 minimal execution timed out: "
              << (attempt.timed_out ? "true" : "false") << "\n";
    std::cout << "Win32 guest stack switch supported: "
              << (attempt.guest_stack_switch_supported ? "true" : "false")
              << "\n";
    std::cout << "Win32 guest stack switch attempted: "
              << (attempt.guest_stack_switch_attempted ? "true" : "false")
              << "\n";
    if (attempt.guest_stack_switch_attempted)
    {
        std::cout << "Win32 guest stack initial ESP: "
                  << Hex32(attempt.guest_stack_initial_esp) << "\n";
        if (attempt.guest_stack_return_esp != 0)
        {
            std::cout << "Win32 guest stack return ESP: "
                      << Hex32(attempt.guest_stack_return_esp) << "\n";
        }
    }
    std::cout << "Win32 minimal execution thread exit code: "
              << attempt.thread_exit_code << "\n";
    if (!attempt.hle_console_output.empty())
    {
        std::cout << "Win32 HLE console output:\n"
                  << attempt.hle_console_output;
        if (attempt.hle_console_output.back() != '\n')
        {
            std::cout << "\n";
        }
    }
    std::cout << "Win32 minimal execution message: "
              << attempt.message << "\n";
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

const repiu::target::TargetProfile* SelectTargetProfile(int argc,
                                                        char** argv)
{
    std::string_view target_id = "piu_1st";
    if (argc >= 2)
    {
        target_id = argv[1];
    }

    return repiu::target::FindTargetProfileById(target_id);
}

}  // namespace

int main(int argc, char** argv)
{
    const repiu::target::TargetProfile* profile =
        SelectTargetProfile(argc, argv);
    if (profile == nullptr)
    {
        std::cerr << "Target profile was not found\n";
        return 1;
    }

    std::cout << "Win32 loader target: " << profile->id << "\n";
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

    repiu::runtime::GuestStackSwitchPlan stack_plan;
    std::uint32_t stack_base = relocated_image.relocated_image_base;
    std::uint32_t stack_limit =
        relocated_image.relocated_stack_top_linear_address;
    if (load_result.le_header.stack_object > 0 &&
        load_result.le_header.stack_object <=
            relocatable_plan.object_regions.size())
    {
        const repiu::runtime::RelocatableRuntimeObjectRegion& stack_region =
            relocatable_plan
                .object_regions[load_result.le_header.stack_object - 1];
        stack_base = stack_region.relocated_base_address;
        stack_limit = stack_region.relocated_base_address +
                      stack_region.virtual_size;
    }
    const std::uint32_t stack_size = stack_limit > stack_base
                                         ? stack_limit - stack_base
                                         : 0;
    const std::uint32_t guard_bytes = stack_size > 8192 ? 4096 : 256;
    repiu::runtime::BuildGuestStackSwitchPlan(
        relocated_image.relocated_entry_linear_address,
        stack_base,
        stack_limit,
        relocated_image.relocated_stack_top_linear_address,
        guard_bytes,
        &stack_plan);
    PrintGuestStackPlan(stack_plan);

    repiu::hle::HleDispatcherTable hle_dispatcher;
    repiu::hle::BuildInitialHleDispatcherTable(&hle_dispatcher);
    PrintHleDispatcherTable(hle_dispatcher);

    repiu::platform::win32::Win32RelocatedImagePlacement placement;
    if (!repiu::platform::win32::PlaceWin32RelocatedImage(
            relocated_image,
            profile->runtime_reservation_hint.reserve_size,
            &placement))
    {
        std::cerr << "Failed to place relocated image\n";
        return 1;
    }

    PrintPlacement(placement);
    std::cout.flush();
    repiu::platform::win32::Win32MinimalExecutionAttempt attempt;
    const bool use_dos_console_hle = profile->id == "dos4gw_hello";
    const bool attempted_execution =
        use_dos_console_hle
            ? repiu::platform::win32::AttemptWin32GuestStackHleExecution(
                  placement,
                  stack_plan,
                  1000,
                  &attempt)
            : repiu::platform::win32::AttemptWin32GuestStackExecution(
                  placement,
                  stack_plan,
                  1000,
                  &attempt);
    if (!attempted_execution)
    {
        std::cerr << "Failed to attempt minimal original entry execution\n";
        repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
        return 1;
    }

    PrintExecutionAttempt(attempt);
    if (attempt.exception_caught)
    {
        repiu::runtime::RelocatedImageByteWindow window;
        repiu::runtime::BuildRelocatedImageByteWindow(
            relocated_image,
            attempt.seh_exception_address,
            16,
            16,
            &window);
        PrintByteWindow(window);
    }
    repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
    return 0;
}
