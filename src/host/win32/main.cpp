#include "repiu/exe/dos4gw_loader.h"
#include "repiu/hle/dos_file_system.h"
#include "repiu/hle/hle_dispatcher.h"
#include "repiu/hle/privileged_instruction.h"
#include "repiu/platform/win32/execution_trampoline.h"
#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/guest_context.h"
#include "repiu/runtime/image_address.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/runtime/runtime_memory_arena.h"
#include "repiu/runtime/selector_table.h"
#include "repiu/target/target_profile.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::shared_ptr<spdlog::logger> CreateLoaderLogger()
{
    std::shared_ptr<spdlog::logger> logger =
        spdlog::stderr_color_mt("loader");
    logger->set_pattern("[%X.%e] [%8l] [%n] %v");
    logger->set_level(spdlog::level::info);
    return logger;
}

std::string Hex32(std::uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(8)
           << std::setfill('0') << value;
    return stream.str();
}

std::string Hex8(std::uint8_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(2)
           << std::setfill('0') << static_cast<unsigned>(value);
    return stream.str();
}

std::string Hex16(std::uint16_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(4)
           << std::setfill('0') << value;
    return stream.str();
}

const char* SegmentRegisterName(std::uint32_t segment_register)
{
    switch (segment_register)
    {
        case 0:
            return "ES";
        case 2:
            return "SS";
        case 3:
            return "DS";
        case 4:
            return "FS";
        case 5:
            return "GS";
        default:
            return "unknown";
    }
}

void WriteGuestOutput(std::string_view output)
{
    if (output.empty())
    {
        return;
    }

    std::fwrite(output.data(), 1, output.size(), stdout);
    if (output.back() != '\n')
    {
        std::fwrite("\n", 1, 1, stdout);
    }
    std::fflush(stdout);
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


std::string BuildByteWindowText(
    const repiu::runtime::RelocatedImageByteWindow& window)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < window.bytes.size(); ++index)
    {
        if (index == window.focus_offset)
        {
            stream << " [";
        }
        else
        {
            stream << " ";
        }
        stream << std::uppercase << std::hex << std::setw(2)
               << std::setfill('0')
               << static_cast<unsigned>(window.bytes[index])
               << std::dec << std::setfill(' ');
        if (index == window.focus_offset)
        {
            stream << "]";
        }
    }
    return stream.str();
}

void PrintByteWindow(
    spdlog::logger& logger,
    const repiu::runtime::RelocatedImageByteWindow& window)
{
    logger.error("Relocated exception byte window: {}",
                 window.valid ? "valid" : "invalid");
    logger.error("Relocated exception byte window message: {}",
                 window.message);
    if (!window.valid)
    {
        return;
    }

    logger.error("Relocated exception byte object: {}",
                 window.object_index);
    logger.error("Relocated exception byte base: {}",
                 Hex32(window.window_base));
    logger.error("Relocated exception byte focus offset: {}",
                 Hex32(window.focus_offset));
    logger.error("Relocated exception bytes:{}",
                 BuildByteWindowText(window));
}

void PrintPrivilegedInstructionClassification(
    spdlog::logger& logger,
    const repiu::hle::PrivilegedInstructionClassification& classification)
{
    const bool current_blocker = !classification.valid;
    if (current_blocker)
    {
        logger.error("Privileged instruction classification: unknown");
        logger.error("Privileged instruction opcode: {}",
                     Hex8(classification.opcode));
    }
    else
    {
        logger.info("Privileged instruction classification: valid");
        logger.info("Privileged instruction opcode: {}",
                    Hex8(classification.opcode));
    }
    if (classification.valid)
    {
        logger.info("Privileged instruction mnemonic: {}",
                    classification.mnemonic);
        logger.info("Privileged instruction length: {}",
                    classification.length);
        logger.info("Privileged instruction class: {}",
                    repiu::hle::PrivilegedInstructionClassName(
                        classification.instruction_class));
        logger.info("Privileged instruction HLE trap candidate: {}",
                    classification.hle_trap_candidate ? "true" : "false");
        logger.info("Privileged instruction CPU/DPMI state candidate: {}",
                    classification.cpu_state_initialization_candidate
                        ? "true"
                        : "false");
    }
    if (current_blocker)
    {
        logger.error("Privileged instruction classification message: {}",
                     classification.message);
        logger.error("Current execution blocker: unhandled or unclassified "
                     "instruction/memory access at exception point");
    }
    else
    {
        logger.info("Privileged instruction classification message: {}",
                    classification.message);
    }
}

void PrintGuestStackPlan(
    spdlog::logger& logger,
    const repiu::runtime::GuestStackSwitchPlan& plan)
{
    logger.info("Guest stack switch plan: {}",
                plan.valid ? "valid" : "invalid");
    logger.info("Guest stack switch entry: {}", Hex32(plan.entry_eip));
    logger.info("Guest stack switch stack base: {}",
                Hex32(plan.stack_base));
    logger.info("Guest stack switch stack limit: {}",
                Hex32(plan.stack_limit));
    logger.info("Guest stack switch initial ESP: {}",
                Hex32(plan.initial_esp));
    logger.info("Guest stack switch message: {}", plan.message);
}

void PrintHleDispatcherTable(
    spdlog::logger& logger,
    const repiu::hle::HleDispatcherTable& table)
{
    logger.info("HLE dispatcher table: {}",
                table.valid ? "valid" : "invalid");
    logger.info("HLE dispatcher trap count: {}", table.traps.size());
    logger.info("HLE dispatcher message: {}", table.message);
}

void PrintRuntimeMemoryArenaPlan(
    spdlog::logger& logger,
    const repiu::runtime::RuntimeMemoryArenaPlan& plan)
{
    logger.info("Runtime memory arena plan: {}",
                plan.valid ? "valid" : "invalid");
    logger.info("Runtime memory arena base: {}",
                Hex32(plan.base_address));
    logger.info("Runtime memory arena image reserve size: {}",
                Hex32(plan.image_reserve_size));
    logger.info("Runtime memory arena expansion slack size: {}",
                Hex32(plan.expansion_slack_size));
    if (plan.valid)
    {
        logger.info("Runtime memory arena reserve size: {}",
                    Hex32(plan.arena_reserve_size));
        logger.info("Runtime memory arena end: {}",
                    Hex32(plan.arena_end_address));
    }
    if (!plan.message.empty())
    {
        logger.info("Runtime memory arena message: {}", plan.message);
    }
}

void PrintDosVirtualFileSystem(
    spdlog::logger& logger,
    const repiu::hle::DosVirtualFileSystemState& state)
{
    logger.info("DOS virtual filesystem: {}",
                state.valid ? "valid" : "invalid");
    logger.info("DOS virtual filesystem root: {}",
                state.host_root.string());
    logger.info("DOS virtual filesystem message: {}", state.message);
}

void PrintParseError(spdlog::logger& logger,
                     const repiu::exe::ParseError& error)
{
    logger.error("Parse error at {}: {}",
                 Hex32(error.file_offset),
                 error.message);
}

void PrintPolicy(
    spdlog::logger& logger,
    const repiu::platform::win32::Win32RuntimeMemoryPolicy& policy)
{
    logger.info("Win32 loader policy: {}",
                policy.valid ? "valid" : "invalid");
    logger.info("Win32 host pointer bits: {}", policy.host_pointer_bits);
    logger.info("Win32 direct x86 execution: {}",
                policy.direct_x86_execution_supported ? "supported"
                                                       : "unsupported");
    logger.info("Win32 fixed reserve base: {}",
                Hex32(policy.preferred_allocation_base));
    logger.info("Win32 fixed reserve size: {}",
                Hex32(policy.required_reserve_size));
    logger.info("Win32 fixed reserve end: {}",
                Hex32(policy.hle_reserve_base));
    logger.info("Win32 loader policy message: {}", policy.message);
}

void PrintReservation(
    spdlog::logger& logger,
    const repiu::platform::win32::Win32AddressRangeReservation& reservation)
{
    logger.info("Win32 early reservation attempt: {}",
                reservation.valid ? "valid" : "invalid");
    if (reservation.reserved)
    {
        logger.info("Win32 early reservation result: reserved");
    }
    else
    {
        logger.warn("Win32 early reservation result: not reserved");
    }
    logger.info("Win32 requested reserve base: {}",
                Hex32(reservation.requested_base));
    logger.info("Win32 requested reserve size: {}",
                Hex32(reservation.requested_size));
    if (reservation.reserved)
    {
        logger.info("Win32 reserved base: {}",
                    Hex32(reservation.reserved_base));
        logger.info("Win32 reserved size: {}",
                    Hex32(reservation.reserved_size));
    }
    if (reservation.windows_error != 0)
    {
        logger.warn("Win32 reservation error: {}",
                    reservation.windows_error);
    }
    if (reservation.reserved)
    {
        logger.info("Win32 early reservation message: {}",
                    reservation.message);
    }
    else
    {
        logger.warn("Win32 early reservation message: {}",
                    reservation.message);
    }
}

void PrintProbe(
    spdlog::logger& logger,
    const repiu::platform::win32::Win32AddressRangeProbe& probe)
{
    logger.info("Win32 host range probe: {}",
                probe.valid ? "valid" : "invalid");
    if (probe.range_available)
    {
        logger.info("Win32 host range available: true");
    }
    else
    {
        logger.warn("Win32 host range available: false");
    }
    logger.info("Win32 host probe base: {}",
                Hex32(probe.checked_base));
    logger.info("Win32 host probe size: {}",
                Hex32(probe.checked_size));
    if (probe.valid && !probe.range_available)
    {
        logger.warn("Win32 host first blocking block base: {}",
                    Hex32(probe.first_block_base));
        logger.warn("Win32 host first blocking block size: {}",
                    Hex32(probe.first_block_size));
        logger.warn("Win32 host first blocking block state: {}",
                    probe.first_block_state);
    }
    if (probe.range_available)
    {
        logger.info("Win32 host range probe message: {}", probe.message);
    }
    else
    {
        logger.warn("Win32 host range probe message: {}", probe.message);
    }
}

void PrintPlacement(
    spdlog::logger& logger,
    const repiu::platform::win32::Win32RelocatedImagePlacement& placement)
{
    logger.info("Win32 relocated image placement: {}",
                placement.valid ? "valid" : "invalid");
    logger.info("Win32 relocated image placement result: {}",
                placement.placed ? "placed" : "not placed");
    logger.info("Win32 relocated image requested base: {}",
                Hex32(placement.requested_base));
    logger.info("Win32 relocated image requested size: {}",
                Hex32(placement.requested_size));
    if (placement.placed)
    {
        logger.info("Win32 relocated image placed base: {}",
                    Hex32(placement.placed_base));
        logger.info("Win32 relocated image placed size: {}",
                    Hex32(placement.placed_size));
    }
    logger.info("Win32 relocated image copied objects: {}",
                placement.copied_object_count);
    logger.info("Win32 relocated image protected objects: {}",
                placement.protected_object_count);
    if (placement.windows_error != 0)
    {
        logger.info("Win32 relocated image placement error: {}",
                    placement.windows_error);
    }
    logger.info("Win32 relocated image placement message: {}",
                placement.message);
}

void PrintExecutionAttempt(
    spdlog::logger& logger,
    const repiu::platform::win32::Win32MinimalExecutionAttempt& attempt)
{
    logger.info("Win32 minimal execution attempt: {}",
                attempt.valid ? "valid" : "invalid");
    logger.info("Win32 minimal execution supported: {}",
                attempt.supported ? "true" : "false");
    logger.info("Win32 minimal execution attempted: {}",
                attempt.attempted ? "true" : "false");
    logger.info("Win32 minimal execution entry: {}",
                Hex32(attempt.entry_address));
    logger.info("Win32 minimal execution returned: {}",
                attempt.returned ? "true" : "false");
    if (attempt.exception_caught)
    {
        logger.error("Win32 minimal execution exception caught: true");
    }
    else
    {
        logger.info("Win32 minimal execution exception caught: false");
    }
    if (attempt.exception_caught)
    {
        logger.error("Win32 minimal execution exception code: {}",
                     Hex32(attempt.seh_exception_code));
        logger.error("Win32 minimal execution exception address: {}",
                     Hex32(attempt.seh_exception_address));
        logger.error("Win32 minimal execution exception EAX: {}",
                     Hex32(attempt.exception_eax));
        logger.error("Win32 minimal execution exception EBX: {}",
                     Hex32(attempt.exception_ebx));
        logger.error("Win32 minimal execution exception ECX: {}",
                     Hex32(attempt.exception_ecx));
        logger.error("Win32 minimal execution exception EDX: {}",
                     Hex32(attempt.exception_edx));
        logger.error("Win32 minimal execution exception ESI: {}",
                     Hex32(attempt.exception_esi));
        logger.error("Win32 minimal execution exception EDI: {}",
                     Hex32(attempt.exception_edi));
    }
    logger.info("Win32 minimal execution timed out: {}",
                attempt.timed_out ? "true" : "false");
    logger.info("Win32 guest stack switch supported: {}",
                attempt.guest_stack_switch_supported ? "true" : "false");
    logger.info("Win32 guest stack switch attempted: {}",
                attempt.guest_stack_switch_attempted ? "true" : "false");
    if (attempt.guest_stack_switch_attempted)
    {
        logger.info("Win32 guest stack initial ESP: {}",
                    Hex32(attempt.guest_stack_initial_esp));
        if (attempt.guest_stack_return_esp != 0)
        {
            logger.info("Win32 guest stack return ESP: {}",
                        Hex32(attempt.guest_stack_return_esp));
        }
    }
    logger.info("Win32 handled HLE trap count: {}",
                attempt.handled_hle_trap_count);
    if (attempt.handled_hle_trap_count > 0)
    {
        logger.info("Win32 last handled HLE trap address: {}",
                    Hex32(attempt.last_hle_trap_address));
        logger.info("Win32 last handled HLE trap opcode: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_hle_trap_opcode & 0xFFU)));
    }
    logger.info("Win32 handled DOS interrupt count: {}",
                attempt.handled_dos_interrupt_count);
    if (attempt.handled_dos_interrupt_count > 0)
    {
        logger.info("Win32 last handled DOS interrupt vector: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_dos_interrupt_vector & 0xFFU)));
        logger.info("Win32 last handled DOS interrupt AH: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_dos_interrupt_ah & 0xFFU)));
    }
    logger.info("Win32 handled DOS chdir count: {}",
                attempt.handled_dos_chdir_count);
    if (attempt.handled_dos_chdir_count > 0)
    {
        logger.info("Win32 last DOS chdir guest path: {}",
                    attempt.last_dos_chdir_guest_path);
        logger.info("Win32 last DOS chdir virtual path: {}",
                    attempt.last_dos_chdir_virtual_path);
        logger.info("Win32 last DOS chdir host path: {}",
                    attempt.last_dos_chdir_host_path);
        logger.info("Win32 last DOS chdir result: {}",
                    attempt.last_dos_chdir_success ? "success" : "failure");
        if (!attempt.last_dos_chdir_success)
        {
            logger.info("Win32 last DOS chdir error: {}",
                        Hex16(attempt.last_dos_chdir_error));
        }
    }
    logger.info("Win32 handled DOS open count: {}",
                attempt.handled_dos_open_count);
    if (attempt.handled_dos_open_count > 0)
    {
        logger.info("Win32 last DOS open guest path: {}",
                    attempt.last_dos_open_guest_path);
        logger.info("Win32 last DOS open virtual path: {}",
                    attempt.last_dos_open_virtual_path);
        logger.info("Win32 last DOS open host path: {}",
                    attempt.last_dos_open_host_path);
        logger.info("Win32 last DOS open access mode: {}",
                    Hex8(attempt.last_dos_open_access_mode));
        logger.info("Win32 last DOS open result: {}",
                    attempt.last_dos_open_success ? "success" : "failure");
        if (attempt.last_dos_open_success)
        {
            logger.info("Win32 last DOS open handle: {}",
                        Hex16(attempt.last_dos_open_handle));
        }
        else
        {
            logger.info("Win32 last DOS open error: {}",
                        Hex16(attempt.last_dos_open_error));
        }
    }
    logger.info("Win32 handled DOS IOCTL count: {}",
                attempt.handled_dos_ioctl_count);
    if (attempt.handled_dos_ioctl_count > 0)
    {
        logger.info("Win32 last DOS IOCTL subfunction: {}",
                    Hex8(attempt.last_dos_ioctl_subfunction));
        logger.info("Win32 last DOS IOCTL handle: {}",
                    Hex16(attempt.last_dos_ioctl_handle));
        logger.info("Win32 last DOS IOCTL result: {}",
                    attempt.last_dos_ioctl_success ? "success" : "failure");
        if (attempt.last_dos_ioctl_success)
        {
            logger.info("Win32 last DOS IOCTL device info: {}",
                        Hex16(attempt.last_dos_ioctl_device_info));
        }
        else
        {
            logger.info("Win32 last DOS IOCTL error: {}",
                        Hex16(attempt.last_dos_ioctl_error));
        }
    }
    logger.info("Win32 handled DOS resize count: {}",
                attempt.handled_dos_resize_count);
    if (attempt.handled_dos_resize_count > 0)
    {
        logger.info("Win32 last DOS resize selector: {}",
                    Hex16(attempt.last_dos_resize_selector));
        logger.info("Win32 last DOS resize paragraphs: {}",
                    Hex16(attempt.last_dos_resize_paragraphs));
        logger.info("Win32 last DOS resize result: {}",
                    attempt.last_dos_resize_success ? "success" : "failure");
        if (!attempt.last_dos_resize_success)
        {
            logger.info("Win32 last DOS resize error: {}",
                        Hex16(attempt.last_dos_resize_error));
        }
    }
    logger.info("Win32 handled segment load count: {}",
                attempt.handled_segment_load_count);
    if (attempt.handled_segment_load_count > 0)
    {
        logger.info("Win32 last handled segment load address: {}",
                    Hex32(attempt.last_segment_load_address));
        logger.info("Win32 last handled segment load opcode: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_segment_load_opcode & 0xFFU)));
        logger.info("Win32 last handled segment register: {}",
                    SegmentRegisterName(
                        attempt.last_segment_load_register));
        logger.info("Win32 last handled segment selector: {}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.last_segment_load_selector & 0xFFFFU)));
        if (attempt.last_segment_load_source != 0)
        {
            logger.info("Win32 last segment load source: {}",
                        Hex32(attempt.last_segment_load_source));
        }
    }
    logger.info("Win32 handled segment store count: {}",
                attempt.handled_segment_store_count);
    if (attempt.handled_segment_store_count > 0)
    {
        logger.info("Win32 last handled segment store address: {}",
                    Hex32(attempt.last_segment_store_address));
        logger.info("Win32 last handled segment store opcode: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_segment_store_opcode & 0xFFU)));
        logger.info("Win32 last stored segment register: {}",
                    SegmentRegisterName(
                        attempt.last_segment_store_register));
        logger.info("Win32 last stored segment selector: {}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.last_segment_store_selector & 0xFFFFU)));
        logger.info("Win32 last segment store destination: {}",
                    Hex32(attempt.last_segment_store_destination));
    }
    logger.info("Win32 handled segment memory load count: {}",
                attempt.handled_segment_memory_load_count);
    if (attempt.handled_segment_memory_load_count > 0)
    {
        logger.info("Win32 last handled segment memory load address: {}",
                    Hex32(attempt.last_segment_memory_load_address));
        logger.info("Win32 last handled segment memory load opcode: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_segment_memory_load_opcode & 0xFFU)));
        logger.info("Win32 last segment memory load register: {}",
                    SegmentRegisterName(
                        attempt.last_segment_memory_load_register));
        logger.info("Win32 last segment memory load selector: {}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.last_segment_memory_load_selector &
                        0xFFFFU)));
        logger.info("Win32 last segment memory load offset: {}",
                    Hex32(attempt.last_segment_memory_load_offset));
        logger.info("Win32 last segment memory load width: {}",
                    attempt.last_segment_memory_load_width);
        if (attempt.last_segment_memory_load_width == 4)
        {
            logger.info("Win32 last segment memory load value: {}",
                        Hex32(attempt.last_segment_memory_load_value));
        }
        else
        {
            logger.info("Win32 last segment memory load value: {}",
                        Hex8(static_cast<std::uint8_t>(
                            attempt.last_segment_memory_load_value & 0xFFU)));
        }
    }
    logger.info("Win32 minimal execution thread exit code: {}",
                attempt.thread_exit_code);
    if (!attempt.hle_console_output.empty())
    {
        logger.info("Win32 HLE console output bytes: {}",
                    attempt.hle_console_output.size());
        WriteGuestOutput(attempt.hle_console_output);
    }
    if (attempt.exception_caught)
    {
        logger.error("Win32 minimal execution message: {}", attempt.message);
    }
    else
    {
        logger.info("Win32 minimal execution message: {}", attempt.message);
    }
}

bool SelectRelocatedImageBase(std::uint32_t reserve_size,
                              spdlog::logger& logger,
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

        logger.info("Win32 relocated base candidate {}: {}",
                    Hex32(candidate),
                    probe.range_available ? "available" : "occupied");
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

std::optional<repiu::target::TargetProfile> BuildDirectExecutableProfile(
    int argc,
    char** argv)
{
    if (argc < 2)
    {
        return std::nullopt;
    }

    std::filesystem::path executable_path = argv[1];
    if (!std::filesystem::is_regular_file(executable_path))
    {
        return std::nullopt;
    }

    std::filesystem::path working_directory =
        executable_path.parent_path();
    if (working_directory.empty())
    {
        working_directory = ".";
    }

    return repiu::target::TargetProfile{
        "direct_executable",
        "Direct executable",
        executable_path,
        working_directory,
        working_directory,
        repiu::target::ExecutableFormatHint::kDos4gwLe,
        "dos4gw_console_sample",
        repiu::target::TargetRuntimeReservationHint{
            true,
            0x00010000,
            0x00800000,
        },
    };
}

}  // namespace

int main(int argc, char** argv)
{
    std::shared_ptr<spdlog::logger> logger = CreateLoaderLogger();

    const repiu::target::TargetProfile* profile =
        SelectTargetProfile(argc, argv);
    std::optional<repiu::target::TargetProfile> direct_profile;
    if (profile == nullptr)
    {
        direct_profile = BuildDirectExecutableProfile(argc, argv);
        if (!direct_profile.has_value())
        {
            logger->error("Target profile or executable path was not found");
            return 1;
        }
        profile = &direct_profile.value();
    }

    logger->info("Win32 loader target: {}", profile->id);
    logger->info("Win32 loader executable: {}",
                 profile->executable_path.string());
#if defined(REPIU_WIN32_HOST_IMAGE_BASE)
    logger->info("Win32 host image base policy: {}",
                 Hex32(REPIU_WIN32_HOST_IMAGE_BASE));
#endif

    if (!profile->runtime_reservation_hint.valid)
    {
        logger->error("Target has no runtime reservation hint");
        return 1;
    }

    repiu::platform::win32::Win32RuntimeMemoryPolicy policy;
    if (!repiu::platform::win32::BuildWin32RuntimeMemoryPolicyFromFixedRange(
            profile->runtime_reservation_hint.base_address,
            profile->runtime_reservation_hint.reserve_size,
            &policy))
    {
        logger->error("Failed to build fixed range policy");
        return 1;
    }

    PrintPolicy(*logger, policy);

    repiu::platform::win32::Win32AddressRangeProbe probe;
    if (!repiu::platform::win32::ProbeWin32RuntimeAddressRange(
            policy, &probe))
    {
        logger->error("Failed to probe fixed runtime range: {}",
                      probe.message);
        return 1;
    }

    PrintProbe(*logger, probe);

    repiu::platform::win32::Win32AddressRangeReservation reservation;
    if (!repiu::platform::win32::ReserveWin32RuntimeAddressRange(
            policy, &reservation))
    {
        logger->error("Failed to run early reservation attempt");
        return 1;
    }

    PrintReservation(*logger, reservation);
    repiu::platform::win32::ReleaseWin32RuntimeAddressRange(reservation);

    std::vector<std::uint8_t> data;
    std::string read_error;
    if (!ReadBinaryFile(profile->executable_path, &data, &read_error))
    {
        logger->error("Failed to read {}: {}",
                      profile->executable_path.string(),
                      read_error);
        return 1;
    }

    repiu::exe::ParseError error;
    repiu::exe::Dos4gwLoadResult load_result;
    if (!repiu::exe::LoadDos4gwExecutable(data, *profile, &load_result,
                                          &error))
    {
        PrintParseError(*logger, error);
        return 1;
    }

    constexpr std::uint32_t kRuntimeArenaExpansionSlack = 0x00010000;
    repiu::runtime::RuntimeMemoryArenaPlan arena_size_plan;
    if (!repiu::runtime::BuildRuntimeMemoryArenaPlan(
            profile->runtime_reservation_hint.base_address,
            profile->runtime_reservation_hint.reserve_size,
            kRuntimeArenaExpansionSlack,
            &arena_size_plan) ||
        !arena_size_plan.valid)
    {
        PrintRuntimeMemoryArenaPlan(*logger, arena_size_plan);
        logger->error("Failed to build runtime memory arena plan");
        return 1;
    }

    std::uint32_t relocated_image_base = 0;
    if (!SelectRelocatedImageBase(arena_size_plan.arena_reserve_size,
                                  *logger,
                                  &relocated_image_base))
    {
        logger->error("Failed to find an available relocated image base");
        return 1;
    }

    logger->info("Win32 selected relocated image base: {}",
                 Hex32(relocated_image_base));

    repiu::runtime::RuntimeMemoryArenaPlan arena_plan;
    if (!repiu::runtime::BuildRuntimeMemoryArenaPlan(
            relocated_image_base,
            profile->runtime_reservation_hint.reserve_size,
            kRuntimeArenaExpansionSlack,
            &arena_plan) ||
        !arena_plan.valid)
    {
        PrintRuntimeMemoryArenaPlan(*logger, arena_plan);
        logger->error("Failed to build relocated runtime memory arena plan");
        return 1;
    }

    PrintRuntimeMemoryArenaPlan(*logger, arena_plan);

    repiu::runtime::RelocatableRuntimeImagePlan relocatable_plan;
    if (!repiu::runtime::BuildRelocatableRuntimeImagePlan(
            load_result, relocated_image_base, &relocatable_plan, &error))
    {
        PrintParseError(*logger, error);
        return 1;
    }

    repiu::runtime::RelocatedRuntimeImage relocated_image;
    if (!repiu::runtime::BuildRelocatedRuntimeImage(
            load_result, relocatable_plan, &relocated_image, &error))
    {
        PrintParseError(*logger, error);
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
    PrintGuestStackPlan(*logger, stack_plan);

    repiu::hle::HleDispatcherTable hle_dispatcher;
    repiu::hle::BuildInitialHleDispatcherTable(&hle_dispatcher);
    PrintHleDispatcherTable(*logger, hle_dispatcher);

    repiu::hle::DosVirtualFileSystemState dos_file_system;
    if (!repiu::hle::InitializeDosVirtualFileSystem(
            profile->working_directory,
            &dos_file_system) ||
        !dos_file_system.valid)
    {
        PrintDosVirtualFileSystem(*logger, dos_file_system);
        logger->error("Failed to initialize DOS virtual filesystem");
        return 1;
    }
    PrintDosVirtualFileSystem(*logger, dos_file_system);

    repiu::platform::win32::Win32RelocatedImagePlacement placement;
    if (!repiu::platform::win32::PlaceWin32RelocatedImage(
            relocated_image,
            arena_plan.arena_reserve_size,
            &placement))
    {
        logger->error("Failed to place relocated image");
        return 1;
    }

    PrintPlacement(*logger, placement);
    logger->flush();
    repiu::platform::win32::Win32MinimalExecutionAttempt attempt;
    const bool use_dos_console_hle =
        profile->hle_profile_id == "dos4gw_console_sample";
    const bool attempted_execution =
        use_dos_console_hle
            ? repiu::platform::win32::AttemptWin32GuestStackHleExecution(
                  placement,
                  stack_plan,
                  dos_file_system,
                  1000,
                  &attempt)
            : repiu::platform::win32::AttemptWin32GuestStackTrapExecution(
                  placement,
                  stack_plan,
                  dos_file_system,
                  1000,
                  &attempt);
    if (!attempted_execution)
    {
        logger->error("Failed to attempt minimal original entry execution");
        repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
        return 1;
    }

    PrintExecutionAttempt(*logger, attempt);
    if (attempt.exception_caught)
    {
        repiu::runtime::RelocatedImageByteWindow window;
        repiu::runtime::BuildRelocatedImageByteWindow(
            relocated_image,
            attempt.seh_exception_address,
            16,
            16,
            &window);
        PrintByteWindow(*logger, window);
        if (window.valid)
        {
            repiu::hle::PrivilegedInstructionClassification classification;
            repiu::hle::ClassifyPrivilegedInstruction(
                window.bytes,
                window.focus_offset,
                &classification);
            PrintPrivilegedInstructionClassification(*logger, classification);
        }
    }
    repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
    return 0;
}
