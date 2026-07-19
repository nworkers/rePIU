#include "repiu/exe/dos4gw_loader.h"
#include "repiu/assets/pumpit1_mount.h"
#include "repiu/exe/dos16m_bound_module.h"
#include "repiu/hle/dos_file_system.h"
#include "repiu/hle/hle_dispatcher.h"
#include "repiu/hle/privileged_instruction.h"
#include "repiu/platform/win32/execution_trampoline.h"
#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/platform/win32/live_telemetry.h"
#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/guest_context.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/image_address.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/runtime/runtime_memory_arena.h"
#include "repiu/runtime/selector_table.h"
#include "repiu/target/target_profile.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
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

constexpr std::uint32_t kDefaultExecutionTimeoutMilliseconds = 1000U;

bool UseAotExecutionBackend()
{
    const char* value = std::getenv("REPIU_EXECUTION_BACKEND");
    return value != nullptr &&
        (std::string_view(value) == "aot" ||
         std::string_view(value) == "aot-dynamic");
}

bool UseDynamicAotTranslation()
{
    const char* value = std::getenv("REPIU_EXECUTION_BACKEND");
    return value != nullptr && std::string_view(value) == "aot-dynamic";
}

std::uint32_t ReadExecutionTimeoutMilliseconds()
{
    const char* text = std::getenv(
        repiu::platform::win32::kWin32ExecutionTimeoutEnvironment);
    if (text == nullptr || *text == '\0')
    {
        return kDefaultExecutionTimeoutMilliseconds;
    }

    std::uint32_t value = 0;
    const char* end = text;
    while (*end != '\0')
    {
        ++end;
    }
    const auto result = std::from_chars(text, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return kDefaultExecutionTimeoutMilliseconds;
    }
    return value == 0 ? INFINITE : value;
}

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

std::string HexBytes(const std::uint8_t* bytes, std::size_t byte_count)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < byte_count; ++index)
    {
        if (index != 0U)
        {
            stream << ' ';
        }
        stream << std::uppercase << std::hex << std::setw(2)
               << std::setfill('0')
               << static_cast<unsigned>(bytes[index]);
    }
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

void LogGuestOutput(spdlog::logger& logger,
                    spdlog::level::level_enum level,
                    std::string_view output)
{
    std::size_t begin = 0;
    while (begin < output.size())
    {
        const std::size_t newline = output.find('\n', begin);
        const std::size_t end = newline == std::string_view::npos
            ? output.size()
            : newline;
        std::size_t content_end = end;
        if (content_end > begin && output[content_end - 1U] == '\r')
        {
            --content_end;
        }
        logger.log(level, "{}", output.substr(begin, content_end - begin));
        if (newline == std::string_view::npos)
        {
            break;
        }
        begin = newline + 1U;
    }
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
    const bool breakpoint_trap =
        classification.instruction_class ==
        repiu::hle::PrivilegedInstructionClass::kGuestBreakpointTrap;
    if (!classification.valid)
    {
        logger.error("Privileged instruction classification: unknown");
        logger.error("Privileged instruction opcode: {}",
                     Hex8(classification.opcode));
    }
    else if (breakpoint_trap)
    {
        logger.error("Privileged instruction classification: breakpoint");
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
        spdlog::logger& output_logger = logger;
        const spdlog::level::level_enum level =
            breakpoint_trap ? spdlog::level::err : spdlog::level::info;
        output_logger.log(level,
                          "Privileged instruction mnemonic: {}",
                          classification.mnemonic);
        output_logger.log(level,
                          "Privileged instruction length: {}",
                          classification.length);
        output_logger.log(level,
                          "Privileged instruction class: {}",
                          repiu::hle::PrivilegedInstructionClassName(
                              classification.instruction_class));
        output_logger.log(level,
                          "Privileged instruction HLE trap candidate: {}",
                          classification.hle_trap_candidate ? "true"
                                                            : "false");
        output_logger.log(
            level,
            "Privileged instruction CPU/DPMI state candidate: {}",
            classification.cpu_state_initialization_candidate ? "true"
                                                             : "false");
    }
    if (!classification.valid)
    {
        logger.error("Privileged instruction classification message: {}",
                     classification.message);
        logger.error("Current execution blocker: unhandled or unclassified "
                     "instruction/memory access at exception point");
    }
    else if (breakpoint_trap)
    {
        logger.error("Privileged instruction classification message: {}",
                     classification.message);
        logger.error("Current execution blocker: guest breakpoint trap");
    }
    else if (classification.hle_trap_candidate)
    {
        logger.error("Privileged instruction classification message: {}",
                     classification.message);
        logger.error("Current execution blocker: unhandled HLE trap candidate");
    }
    else
    {
        logger.info("Privileged instruction classification message: {}",
                    classification.message);
    }
}

void PrintX86ExecutionSnapshot(spdlog::logger& logger,
                               std::string_view label,
                               const repiu::platform::win32::
                                   X86ExecutionSnapshot& snapshot)
{
    logger.error("{} context captured: {}",
                 label,
                 snapshot.captured ? "true" : "false");
    if (!snapshot.captured)
    {
        return;
    }

    logger.error("{} EIP: {}", label, Hex32(snapshot.eip));
    logger.error("{} EAX: {}", label, Hex32(snapshot.eax));
    logger.error("{} EBX: {}", label, Hex32(snapshot.ebx));
    logger.error("{} ECX: {}", label, Hex32(snapshot.ecx));
    logger.error("{} EDX: {}", label, Hex32(snapshot.edx));
    logger.error("{} ESI: {}", label, Hex32(snapshot.esi));
    logger.error("{} EDI: {}", label, Hex32(snapshot.edi));
    logger.error("{} ESP: {}", label, Hex32(snapshot.esp));
    logger.error("{} EBP: {}", label, Hex32(snapshot.ebp));
    logger.error("{} EFLAGS: {}", label, Hex32(snapshot.eflags));
    logger.error("{} CS: {}", label, Hex16(snapshot.cs));
    logger.error("{} DS: {}", label, Hex16(snapshot.ds));
    logger.error("{} ES: {}", label, Hex16(snapshot.es));
    logger.error("{} SS: {}", label, Hex16(snapshot.ss));
    logger.error("{} FS: {}", label, Hex16(snapshot.fs));
    logger.error("{} GS: {}", label, Hex16(snapshot.gs));
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
    const std::string current_directory =
        repiu::hle::GetDosCurrentDirectory(state);
    logger.info("DOS virtual filesystem current directory: {}",
                current_directory.empty()
                    ? "\\"
                    : "\\" + current_directory);
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
    logger.info("Win32 relocated selector binding count: {}",
                placement.selector_bindings.size());
    for (const repiu::runtime::RelocatedSelectorBinding& binding :
         placement.selector_bindings)
    {
        logger.info(
            "Win32 relocated selector binding: selector={} object={} base={} limit={}",
            Hex16(binding.selector),
            binding.target_object,
            Hex32(binding.relocated_base_address),
            Hex32(binding.limit));
    }
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
    const repiu::platform::win32::Win32MinimalExecutionAttempt& attempt,
    std::string_view executable_name)
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
        logger.error("Win32 minimal execution exception access/fault VA: {}/{}",
                     attempt.exception_access_kind,
                     Hex32(attempt.exception_fault_va));
        logger.error(
            "Win32 exception fault page base/alloc/state/protect/size: "
            "{}/{}/{}/{}/{}",
            Hex32(attempt.exception_fault_region_base),
            Hex32(attempt.exception_fault_alloc_base),
            Hex32(attempt.exception_fault_state),
            Hex32(attempt.exception_fault_protect),
            Hex32(attempt.exception_fault_region_size));
        logger.error(
            "Win32 exception ESI structure +0x20..+0x3C (mask {}): "
            "{} {} {} {} {} {} {} {}",
            Hex32(attempt.exception_esi_dword_valid_mask),
            Hex32(attempt.exception_esi_dwords[0]),
            Hex32(attempt.exception_esi_dwords[1]),
            Hex32(attempt.exception_esi_dwords[2]),
            Hex32(attempt.exception_esi_dwords[3]),
            Hex32(attempt.exception_esi_dwords[4]),
            Hex32(attempt.exception_esi_dwords[5]),
            Hex32(attempt.exception_esi_dwords[6]),
            Hex32(attempt.exception_esi_dwords[7]));
        {
            static const char* const kRegisterNames[6] = {
                "EAX", "EBX", "ECX", "EDX", "ESI", "EDI"};
            for (std::uint32_t reg = 0; reg < 6U; ++reg)
            {
                if ((attempt.exception_register_string_valid_mask &
                     (1U << reg)) == 0)
                {
                    continue;
                }
                const std::uint8_t* bytes =
                    attempt.exception_register_strings[reg];
                std::string ascii;
                std::string hex;
                for (std::uint32_t index = 0;
                     index < sizeof(attempt.exception_register_strings[reg]);
                     ++index)
                {
                    const std::uint8_t value = bytes[index];
                    if (!hex.empty())
                    {
                        hex += ' ';
                    }
                    hex += Hex8(value);
                    ascii += (value >= 0x20 && value < 0x7F)
                                 ? static_cast<char>(value)
                                 : '.';
                }
                logger.error(
                    "Win32 exception register string {}: \"{}\" [{}]",
                    kRegisterNames[reg], ascii, hex);
            }
        }
        logger.error("Win32 exception stack window base/count: {}/{}",
                     Hex32(attempt.exception_stack_base),
                     attempt.exception_stack_dword_count);
        for (std::uint32_t row = 0;
             row < attempt.exception_stack_dword_count; row += 4U)
        {
            std::string line;
            const std::uint32_t row_end = std::min<std::uint32_t>(
                row + 4U, attempt.exception_stack_dword_count);
            for (std::uint32_t index = row; index < row_end; ++index)
            {
                line += ' ';
                line += Hex32(attempt.exception_stack_dwords[index]);
            }
            logger.error("Win32 exception stack +{}:{}",
                         Hex32(row * 4U), line);
        }
        if (attempt.aot_probe_guest_address != 0)
        {
            logger.error(
                "Win32 AOT runtime cache probe guest/cache/valid: {}/{}/{}",
                Hex32(attempt.aot_probe_guest_address),
                Hex32(attempt.aot_probe_cache_address),
                attempt.aot_probe_cache_valid);
            for (std::uint32_t base = 0;
                 base < sizeof(attempt.aot_probe_cache_bytes); base += 16U)
            {
                std::string probe_line;
                char byte_hex[4];
                const std::uint32_t end = std::min<std::uint32_t>(
                    base + 16U, sizeof(attempt.aot_probe_cache_bytes));
                for (std::uint32_t index = base; index < end; ++index)
                {
                    std::snprintf(byte_hex, sizeof(byte_hex), " %02X",
                                  attempt.aot_probe_cache_bytes[index]);
                    probe_line += byte_hex;
                }
                logger.error("Win32 AOT runtime cache probe +{}:{}",
                             Hex32(base), probe_line);
            }
        }
        PrintX86ExecutionSnapshot(logger,
                                  "Win32 minimal execution exception",
                                  attempt.exception_snapshot);
    }
    logger.info("Win32 handled original fatal breakpoint count: {}",
                attempt.handled_fatal_breakpoint_count);
    if (attempt.handled_fatal_breakpoint_count != 0)
    {
        logger.error("Win32 last original fatal breakpoint address: {}",
                     Hex32(attempt.last_fatal_breakpoint_address));
        logger.error("Win32 last original fatal message address: {}",
                     Hex32(attempt.last_fatal_message_address));
        logger.error("Win32 last original fatal message: {}",
                     attempt.last_fatal_message);
    }
    logger.info("Win32 original fatal halt reached: {}",
                attempt.fatal_halt_reached ? "true" : "false");
    logger.info("Win32 minimal execution timed out: {}",
                attempt.timed_out ? "true" : "false");
    if (attempt.timed_out)
    {
        const auto& snapshot = attempt.timeout_snapshot;
        logger.info("Win32 minimal execution timeout context captured: {}",
                    snapshot.captured ? "true" : "false");
        if (snapshot.captured)
        {
            logger.info("Win32 minimal execution timeout EIP: {}",
                        Hex32(snapshot.eip));
            logger.info("Win32 minimal execution timeout EAX: {}",
                        Hex32(snapshot.eax));
            logger.info("Win32 minimal execution timeout EBX: {}",
                        Hex32(snapshot.ebx));
            logger.info("Win32 minimal execution timeout ECX: {}",
                        Hex32(snapshot.ecx));
            logger.info("Win32 minimal execution timeout EDX: {}",
                        Hex32(snapshot.edx));
            logger.info("Win32 minimal execution timeout ESI: {}",
                        Hex32(snapshot.esi));
            logger.info("Win32 minimal execution timeout EDI: {}",
                        Hex32(snapshot.edi));
            logger.info("Win32 minimal execution timeout ESP: {}",
                        Hex32(snapshot.esp));
            logger.info("Win32 minimal execution timeout EBP: {}",
                        Hex32(snapshot.ebp));
            logger.info("Win32 minimal execution timeout EFLAGS: {}",
                        Hex32(snapshot.eflags));
            logger.info("Win32 minimal execution timeout CS: {}",
                        Hex16(snapshot.cs));
            logger.info("Win32 minimal execution timeout DS: {}",
                        Hex16(snapshot.ds));
            logger.info("Win32 minimal execution timeout ES: {}",
                        Hex16(snapshot.es));
            logger.info("Win32 minimal execution timeout SS: {}",
                        Hex16(snapshot.ss));
            logger.info("Win32 minimal execution timeout FS: {}",
                        Hex16(snapshot.fs));
            logger.info("Win32 minimal execution timeout GS: {}",
                        Hex16(snapshot.gs));
        }
    }
    logger.info("Win32 single-step trace count: {}",
                attempt.single_step_trace_count);
    logger.info("Win32 native fast path entry/return/cancel: {}/{}/{}",
                attempt.native_fast_path_entry_count,
                attempt.native_fast_path_return_count,
                attempt.native_fast_path_cancel_count);
    logger.info("Win32 native fast path last entry/return: {}/{}",
                Hex32(attempt.native_fast_path_last_entry),
                Hex32(attempt.native_fast_path_last_return));
    logger.info("Win32 execution backend: {}",
                attempt.aot_backend_active ? "aot" : "legacy");
    logger.info("Win32 AOT entry/boundary/reentry/fallback: {}/{}/{}/{}",
                attempt.aot_cache_entry_count,
                attempt.aot_boundary_count,
                attempt.aot_reentry_count,
                attempt.aot_legacy_fallback_count);
    logger.info("Win32 AOT last fallback address: {}",
                Hex32(attempt.aot_last_fallback_address));
    logger.info("Win32 AOT dynamic attempt/success/bytes: {}/{}/{}",
                attempt.aot_dynamic_attempt_count,
                attempt.aot_dynamic_success_count,
                attempt.aot_dynamic_added_bytes);
    logger.info("Win32 AOT indirect dispatch/source/target: {}/{}/{}",
                attempt.aot_indirect_dispatch_count,
                Hex32(attempt.aot_last_indirect_source),
                Hex32(attempt.aot_last_indirect_target));
    logger.info("Win32 AOT inline-cache patch attempt/success: {}/{}",
                attempt.aot_inline_cache_patch_attempt_count,
                attempt.aot_inline_cache_patch_success_count);
    logger.info("Win32 AOT inline-cache sites/last cache boundary: {}/{}",
                attempt.aot_inline_cache_site_count,
                Hex32(attempt.aot_last_reentry_cache_address));
    logger.info("Win32 AOT code writes/retire attempt/success: {}/{}/{}",
                attempt.aot_code_write_count,
                attempt.aot_page_retire_attempt_count,
                attempt.aot_page_retire_success_count);
    logger.info("Win32 AOT generation publishes/quarantines: {}/{}",
                attempt.aot_generation_publish_count,
                attempt.aot_quarantine_count);
    logger.info("Win32 AOT generation failures/relinked/retired traps: {}/{}/{}",
                attempt.aot_generation_failure_count,
                attempt.aot_generation_relinked_entry_count,
                attempt.aot_retired_entry_trap_count);
    logger.info("Win32 AOT last code write source/destination: {}/{}",
                Hex32(attempt.aot_last_code_write_source),
                Hex32(attempt.aot_last_code_write_destination));
    logger.info("Win32 AOT last retired page/published generation: {}/{}",
                Hex32(attempt.aot_last_retired_page),
                attempt.aot_last_published_generation);
    logger.info("Win32 AOT exception cache/guest mapping: {}/{}/{}",
                attempt.aot_exception_mapping_valid ? "valid" : "invalid",
                Hex32(attempt.aot_exception_cache_address),
                Hex32(attempt.aot_exception_guest_address));
    if (attempt.aot_exception_mapping_valid)
    {
        logger.info("Win32 AOT exception cache bytes: {}",
                    HexBytes(attempt.aot_exception_cache_bytes,
                             sizeof(attempt.aot_exception_cache_bytes)));
        logger.info("Win32 AOT exception guest bytes: {}",
                    HexBytes(attempt.aot_exception_guest_bytes,
                             sizeof(attempt.aot_exception_guest_bytes)));
    }
    logger.info("Win32 AOT return dispatch/source/target: {}/{}/{}",
                attempt.aot_return_dispatch_count,
                Hex32(attempt.aot_last_return_source),
                Hex32(attempt.aot_last_return_target));
    logger.info("Win32 AOT return stack: {} {} {} {}",
                Hex32(attempt.aot_last_return_stack[0]),
                Hex32(attempt.aot_last_return_stack[1]),
                Hex32(attempt.aot_last_return_stack[2]),
                Hex32(attempt.aot_last_return_stack[3]));
    logger.info("Win32 AOT call depth/return match/expected: {}/{}/{}",
                attempt.aot_call_depth,
                attempt.aot_last_return_matches_call ? "true" : "false",
                Hex32(attempt.aot_last_expected_return));
    logger.info("Win32 AOT last call source/target: {}/{}",
                Hex32(attempt.aot_last_call_source),
                Hex32(attempt.aot_last_call_target));
    logger.info("Win32 AOT expected call source/target: {}/{}",
                Hex32(attempt.aot_last_expected_call_source),
                Hex32(attempt.aot_last_expected_call_target));
    logger.info("Win32 AOT return trace entries: {}",
                attempt.aot_return_trace_count);
    const std::uint32_t trace_begin = attempt.aot_return_trace_count >
        repiu::platform::win32::kWin32AotReturnTraceCapacity
        ? attempt.aot_return_trace_count -
              repiu::platform::win32::kWin32AotReturnTraceCapacity
        : 0U;
    for (std::uint32_t sequence = trace_begin;
         sequence < attempt.aot_return_trace_count; ++sequence)
    {
        const auto& trace = attempt.aot_return_trace[
            sequence % repiu::platform::win32::kWin32AotReturnTraceCapacity];
        logger.info("Win32 AOT return trace #{} source/actual/expected/ESP/match: {}/{}/{}/{}/{}",
                    sequence + 1U, Hex32(trace.source),
                    Hex32(trace.actual_target), Hex32(trace.expected_target),
                    Hex32(trace.esp), trace.matches ? "true" : "false");
    }
    logger.info("Win32 AOT transfer trace entries: {}",
                attempt.aot_transfer_trace_count);
    const std::uint32_t transfer_begin = attempt.aot_transfer_trace_count >
        repiu::platform::win32::kWin32AotTransferTraceCapacity
        ? attempt.aot_transfer_trace_count -
              repiu::platform::win32::kWin32AotTransferTraceCapacity
        : 0U;
    for (std::uint32_t sequence = transfer_begin;
         sequence < attempt.aot_transfer_trace_count; ++sequence)
    {
        const auto& transfer = attempt.aot_transfer_trace[
            sequence % repiu::platform::win32::kWin32AotTransferTraceCapacity];
        logger.info("Win32 AOT transfer trace #{} source/target/kind: {}/{}/{}",
                    sequence + 1U, Hex32(transfer.source),
                    Hex32(transfer.target),
                    transfer.is_call ? "call" : "jump");
    }
    logger.info("Win32 execution probe configured/hit/offset: {}/{}/{}",
                attempt.execution_probe_configured ? "true" : "false",
                attempt.execution_probe_hit ? "true" : "false",
                Hex32(attempt.execution_probe_offset));
    if (attempt.execution_probe_hit)
    {
        const auto& probe = attempt.execution_probe_snapshot;
        logger.info("Win32 execution probe EIP/ESP/EFLAGS: {}/{}/{}",
                    Hex32(probe.eip), Hex32(probe.esp),
                    Hex32(probe.eflags));
        logger.info("Win32 execution probe EAX/EBX/ECX/EDX: {}/{}/{}/{}",
                    Hex32(probe.eax), Hex32(probe.ebx),
                    Hex32(probe.ecx), Hex32(probe.edx));
        logger.info("Win32 execution probe ESI/EDI/EBP: {}/{}/{}",
                    Hex32(probe.esi), Hex32(probe.edi), Hex32(probe.ebp));
        logger.info("Win32 execution probe stack: {} {} {} {} {} {} {} {}",
                    Hex32(attempt.execution_probe_stack[0]),
                    Hex32(attempt.execution_probe_stack[1]),
                    Hex32(attempt.execution_probe_stack[2]),
                    Hex32(attempt.execution_probe_stack[3]),
                    Hex32(attempt.execution_probe_stack[4]),
                    Hex32(attempt.execution_probe_stack[5]),
                    Hex32(attempt.execution_probe_stack[6]),
                    Hex32(attempt.execution_probe_stack[7]));
    }
    if (attempt.execution_trace_configured)
    {
        logger.info(
            "Win32 execution trace start/end/esp_offset/hits: "
            "{}/{}/{}/{}",
            Hex32(attempt.execution_trace_start_offset),
            Hex32(attempt.execution_trace_end_offset),
            Hex32(attempt.execution_trace_esp_offset),
            attempt.execution_trace_hit_count);
        if (attempt.execution_trace_sentinel2_configured)
        {
            logger.info(
                "Win32 execution trace sentinel2_offset/rearm_count: {}/{}",
                Hex32(attempt.execution_trace_sentinel2_offset),
                attempt.execution_trace_sentinel_rearm_count);
        }
        const std::uint32_t stored = std::min<std::uint32_t>(
            attempt.execution_trace_hit_count,
            static_cast<std::uint32_t>(
                sizeof(attempt.execution_trace) /
                sizeof(attempt.execution_trace[0])));
        for (std::uint32_t index = 0; index < stored; ++index)
        {
            const auto& entry = attempt.execution_trace[index];
            logger.info(
                "Win32 execution trace #{} seq={} eip/esp/value: {}/{}/{}",
                index, entry.sequence, Hex32(entry.eip), Hex32(entry.esp),
                Hex32(entry.value_at_esp_offset));
        }
    }
    logger.info("Win32 diagnostic poll iterations: {}",
                attempt.diagnostic_poll_iteration_count);
    logger.info("Win32 diagnostic progress count: {}",
                attempt.diagnostic_progress_count);
    logger.info("Win32 diagnostic quiet iterations: {}",
                attempt.diagnostic_quiet_iteration_count);
    logger.info("Win32 exception dispatch entry count: {}",
                attempt.exception_dispatch_entry_count);
    logger.info("Win32 exception dispatch exit count: {}",
                attempt.exception_dispatch_exit_count);
    const std::uint32_t outstanding_dispatch_count =
        attempt.exception_dispatch_entry_count >=
                attempt.exception_dispatch_exit_count
            ? attempt.exception_dispatch_entry_count -
                  attempt.exception_dispatch_exit_count
            : 0;
    logger.info("Win32 exception dispatch outstanding count: {}",
                outstanding_dispatch_count);
    logger.info("Win32 exception dispatch last EIP: {}",
                Hex32(attempt.exception_dispatch_last_eip));
    logger.info("Win32 selector table valid: {}",
                attempt.selector_table_valid ? "true" : "false");
    logger.info("Win32 selector descriptor count: {}",
                attempt.selector_descriptor_count);
    logger.info("Win32 LINEXE environment active: {}",
                attempt.linexe_environment_active ? "true" : "false");
    logger.info("Win32 LINEXE saved client GS: {}",
                Hex32(attempt.linexe_saved_client_gs));
    logger.info("Win32 LINEXE client descriptor: {} base={} limit={}",
                attempt.linexe_client_descriptor_valid ? "valid" : "invalid",
                Hex32(attempt.linexe_client_descriptor_base),
                Hex32(attempt.linexe_client_descriptor_limit));
    logger.info("Win32 LINEXE direct root: {}:{}",
                Hex32(attempt.linexe_root_selector),
                Hex32(attempt.linexe_root_offset));
    logger.info("Win32 LINEXE data descriptor: {} base={}",
                attempt.linexe_data_descriptor_valid ? "valid" : "invalid",
                Hex32(attempt.linexe_data_descriptor_base));
    logger.info("Win32 LINEXE module name pointer: {}:{}",
                Hex32(attempt.linexe_module_name_selector),
                Hex32(attempt.linexe_module_name_offset));
    logger.info("Win32 LINEXE direct module name: {}",
                attempt.linexe_direct_module_name);
    logger.info("Win32 LINEXE direct exports: count={} table={}:{}",
                attempt.linexe_direct_export_count,
                Hex32(attempt.linexe_direct_export_table_selector),
                Hex32(attempt.linexe_direct_export_table_offset));
    logger.info("Win32 LINEXE direct first export name: {}:{}",
                Hex32(attempt.linexe_direct_first_export_name_selector),
                Hex32(attempt.linexe_direct_first_export_name_offset));
    logger.info("Win32 LINEXE GS byte loads: {} first_offset={} first_value={}",
                attempt.linexe_gs_byte_load_count,
                Hex32(attempt.linexe_first_gs_byte_offset),
                Hex32(attempt.linexe_first_gs_byte_value));
    logger.info("Win32 LINEXE resolved export count: {}",
                attempt.linexe_resolved_export_count);
    logger.info("Win32 LINEXE scan entry/match/return: {}/{}/{}",
                attempt.linexe_scan_entry_count,
                attempt.linexe_export_match_count,
                attempt.linexe_scan_return_count);
    logger.info("Win32 LINEXE indirect far call count/source/pointer/target: {}/{}/{}/{}:{} ({})",
                attempt.linexe_indirect_far_call_count,
                Hex32(attempt.linexe_indirect_far_call_source),
                Hex32(attempt.linexe_indirect_far_call_pointer),
                Hex32(attempt.linexe_indirect_far_call_selector),
                Hex32(attempt.linexe_indirect_far_call_offset),
                attempt.linexe_indirect_far_call_known_export ? "known export" : "unknown");
    logger.info("Win32 INT 8 chain HLE count/source/pointer/target: {}/{}/{}/{}:{}",
                attempt.timer_interrupt_chain_hle_count,
                Hex32(attempt.timer_interrupt_chain_hle_source),
                Hex32(attempt.timer_interrupt_chain_hle_pointer),
                Hex32(attempt.timer_interrupt_chain_hle_selector),
                Hex32(attempt.timer_interrupt_chain_hle_offset));
    logger.info("Win32 LINEXE export entry loop count: {}",
                attempt.linexe_export_entry_loop_count);
    logger.info("Win32 LINEXE export compare count/EAX/ECX/EFLAGS: {}/{}/{}/{}",
                attempt.linexe_export_compare_count,
                Hex32(attempt.linexe_export_compare_eax),
                Hex32(attempt.linexe_export_compare_ecx),
                Hex32(attempt.linexe_export_compare_eflags));
    logger.info("Win32 LINEXE export count load EDX/GS: {}/{}",
                Hex32(attempt.linexe_export_count_load_edx),
                Hex32(attempt.linexe_export_count_load_gs));
    logger.info("Win32 LINEXE module candidate/match: {}/{}",
                attempt.linexe_module_candidate_count,
                attempt.linexe_module_match_count);
    logger.info("Win32 LINEXE name pointer/byte instruction: {}/{}",
                attempt.linexe_name_pointer_valid_count,
                attempt.linexe_name_byte_instruction_count);
    logger.info("Win32 LINEXE GS=0090h load count: {}",
                attempt.linexe_data_gs_load_count);
    logger.info("Win32 LINEXE module selector stack value: {}",
                Hex32(attempt.linexe_module_selector_stack_value));
    logger.info("Win32 LINEXE module/export stack pointer: {}:{}/{}:{}",
                Hex32(attempt.linexe_module_selector_stack_value),
                Hex32(attempt.linexe_module_offset_stack_value),
                Hex32(attempt.linexe_export_selector_stack_value),
                Hex32(attempt.linexe_export_offset_stack_value));
    logger.info("Win32 LINEXE export jump source ESP/module: {}/{}:{}",
                Hex32(attempt.linexe_export_jump_source_esp),
                Hex32(attempt.linexe_export_jump_source_module_selector),
                Hex32(attempt.linexe_export_jump_source_module_offset));
    logger.info("Win32 LINEXE export jump target ESP/module: {}/{}:{}",
                Hex32(attempt.linexe_export_jump_target_esp),
                Hex32(attempt.linexe_export_jump_target_module_selector),
                Hex32(attempt.linexe_export_jump_target_module_offset));
    logger.info("Win32 LINEXE export name compare count/GS/EDI/ESI/bytes: {}/{}/{}/{}/{}/{}",
                attempt.linexe_export_name_compare_count,
                Hex32(attempt.linexe_export_name_compare_gs),
                Hex32(attempt.linexe_export_name_compare_edi),
                Hex32(attempt.linexe_export_name_compare_esi),
                Hex32(attempt.linexe_export_name_actual_byte),
                Hex32(attempt.linexe_export_name_expected_byte));
    logger.info("Win32 LINEXE export name stage mask: {}",
                Hex32(attempt.linexe_export_name_stage_mask));
    logger.info("Win32 LINEXE export entry name loaded offset/selector: {}/{}",
                Hex32(attempt.linexe_export_entry_name_offset_value),
                Hex32(attempt.linexe_export_entry_name_selector_value));
    logger.info("Win32 LINEXE export result stores count/destination/value: {}/{}/{}",
                attempt.linexe_export_result_store_count,
                Hex32(attempt.linexe_export_result_store_destination),
                Hex32(attempt.linexe_export_result_store_value));
    logger.info("Win32 LINEXE export value load selector/offset/value: {}/{}/{}",
                Hex32(attempt.linexe_export_value_load_selector),
                Hex32(attempt.linexe_export_value_load_offset),
                Hex32(attempt.linexe_export_value_load_value));
    logger.info("Win32 LINEXE bridge entry/gate/target/service: {}/{}/{}:{}/{}",
                attempt.linexe_bridge_entry_count,
                attempt.linexe_bridge_gate_valid ? "valid" : "invalid",
                Hex32(attempt.linexe_bridge_selector),
                Hex32(attempt.linexe_bridge_offset),
                attempt.linexe_bridge_service);
    logger.info("Win32 LINEXE bridge ESP/EBP: {}/{}",
                Hex32(attempt.linexe_bridge_esp),
                Hex32(attempt.linexe_bridge_ebp));
    logger.info("Win32 LINEXE bridge stack: {} {} {} {} {} {}",
                Hex32(attempt.linexe_bridge_stack[0]),
                Hex32(attempt.linexe_bridge_stack[1]),
                Hex32(attempt.linexe_bridge_stack[2]),
                Hex32(attempt.linexe_bridge_stack[3]),
                Hex32(attempt.linexe_bridge_stack[4]),
                Hex32(attempt.linexe_bridge_stack[5]));
    logger.info("Win32 LINEXE bridge stack continued: {} {} {} {} {} {}",
                Hex32(attempt.linexe_bridge_stack[6]),
                Hex32(attempt.linexe_bridge_stack[7]),
                Hex32(attempt.linexe_bridge_stack[8]),
                Hex32(attempt.linexe_bridge_stack[9]),
                Hex32(attempt.linexe_bridge_stack[10]),
                Hex32(attempt.linexe_bridge_stack[11]));
    logger.info("Win32 LINEXE bridge stack tail: {} {} {} {} {} {} {} {}",
                Hex32(attempt.linexe_bridge_stack[12]),
                Hex32(attempt.linexe_bridge_stack[13]),
                Hex32(attempt.linexe_bridge_stack[14]),
                Hex32(attempt.linexe_bridge_stack[15]),
                Hex32(attempt.linexe_bridge_stack[16]),
                Hex32(attempt.linexe_bridge_stack[17]),
                Hex32(attempt.linexe_bridge_stack[18]),
                Hex32(attempt.linexe_bridge_stack[19]));
    for (std::size_t index = 0;
         index < std::size(attempt.linexe_bridge_stack_text);
         ++index)
    {
        if (attempt.linexe_bridge_stack_text[index][0] != '\0')
        {
            logger.info("Win32 LINEXE bridge stack text #{}: {}",
                        index,
                        attempt.linexe_bridge_stack_text[index]);
        }
    }
    logger.info("Win32 LINEXE bridge argument text: {}",
                attempt.linexe_bridge_argument_text);
    logger.info("Win32 LINEXE virtual module loads/handle: {}/{}",
                attempt.linexe_virtual_module_load_count,
                Hex32(attempt.linexe_virtual_module_handle));
    logger.info("Win32 LINEXE get-proc count/name/result: {}/{}/{}",
                attempt.linexe_get_proc_count,
                attempt.linexe_get_proc_name,
                Hex32(attempt.linexe_get_proc_result_pointer));
    logger.info("Win32 Glide gate entries/handled/ESP: {}/{}/{}",
                attempt.glide_gate_entry_count,
                attempt.glide_gate_handled_count,
                Hex32(attempt.glide_gate_esp));
    logger.info("Win32 Glide gate ordinal/name/argument bytes: {}/{}/{}",
                attempt.glide_gate_ordinal,
                attempt.glide_gate_name,
                attempt.glide_gate_argument_bytes);
    logger.info("Win32 Glide window opens/logical size: {}/{}x{}",
                attempt.glide_window_open_count,
                attempt.glide_logical_width,
                attempt.glide_logical_height);
    logger.info("Win32 Glide backend message: {}",
                attempt.glide_backend_message);
    logger.info("Win32 Glide virtual texture bytes/max address: {}/{}",
                attempt.glide_texture_memory_bytes,
                Hex32(attempt.glide_texture_max_address));
    logger.info("Win32 Glide gate stack: {} {} {} {} {} {} {} {}",
                Hex32(attempt.glide_gate_stack[0]),
                Hex32(attempt.glide_gate_stack[1]),
                Hex32(attempt.glide_gate_stack[2]),
                Hex32(attempt.glide_gate_stack[3]),
                Hex32(attempt.glide_gate_stack[4]),
                Hex32(attempt.glide_gate_stack[5]),
                Hex32(attempt.glide_gate_stack[6]),
                Hex32(attempt.glide_gate_stack[7]));
    logger.info("Win32 Glide texture gate trace count/wrapped: {}/{}",
                attempt.glide_texture_gate_trace_count,
                attempt.glide_texture_gate_trace_wrapped ? "true" : "false");
    for (const auto& entry : attempt.glide_texture_gate_trace)
    {
        if (!entry.valid)
        {
            continue;
        }
        logger.info("Win32 Glide texture gate trace #{} kind={} ordinal={} entry-eip={} entry-esp={} return-address={} tmu={} entry-eax={} return-eax={} planned-return-esp={}",
                    entry.sequence,
                    entry.is_max_address ? "max" : "min",
                    entry.ordinal,
                    Hex32(entry.entry_eip),
                    Hex32(entry.entry_esp),
                    Hex32(entry.return_address),
                    entry.tmu,
                    Hex32(entry.entry_eax),
                    Hex32(entry.return_eax),
                    Hex32(entry.planned_return_esp));
    }
    for (const auto& call : attempt.glide_calls)    {
        logger.info("Win32 Glide call trace: ordinal={} name={} count={} first_stack={} {} {} {} {} {} {} {}",
                    call.ordinal,
                    call.name,
                    call.count,
                    Hex32(call.first_stack[0]),
                    Hex32(call.first_stack[1]),
                    Hex32(call.first_stack[2]),
                    Hex32(call.first_stack[3]),
                    Hex32(call.first_stack[4]),
                    Hex32(call.first_stack[5]),
                    Hex32(call.first_stack[6]),
                    Hex32(call.first_stack[7]));
    }
    logger.info("Win32 MSCDEX available/audio/tracks/requests/current LBA: {}/{}/{}/{}/{}",
                attempt.mscdex_available ? "true" : "false",
                attempt.cd_audio_available ? "true" : "false",
                attempt.mscdex_track_count,
                attempt.mscdex_request_count,
                attempt.cd_audio_current_lba);
    logger.info("Win32 MSCDEX request ES/resolve kind/declines/reason/header: {}/{}/{}/{}/{}",
                Hex32(attempt.mscdex_frame_es),
                attempt.mscdex_last_resolve_kind,
                attempt.mscdex_decline_count,
                attempt.mscdex_last_decline_reason,
                Hex32(attempt.mscdex_last_header_bytes));
    logger.info("Win32 LINEXE scan return EAX/EBP/caller EAX: {}/{}/{}",
                Hex32(attempt.linexe_scan_return_eax),
                Hex32(attempt.linexe_scan_return_ebp),
                Hex32(attempt.linexe_scan_caller_eax));
    logger.info("Win32 LINEXE selector init results: {} {} {}",
                Hex32(attempt.linexe_selector_init_results[0]),
                Hex32(attempt.linexe_selector_init_results[1]),
                Hex32(attempt.linexe_selector_init_results[2]));
    logger.info("Win32 DPMI selector allocations count/request/result: {}/{}/{}",
                attempt.dpmi_allocate_call_count,
                attempt.dpmi_last_allocate_requested_count,
                Hex32(attempt.dpmi_last_allocated_selector));
    logger.info("Win32 LINEXE root selector EAX/GS: {}/{}",
                Hex32(attempt.linexe_root_selector_eax),
                Hex32(attempt.linexe_root_read_gs));
    logger.info("Win32 LINEXE shared segment loads entry/read: {}/{}",
                attempt.linexe_shared_load_entry_count,
                attempt.linexe_shared_load_read_count);
    logger.info("Win32 LINEXE shared segment last selector/offset/value: {}/{}/{}",
                Hex32(attempt.linexe_shared_load_selector),
                Hex32(attempt.linexe_shared_load_offset),
                Hex32(attempt.linexe_shared_load_value));
    logger.info("Win32 LINEXE root word loads offset/selector: {}({})/{}({})",
                Hex32(attempt.linexe_root_offset_load_value),
                attempt.linexe_root_offset_load_success,
                Hex32(attempt.linexe_root_selector_load_value),
                attempt.linexe_root_selector_load_success);
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        logger.info("Win32 LINEXE export slot #{}: {}",
                    index,
                    Hex32(attempt.linexe_resolved_exports[index]));
    }
    logger.info("Win32 LINEXE selector words: {} {} {} {}",
                Hex32(attempt.linexe_selector_words[0]),
                Hex32(attempt.linexe_selector_words[1]),
                Hex32(attempt.linexe_selector_words[2]),
                Hex32(attempt.linexe_selector_words[3]));
    logger.info("Win32 DOS low memory valid: {}",
                attempt.dos_low_memory_valid ? "true" : "false");
    logger.info("Win32 DOS low memory bytes: {}",
                attempt.dos_low_memory_size);
    logger.info("Win32 last single-step context captured: {}",
                attempt.last_single_step_snapshot.captured ? "true"
                                                           : "false");
    if (attempt.last_single_step_snapshot.captured)
    {
        const auto& snapshot = attempt.last_single_step_snapshot;
        logger.info("Win32 last single-step EIP: {}", Hex32(snapshot.eip));
        logger.info("Win32 last single-step EAX: {}", Hex32(snapshot.eax));
        logger.info("Win32 last single-step EBX: {}", Hex32(snapshot.ebx));
        logger.info("Win32 last single-step ECX: {}", Hex32(snapshot.ecx));
        logger.info("Win32 last single-step EDX: {}", Hex32(snapshot.edx));
        logger.info("Win32 last single-step ESI: {}", Hex32(snapshot.esi));
        logger.info("Win32 last single-step EDI: {}", Hex32(snapshot.edi));
        logger.info("Win32 last single-step ESP: {}", Hex32(snapshot.esp));
        logger.info("Win32 last single-step EBP: {}", Hex32(snapshot.ebp));
        logger.info("Win32 last single-step EFLAGS: {}",
                    Hex32(snapshot.eflags));
        logger.info("Win32 last single-step CS: {}", Hex16(snapshot.cs));
        logger.info("Win32 last single-step DS: {}", Hex16(snapshot.ds));
        logger.info("Win32 last single-step ES: {}", Hex16(snapshot.es));
        logger.info("Win32 last single-step SS: {}", Hex16(snapshot.ss));
        logger.info("Win32 last single-step FS: {}", Hex16(snapshot.fs));
        logger.info("Win32 last single-step GS: {}", Hex16(snapshot.gs));
    }
    logger.info("Win32 DOS environment block bytes: {}",
                attempt.dos_environment_block_size);
    logger.info("Win32 DOS environment access observed: {}",
                attempt.last_dos_environment_access_valid ? "true" :
                                                            "false");
    if (attempt.last_dos_environment_access_valid)
    {
        logger.info("Win32 last DOS environment read offset: {}",
                    Hex32(attempt.last_dos_environment_access_offset));
        logger.info("Win32 last DOS environment entry offset: {}",
                    Hex32(attempt.last_dos_environment_entry_offset));
        logger.info("Win32 last DOS environment entry: {}=<redacted>",
                    attempt.last_dos_environment_entry_name);
        logger.info("Win32 last DOS environment value bytes: {}",
                    attempt.last_dos_environment_value_length);
    }
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
    logger.info("Win32 port I/O observation count: {}",
                attempt.port_io.observed_count);
    if (attempt.port_io.observed_count > 0)
    {
        logger.info("Win32 last port I/O address: {}",
                    Hex32(attempt.port_io.last_address));
        logger.info("Win32 last port I/O opcode: {}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.port_io.last_opcode & 0xFFFFU)));
        logger.info("Win32 last port I/O direction: {}",
                    attempt.port_io.last_is_input ? "in" : "out");
        logger.info("Win32 last port I/O port: {}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.port_io.last_port & 0xFFFFU)));
        logger.info("Win32 last port I/O width: {}",
                    attempt.port_io.last_width);
        logger.info("Win32 last port I/O value: {}",
                    Hex32(attempt.port_io.last_value));
        logger.info("Win32 last port I/O handled: {}",
                    attempt.port_io.last_handled ? "true" : "false");
        logger.info("Win32 last port I/O result: {}",
                    attempt.port_io.last_result);
        logger.info("Win32 port I/O trace stored count: {}",
                    attempt.port_io.trace_stored_count);
        logger.info("Win32 port I/O trace limit reached: {}",
                    attempt.port_io.trace_limit_reached ? "true" : "false");
        for (std::uint32_t index = 0;
             index < attempt.port_io.trace_stored_count &&
             index < repiu::platform::win32::kWin32PortIoTraceCapacity;
             ++index)
        {
            const repiu::platform::win32::Win32PortIoTraceEntry& entry =
                attempt.port_io.trace[index];
            if (!entry.valid)
            {
                continue;
            }
            logger.info(
                "Win32 port I/O trace #{} address={} opcode={} direction={} port={} width={} value={} handled={}",
                entry.sequence,
                Hex32(entry.address),
                Hex16(static_cast<std::uint16_t>(entry.opcode & 0xFFFFU)),
                entry.is_input ? "in" : "out",
                Hex16(static_cast<std::uint16_t>(entry.port & 0xFFFFU)),
                entry.width,
                Hex32(entry.value),
                entry.handled ? "true" : "false");
        }
    }
    logger.info("Win32 DOS path trace stored count: {}",
                attempt.dos_path.trace_stored_count);
    logger.info("Win32 DOS path trace limit reached: {}",
                attempt.dos_path.trace_limit_reached ? "true" : "false");
    if (attempt.dos_path.trace_stored_count > 0)
    {
        const std::uint32_t first_sequence =
            attempt.dos_path.observed_count >
                    attempt.dos_path.trace_stored_count
                ? attempt.dos_path.observed_count -
                      attempt.dos_path.trace_stored_count + 1
                : 1;
        const std::uint32_t last_sequence =
            attempt.dos_path.observed_count;
        for (std::uint32_t sequence = first_sequence;
             sequence <= last_sequence;
             ++sequence)
        {
            const std::uint32_t slot =
                (sequence - 1) % repiu::platform::win32::
                    kWin32DosPathTraceCapacity;
            const repiu::platform::win32::Win32DosPathTraceEntry& entry =
                attempt.dos_path.trace[slot];
            if (!entry.valid || entry.sequence != sequence)
            {
                continue;
            }

            logger.info(
                "Win32 DOS path trace #{} service={} result={} error={} drive={} access={} guest={} virtual={} host={}",
                entry.sequence,
                entry.service,
                entry.result,
                Hex16(entry.dos_error),
                Hex8(entry.drive),
                Hex8(entry.access_mode),
                entry.guest_path,
                entry.virtual_path,
                entry.host_path);
        }
    }
    logger.info("Win32 DOS file I/O trace observed/stored: {}/{}",
                attempt.dos_file_io.observed_count,
                attempt.dos_file_io.trace_stored_count);
    logger.info("Win32 DOS file I/O trace wrapped: {}",
                attempt.dos_file_io.trace_wrapped ? "true" : "false");
    if (attempt.dos_file_io.trace_stored_count != 0)
    {
        const std::uint32_t first_sequence =
            attempt.dos_file_io.observed_count -
            attempt.dos_file_io.trace_stored_count + 1U;
        for (std::uint32_t sequence = first_sequence;
             sequence <= attempt.dos_file_io.observed_count;
             ++sequence)
        {
            const std::uint32_t slot = (sequence - 1U) %
                repiu::platform::win32::kWin32DosFileIoTraceCapacity;
            const auto& entry = attempt.dos_file_io.trace[slot];
            if (!entry.valid || entry.sequence != sequence)
            {
                continue;
            }
            std::ostringstream prefix;
            std::ostringstream guest_stack;
            for (std::uint32_t index = 0;
                 index < entry.prefix_size;
                 ++index)
            {
                if (index != 0)
                {
                    prefix << ' ';
                }
                prefix << std::uppercase << std::hex << std::setw(2)
                       << std::setfill('0')
                       << static_cast<unsigned>(entry.prefix[index]);
            }
            for (const std::uint32_t value : entry.guest_stack)
            {
                if (guest_stack.tellp() != std::streampos(0))
                {
                    guest_stack << ' ';
                }
                guest_stack << Hex32(value);
            }
            logger.info(
                "Win32 DOS file I/O #{} op={} handle={} before={} after={} "
                "origin={} offset={} requested={} actual={} error={} "
                "eip={} esp={} stack=[{}] prefix=[{}] path={}",
                entry.sequence,
                entry.operation,
                Hex16(entry.handle),
                Hex32(entry.position_before),
                Hex32(entry.position_after),
                Hex8(entry.origin),
                entry.seek_offset,
                entry.requested_bytes,
                entry.actual_bytes,
                Hex16(entry.dos_error),
                Hex32(entry.guest_eip),
                Hex32(entry.guest_esp),
                guest_stack.str(),
                prefix.str(),
                entry.host_path);
        }
    }
    logger.info("Win32 allocator probe observation count: {}",
                attempt.allocator_probe.observed_count);
    logger.info("Win32 allocator probe trace stored count: {}",
                attempt.allocator_probe.trace_stored_count);
    logger.info("Win32 allocator probe trace wrapped: {}",
                attempt.allocator_probe.trace_wrapped ? "true" : "false");
    if (attempt.allocator_probe.trace_stored_count > 0)
    {
        const std::uint32_t first_sequence =
            attempt.allocator_probe.observed_count >
                    attempt.allocator_probe.trace_stored_count
                ? attempt.allocator_probe.observed_count -
                      attempt.allocator_probe.trace_stored_count + 1
                : 1;
        for (std::uint32_t sequence = first_sequence;
             sequence <= attempt.allocator_probe.observed_count;
             ++sequence)
        {
            const std::uint32_t slot =
                (sequence - 1) % repiu::platform::win32::
                    kWin32AllocatorProbeTraceCapacity;
            const auto& entry = attempt.allocator_probe.trace[slot];
            if (!entry.valid || entry.sequence != sequence)
            {
                continue;
            }
            logger.info(
                "Win32 allocator probe trace #{} EAX={} ESI={} source={} DS={} pending-before={} size-before={} pending-after={} size-after={} result={}",
                entry.sequence,
                Hex32(entry.eax),
                Hex32(entry.esi),
                Hex32(entry.source),
                Hex16(entry.ds),
                entry.pending_before ? "true" : "false",
                Hex32(entry.pending_size_before),
                entry.pending_after ? "true" : "false",
                Hex32(entry.pending_size_after),
                entry.result);
        }
    }
    logger.info("Win32 allocator control-flow observation count: {}",
                attempt.allocator_control_flow.observed_count);
    logger.info("Win32 allocator control-flow trace stored count: {}",
                attempt.allocator_control_flow.trace_stored_count);
    logger.info("Win32 allocator control-flow trace wrapped: {}",
                attempt.allocator_control_flow.trace_wrapped ? "true"
                                                             : "false");
    if (attempt.allocator_control_flow.trace_stored_count > 0)
    {
        const std::uint32_t first_sequence =
            attempt.allocator_control_flow.observed_count >
                    attempt.allocator_control_flow.trace_stored_count
                ? attempt.allocator_control_flow.observed_count -
                      attempt.allocator_control_flow.trace_stored_count + 1
                : 1;
        for (std::uint32_t sequence = first_sequence;
             sequence <= attempt.allocator_control_flow.observed_count;
             ++sequence)
        {
            const std::uint32_t slot =
                (sequence - 1) % repiu::platform::win32::
                    kWin32AllocatorControlFlowTraceCapacity;
            const auto& entry =
                attempt.allocator_control_flow.trace[slot];
            if (!entry.valid || entry.sequence != sequence)
            {
                continue;
            }
            logger.info(
                "Win32 allocator control-flow trace #{} offset={} exception={} bytes={:02X} {:02X} {:02X} {:02X} EAX={} EBX={} EDX={} ESI={} EDI={} EFLAGS={} pending={} size={} read={} address={} value={} explicit-shadow={} zero-backed={} writer={} writer-sequence={} writer-offset={} writer-opcode={} writer-destination={} writer-value={} writer-width={}",
                entry.sequence,
                Hex32(entry.eip_offset),
                Hex32(entry.seh_code),
                entry.opcode[0],
                entry.opcode[1],
                entry.opcode[2],
                entry.opcode[3],
                Hex32(entry.eax),
                Hex32(entry.ebx),
                Hex32(entry.edx),
                Hex32(entry.esi),
                Hex32(entry.edi),
                Hex32(entry.eflags),
                entry.pending_valid ? "true" : "false",
                Hex32(entry.pending_size),
                entry.read_valid ? "true" : "false",
                Hex32(entry.read_address),
                Hex32(entry.read_value),
                entry.read_explicit_shadow ? "true" : "false",
                entry.read_zero_backed ? "true" : "false",
                entry.writer_valid ? "true" : "false",
                entry.writer_sequence,
                Hex32(entry.writer_eip_offset),
                Hex32(entry.writer_opcode),
                Hex32(entry.writer_destination),
                Hex32(entry.writer_value),
                entry.writer_width);
        }
    }
    const auto log_allocator_link_transition = [&](const char* name,
                                                    bool valid,
                                                    const auto& entry) {
        logger.info("Win32 allocator {} link transition valid: {}",
                    name,
                    valid ? "true" : "false");
        if (!valid)
        {
            return;
        }
        logger.info(
            "Win32 allocator {} link transition sequence={} node={} address={} value={} explicit-shadow={} zero-backed={} writer={} writer-offset={} writer-opcode={} writer-destination={} writer-value={} writer-width={}",
            name,
            entry.sequence,
            Hex32(entry.esi),
            Hex32(entry.read_address),
            Hex32(entry.read_value),
            entry.read_explicit_shadow ? "true" : "false",
            entry.read_zero_backed ? "true" : "false",
            entry.writer_valid ? "true" : "false",
            Hex32(entry.writer_eip_offset),
            Hex32(entry.writer_opcode),
            Hex32(entry.writer_destination),
            Hex32(entry.writer_value),
            entry.writer_width);
    };
    log_allocator_link_transition(
        "null",
        attempt.allocator_control_flow.null_link_transition_valid,
        attempt.allocator_control_flow.null_link_transition);
    log_allocator_link_transition(
        "poison",
        attempt.allocator_control_flow.poison_link_transition_valid,
        attempt.allocator_control_flow.poison_link_transition);
    log_allocator_link_transition(
        "root-null",
        attempt.allocator_control_flow.root_transition_valid,
        attempt.allocator_control_flow.root_transition);
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
        logger.info("Win32 last handled DOS interrupt AX: {}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.last_dos_interrupt_ax & 0xFFFFU)));
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
    logger.info("Win32 handled DOS getcwd count: {}",
                attempt.handled_dos_getcwd_count);
    if (attempt.handled_dos_getcwd_count > 0)
    {
        logger.info("Win32 last DOS getcwd drive: {}",
                    Hex8(attempt.last_dos_getcwd_drive));
        logger.info("Win32 last DOS getcwd path: {}",
                    attempt.last_dos_getcwd_path);
        logger.info("Win32 last DOS getcwd result: {}",
                    attempt.last_dos_getcwd_success
                        ? "success"
                        : "failure");
        if (!attempt.last_dos_getcwd_success)
        {
            logger.info("Win32 last DOS getcwd error: {}",
                        Hex16(attempt.last_dos_getcwd_error));
        }
    }
    logger.info("Win32 handled DOS get drive count: {}",
                attempt.handled_dos_getdrive_count);
    if (attempt.handled_dos_getdrive_count > 0)
    {
        logger.info("Win32 last DOS get drive value: {}",
                    Hex8(attempt.last_dos_getdrive_value));
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
    logger.info("Win32 handled DOS read count: {}",
                attempt.handled_dos_read_count);
    if (attempt.handled_dos_read_count > 0)
    {
        logger.info("Win32 last DOS read handle: {}",
                    Hex16(attempt.last_dos_read_handle));
        logger.info("Win32 last DOS read requested bytes: {}",
                    attempt.last_dos_read_requested_bytes);
        logger.info("Win32 last DOS read actual bytes: {}",
                    attempt.last_dos_read_actual_bytes);
        logger.info("Win32 last DOS read buffer: {}",
                    Hex32(attempt.last_dos_read_buffer));
        logger.info("Win32 last DOS read result: {}",
                    attempt.last_dos_read_success ? "success" : "failure");
        if (!attempt.last_dos_read_success)
        {
            logger.info("Win32 last DOS read error: {}",
                        Hex16(attempt.last_dos_read_error));
        }
    }
    logger.info("Win32 handled DOS seek count: {}",
                attempt.handled_dos_seek_count);
    if (attempt.handled_dos_seek_count > 0)
    {
        logger.info("Win32 last DOS seek handle: {}",
                    Hex16(attempt.last_dos_seek_handle));
        logger.info("Win32 last DOS seek origin: {}",
                    Hex8(attempt.last_dos_seek_origin));
        logger.info("Win32 last DOS seek offset: {}",
                    attempt.last_dos_seek_offset);
        logger.info("Win32 last DOS seek position: {}",
                    attempt.last_dos_seek_position);
        logger.info("Win32 last DOS seek result: {}",
                    attempt.last_dos_seek_success ? "success" : "failure");
        if (!attempt.last_dos_seek_success)
        {
            logger.info("Win32 last DOS seek error: {}",
                        Hex16(attempt.last_dos_seek_error));
        }
    }
    logger.info("Win32 handled DOS close count: {}",
                attempt.handled_dos_close_count);
    if (attempt.handled_dos_close_count > 0)
    {
        logger.info("Win32 last DOS close handle: {}",
                    Hex16(attempt.last_dos_close_handle));
        logger.info("Win32 last DOS close result: {}",
                    attempt.last_dos_close_success ? "success" : "failure");
        if (!attempt.last_dos_close_success)
        {
            logger.info("Win32 last DOS close error: {}",
                        Hex16(attempt.last_dos_close_error));
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
        logger.info("Win32 last DOS resize requested end: {}",
                    Hex32(attempt.last_dos_resize_requested_end));
        logger.info("Win32 last DOS resize allocator end: {}",
                    Hex32(attempt.last_dos_resize_allocator_end));
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
    logger.info("Win32 segment load trace stored count: {}",
                attempt.segment_load.trace_stored_count);
    logger.info("Win32 segment load trace wrapped: {}",
                attempt.segment_load.trace_wrapped ? "true" : "false");
    if (attempt.segment_load.trace_stored_count > 0)
    {
        const std::uint32_t first_sequence =
            attempt.segment_load.observed_count >
                    attempt.segment_load.trace_stored_count
                ? attempt.segment_load.observed_count -
                      attempt.segment_load.trace_stored_count + 1
                : 1;
        for (std::uint32_t sequence = first_sequence;
             sequence <= attempt.segment_load.observed_count;
             ++sequence)
        {
            const std::uint32_t slot =
                (sequence - 1) % repiu::platform::win32::
                    kWin32SegmentLoadTraceCapacity;
            const auto& entry = attempt.segment_load.trace[slot];
            if (!entry.valid || entry.sequence != sequence)
            {
                continue;
            }
            logger.info(
                "Win32 segment load trace #{} offset={} register={} selector={} source={}",
                entry.sequence,
                Hex32(entry.eip_offset),
                SegmentRegisterName(entry.segment_register),
                Hex16(entry.selector),
                Hex32(entry.source));
        }
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
    logger.info("Win32 handled low-memory access count: {}",
                attempt.handled_low_memory_access_count);
    if (attempt.handled_low_memory_access_count > 0)
    {
        logger.info("Win32 last low-memory access address: {}",
                    Hex32(attempt.last_low_memory_access_address));
        logger.info("Win32 last low-memory access opcode: {}",
                    Hex8(static_cast<std::uint8_t>(
                        attempt.last_low_memory_access_opcode & 0xFFU)));
        logger.info("Win32 last low-memory access ESI: {}",
                    Hex32(attempt.last_low_memory_access_esi));
        logger.info("Win32 last low-memory access EDI: {}",
                    Hex32(attempt.last_low_memory_access_edi));
        logger.info("Win32 last low-memory access destination: {}",
                    Hex32(attempt.last_low_memory_access_destination));
        logger.info("Win32 last low-memory access value: {}",
                    Hex32(attempt.last_low_memory_access_value));
    }
    logger.info("Win32 low-memory read emulate count: {}",
                attempt.low_memory_read_emulate_count);
    logger.info("Win32 low-memory read emulate debug stage: {}",
                attempt.debug_emulate_stage);
    if (attempt.debug_emulate_stage > 0)
    {
        logger.info("Win32 low-memory read emulate debug decode result: {}",
                    Hex32(attempt.debug_emulate_decode_result));
        logger.info("Win32 low-memory read emulate debug calculated address: {}",
                    Hex32(attempt.debug_emulate_calculated_address));
    }
    if (attempt.low_memory_read_emulate_count > 0)
    {
        logger.info("Win32 last low-memory read emulate address: {}",
                    Hex32(attempt.last_low_memory_read_emulate_address));
        logger.info("Win32 last low-memory read emulate EIP: {}",
                    Hex32(attempt.last_low_memory_read_emulate_eip));
        logger.info("Win32 last low-memory read emulate value: {}",
                    Hex32(attempt.last_low_memory_read_emulate_value));
        logger.info("Win32 last low-memory read emulate reg: {}",
                    static_cast<unsigned>(attempt.last_low_memory_read_emulate_reg));
    }
    logger.info("Win32 handled memory store count: {}",
                attempt.handled_memory_store_count);
    if (attempt.handled_memory_store_count > 0)
    {
        logger.info("Win32 last handled memory store address: {}",
                    Hex32(attempt.last_memory_store_address));
        if (attempt.last_memory_store_opcode > 0xFFU)
        {
            logger.info("Win32 last memory store opcode: {}",
                        Hex16(static_cast<std::uint16_t>(
                            attempt.last_memory_store_opcode & 0xFFFFU)));
        }
        else
        {
            logger.info("Win32 last memory store opcode: {}",
                        Hex8(static_cast<std::uint8_t>(
                            attempt.last_memory_store_opcode & 0xFFU)));
        }
        logger.info("Win32 last memory store width: {}",
                    attempt.last_memory_store_width);
        logger.info("Win32 last memory store source kind: {}",
                    attempt.last_memory_store_source_kind);
        logger.info("Win32 last memory store destination: {}",
                    Hex32(attempt.last_memory_store_destination));
        logger.info("Win32 last memory store value: {}",
                    Hex32(attempt.last_memory_store_value));
        logger.info("Win32 last memory store applied: {}",
                    attempt.last_memory_store_applied ? "true" : "false");
    }
    logger.info("Win32 shadow memory write count: {}",
                attempt.shadow_memory_write_count);
    logger.info("Win32 REP MOVS safe-copy failure count: {}",
                attempt.rep_movs_copy_failure_count);
    if (attempt.rep_movs_copy_failure_count != 0)
    {
        logger.info("Win32 last REP MOVS failure stage/error: {}/{}",
                    attempt.last_rep_movs_copy_failure_stage,
                    attempt.last_rep_movs_copy_error);
        logger.info("Win32 last REP MOVS source/destination/bytes: {}/{}/{}",
                    Hex32(attempt.last_rep_movs_copy_source),
                    Hex32(attempt.last_rep_movs_copy_destination),
                    attempt.last_rep_movs_copy_bytes);
    }
    logger.info("Win32 shadow memory read hit count: {}",
                attempt.shadow_memory_read_hit_count);
    logger.info("Win32 shadow memory byte count: {}",
                attempt.shadow_memory_byte_count);
    logger.info("Win32 shadow memory range valid: {}",
                attempt.shadow_memory_range_valid ? "true" : "false");
    if (attempt.shadow_memory_range_valid)
    {
        logger.info("Win32 shadow memory min address: {}",
                    Hex32(attempt.shadow_memory_min_address));
        logger.info("Win32 shadow memory max address: {}",
                    Hex32(attempt.shadow_memory_max_address));
    }
    logger.info("Win32 minimal execution thread exit code: {}",
                attempt.thread_exit_code);
    logger.info("Win32 DOS termination captured: {}",
                attempt.dos_termination_captured ? "true" : "false");
    if (attempt.dos_termination_captured)
    {
        logger.info("Win32 DOS termination AX/EIP/ESP: {}/{}/{}",
                    Hex16(static_cast<std::uint16_t>(
                        attempt.dos_termination_ax)),
                    Hex32(attempt.dos_termination_eip),
                    Hex32(attempt.dos_termination_esp));
        std::ostringstream stack;
        for (std::uint32_t index = 0;
             index < repiu::platform::win32::
                 kWin32DosTerminationStackCapacity;
             ++index)
        {
            if (index != 0)
            {
                stack << ' ';
            }
            stack << Hex32(attempt.dos_termination_stack[index]);
        }
        logger.info("Win32 DOS termination stack: {}", stack.str());
    }
    if (!attempt.hle_stdout_output.empty() ||
        !attempt.hle_stderr_output.empty())
    {
        logger.info("Win32 HLE stdout/stderr bytes: {}/{}",
                    attempt.hle_stdout_output.size(),
                    attempt.hle_stderr_output.size());
        std::shared_ptr<spdlog::logger> guest_logger =
            spdlog::get(std::string(executable_name));
        if (guest_logger == nullptr)
        {
            guest_logger = spdlog::stderr_color_mt(
                std::string(executable_name));
            guest_logger->set_pattern("[%X.%e] [%8l] [%n] %v");
            guest_logger->set_level(spdlog::level::info);
        }
        LogGuestOutput(*guest_logger,
                       spdlog::level::info,
                       attempt.hle_stdout_output);
        LogGuestOutput(*guest_logger,
                       spdlog::level::err,
                       attempt.hle_stderr_output);
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

bool SelectAndReserveRelocatedImageBase(
    std::uint32_t reserve_size,
    spdlog::logger& logger,
    repiu::platform::win32::Win32AddressRangeReservation* reservation)
{
    if (reservation == nullptr)
    {
        return false;
    }
    *reservation = repiu::platform::win32::Win32AddressRangeReservation{};

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
        if (!probe.range_available)
        {
            continue;
        }

        repiu::platform::win32::Win32AddressRangeReservation candidate_reservation;
        if (!repiu::platform::win32::ReserveAndCommitWin32RuntimeAddressRange(
                policy, &candidate_reservation))
        {
            continue;
        }

        if (!candidate_reservation.reserved ||
            candidate_reservation.reserved_base != candidate)
        {
            logger.warn("Win32 relocated base candidate {} reserve result: {}",
                        Hex32(candidate),
                        candidate_reservation.message);
            repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
                candidate_reservation);
            continue;
        }

        logger.info("Win32 relocated base candidate {} reserved size: {}",
                    Hex32(candidate),
                    Hex32(candidate_reservation.reserved_size));
        *reservation = candidate_reservation;
        return true;
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
    std::optional<repiu::target::TargetProfile> mounted_profile;
    std::optional<std::filesystem::path> cd_chd_path;
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

    if (profile->id == "pumpit1")
    {
        repiu::assets::PumpIt1MountResult mount;
        if (!repiu::assets::PreparePumpIt1Mount(
                "roms", "build/runtime_mounts", &mount) ||
            !mount.valid || !mount.mounted)
        {
            logger->error("pumpit1 CHD mount failed: {}", mount.message);
            return 1;
        }
        logger->info("pumpit1 ROM ZIP: {}", mount.rom_zip_path.string());
        logger->info("pumpit1 CHD: {}", mount.chd_path.string());
        logger->info("pumpit1 mount root: {}", mount.mount_root.string());
        logger->info("pumpit1 mount cache reused: {}",
                     mount.cache_reused ? "true" : "false");
        logger->info("pumpit1 extracted files/bytes: {}/{}",
                     mount.extracted_file_count,
                     mount.extracted_byte_count);
        mounted_profile = *profile;
        mounted_profile->executable_path = mount.executable_path;
        mounted_profile->working_directory = mount.mount_root / "PIU";
        mounted_profile->asset_root = mount.mount_root;
        cd_chd_path = mount.chd_path;
        profile = &mounted_profile.value();
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

    const std::filesystem::path dos4gw_path =
        profile->executable_path.parent_path() / "DOS4GW.EXE";
    std::optional<repiu::exe::Dos16mBoundModule> linexe_runtime_module;
    if (std::filesystem::exists(dos4gw_path))
    {
        std::vector<std::uint8_t> dos4gw_data;
        if (!ReadBinaryFile(dos4gw_path, &dos4gw_data, &read_error))
        {
            logger->error("Failed to read user DOS4GW asset {}: {}",
                          dos4gw_path.string(),
                          read_error);
            return 1;
        }
        std::vector<repiu::exe::Dos16mBoundModule> dos16m_modules;
        repiu::exe::ParseError dos16m_error;
        if (!repiu::exe::ParseDos16mBoundModules(
                dos4gw_data, &dos16m_modules, &dos16m_error))
        {
            PrintParseError(*logger, dos16m_error);
            return 1;
        }
        const repiu::exe::Dos16mBoundModule* linexe_module =
            repiu::exe::FindDos16mBoundModule(
                dos16m_modules, "LINEXE.EXP");
        if (linexe_module == nullptr)
        {
            logger->error("User DOS4GW asset contains no LINEXE.EXP module");
            return 1;
        }
        linexe_runtime_module = *linexe_module;
        logger->info("DOS4GW bound module count: {}", dos16m_modules.size());
        logger->info("LINEXE runtime extraction: header={} next={} entry={}:{} segments={} relocations={}",
                     Hex32(linexe_module->header_file_offset),
                     Hex32(linexe_module->next_header_file_offset),
                     Hex32(linexe_module->initial_cs),
                     Hex32(linexe_module->initial_ip),
                     linexe_module->segments.size(),
                     linexe_module->relocation_count);
        for (const repiu::exe::Dos16mBoundSegment& segment :
             linexe_module->segments)
        {
            logger->info("LINEXE extracted segment: selector={} limit={} access={} image={} relocations={}",
                         Hex32(segment.selector),
                         Hex32(segment.limit),
                         Hex32(segment.access),
                         segment.image.size(),
                         segment.selector_relocation_offsets.size());
        }
    }
    else
    {
        logger->info("No adjacent user DOS4GW asset; LINEXE extraction skipped");
    }

    std::vector<repiu::exe::LeResidentName> glide_exports;
    const std::filesystem::path glide_path =
        profile->executable_path.parent_path() / "Glide2x.ovl";
    if (std::filesystem::exists(glide_path))
    {
        std::vector<std::uint8_t> glide_data;
        repiu::exe::MzHeader glide_mz;
        repiu::exe::LeHeader glide_le;
        repiu::exe::ParseError glide_error;
        if (!ReadBinaryFile(glide_path, &glide_data, &read_error) ||
            !repiu::exe::ParseMzHeader(
                glide_data, &glide_mz, &glide_error) ||
            !repiu::exe::ParseLeHeader(
                glide_data, glide_mz.le_offset, &glide_le, &glide_error) ||
            !repiu::exe::ParseLeResidentNames(
                glide_data, glide_le, &glide_exports, &glide_error))
        {
            logger->error("Failed to parse Glide2x.ovl export metadata");
            return 1;
        }
        logger->info("Glide2x resident export metadata count: {}",
                     glide_exports.size());
    }

    repiu::exe::ParseError error;
    repiu::exe::Dos4gwLoadResult load_result;
    if (!repiu::exe::LoadDos4gwExecutable(data, *profile, &load_result,
                                          &error))
    {
        PrintParseError(*logger, error);
        return 1;
    }

    // The guest allocator grows its program block to ~0x38AA000 bytes
    // (~57 MiB) via INT 21h AH=4Ah with 32-bit EBX paragraph counts
    // (Task 221); 64 MiB of slack lets those resizes succeed against
    // genuinely committed arena memory while the allocator ceiling still
    // guards the LINEXE pages at the arena end.
    constexpr std::uint32_t kRuntimeArenaExpansionSlack = 0x08000000;
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

    repiu::platform::win32::Win32AddressRangeReservation
        relocated_arena_reservation;
    if (!SelectAndReserveRelocatedImageBase(
            arena_size_plan.arena_reserve_size,
            *logger,
            &relocated_arena_reservation))
    {
        logger->error("Failed to reserve an available relocated image base");
        return 1;
    }

    const std::uint32_t relocated_image_base =
        relocated_arena_reservation.reserved_base;

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
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        return 1;
    }

    PrintRuntimeMemoryArenaPlan(*logger, arena_plan);

    repiu::runtime::RelocatableRuntimeImagePlan relocatable_plan;
    if (!repiu::runtime::BuildRelocatableRuntimeImagePlan(
            load_result, relocated_image_base, &relocatable_plan, &error))
    {
        PrintParseError(*logger, error);
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        return 1;
    }

    repiu::runtime::RelocatedRuntimeImage relocated_image;
    if (!repiu::runtime::BuildRelocatedRuntimeImage(
            load_result, relocatable_plan, &relocated_image, &error))
    {
        PrintParseError(*logger, error);
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        return 1;
    }

    const bool use_aot_backend = UseAotExecutionBackend();
    const bool use_dynamic_aot = UseDynamicAotTranslation();
    repiu::runtime::AotTranslationPlan aot_plan;
    repiu::runtime::AotCodeCacheImage aot_image;
    if (use_aot_backend &&
        (!repiu::runtime::BuildAotTranslationPlan(relocated_image,
                                                  &aot_plan) ||
         !repiu::runtime::BuildAotCodeCacheImage(aot_plan, &aot_image)))
    {
        logger->error("Failed to build requested AOT execution image: {} / {}",
                      aot_plan.message, aot_image.message);
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        return 1;
    }
    logger->info("Win32 requested execution backend: {}",
                 use_dynamic_aot ? "aot-dynamic" :
                 use_aot_backend ? "aot" : "legacy");

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
            profile->asset_root,
            profile->working_directory,
            &dos_file_system) ||
        !dos_file_system.valid)
    {
        PrintDosVirtualFileSystem(*logger, dos_file_system);
        logger->error("Failed to initialize DOS virtual filesystem");
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        return 1;
    }
    PrintDosVirtualFileSystem(*logger, dos_file_system);

    repiu::platform::win32::Win32RelocatedImagePlacement placement;
    if (!repiu::platform::win32::PlaceWin32RelocatedImageInReservedRange(
            relocated_image,
            relocated_arena_reservation,
            &placement))
    {
        logger->error("Failed to place relocated image");
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        return 1;
    }

    PrintPlacement(*logger, placement);
    if (!placement.placed)
    {
        repiu::platform::win32::ReleaseWin32RuntimeAddressRange(
            relocated_arena_reservation);
        logger->error("Failed to place relocated image");
        return 1;
    }
    repiu::platform::win32::Win32AotCodeCachePlacement aot_placement;
    if (use_aot_backend &&
        (!repiu::platform::win32::PlaceWin32AotCodeCache(
             aot_image, &aot_placement) ||
         !aot_placement.placed))
    {
        logger->error("Failed to place requested AOT code cache: {}",
                      aot_placement.message);
        repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
        return 1;
    }
    if (use_aot_backend)
    {
        logger->info("Win32 AOT cache base/bytes/entry: {}/{}/{}",
                     Hex32(aot_placement.base_address),
                     aot_placement.size,
                     Hex32(aot_placement.entry_address));
        logger->info("Win32 AOT plan jump tables/targets: {}/{}",
                     aot_plan.jump_table_count,
                     aot_plan.jump_table_target_count);
    }
    logger->flush();
    repiu::platform::win32::Win32MinimalExecutionAttempt attempt;
    const bool use_dos_console_hle =
        profile->hle_profile_id == "dos4gw_console_sample";
    const std::uint32_t execution_timeout_milliseconds =
        ReadExecutionTimeoutMilliseconds();
    if (execution_timeout_milliseconds == INFINITE)
    {
        logger->info("Win32 guest execution timeout: disabled");
    }
    else
    {
        logger->info("Win32 guest execution timeout: {} ms",
                     execution_timeout_milliseconds);
    }
    const bool attempted_execution = use_aot_backend
        ? repiu::platform::win32::AttemptWin32GuestStackAotExecution(
              placement,
              aot_placement,
              stack_plan,
              dos_file_system,
              linexe_runtime_module ? &*linexe_runtime_module : nullptr,
              glide_exports.empty() ? nullptr : &glide_exports,
              cd_chd_path ? &*cd_chd_path : nullptr,
              use_dynamic_aot,
              execution_timeout_milliseconds,
              &attempt)
        : use_dos_console_hle
            ? repiu::platform::win32::AttemptWin32GuestStackHleExecution(
                  placement,
                  stack_plan,
                  dos_file_system,
                  execution_timeout_milliseconds,
                  &attempt)
            : repiu::platform::win32::AttemptWin32GuestStackTrapExecution(
                  placement,
                  stack_plan,
                  dos_file_system,
                  linexe_runtime_module ? &*linexe_runtime_module : nullptr,
                  glide_exports.empty() ? nullptr : &glide_exports,
                  cd_chd_path ? &*cd_chd_path : nullptr,
                  execution_timeout_milliseconds,
                  &attempt);
    if (!attempted_execution)
    {
        logger->error("Failed to attempt minimal original entry execution");
        repiu::platform::win32::ReleaseWin32AotCodeCache(&aot_placement);
        repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
        return 1;
    }

    for (const auto& resident : glide_exports)
    {
        if (resident.ordinal == 95)
        {
            logger->info("[debug-ordinal-95] ordinal 95 name: {}", resident.name);
        }
    }

    PrintExecutionAttempt(*logger,
                          attempt,
                          profile->executable_path.filename().string());
    repiu::platform::win32::ReleaseWin32AotCodeCache(&aot_placement);
    if (attempt.exception_caught)
    {
        std::uint32_t target_address = attempt.seh_exception_address;
        if (attempt.aot_exception_mapping_valid)
        {
            target_address = attempt.aot_exception_guest_address;
        }
        repiu::runtime::RelocatedImageByteWindow window;
        repiu::runtime::BuildRelocatedImageByteWindow(
            relocated_image,
            target_address,
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
    if (attempt.last_single_step_snapshot.captured)
    {
        repiu::runtime::RelocatedImageByteWindow window;
        repiu::runtime::BuildRelocatedImageByteWindow(
            relocated_image,
            attempt.last_single_step_snapshot.eip,
            16,
            16,
            &window);
        PrintByteWindow(*logger, window);
    }
    if (!attempt.timed_out)
    {
        repiu::platform::win32::ReleaseWin32RelocatedImage(placement);
    }
    return 0;
}
