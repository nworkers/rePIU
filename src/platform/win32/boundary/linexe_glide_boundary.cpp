#include "linexe_glide_boundary.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "instruction_emulation.h"

#include "repiu/hle/glide_texture_decode.h"
#include "repiu/hle/glide_lfb.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace repiu::platform::win32
{
namespace
{

void RecordGlideTextureGateTrace(ThreadContext* context, const CONTEXT* win32_context, const repiu::hle::GlideExportGate& glide_export, std::uint32_t return_address, std::uint32_t return_eax, bool is_max_address)
{
    const std::uint32_t sequence = context->glide_texture_gate_trace_count + 1U;
    Win32GlideTextureGateTraceEntry& entry = context->glide_texture_gate_trace[(sequence - 1U) % kWin32GlideTextureGateTraceCapacity];
    entry.valid = true;
    entry.sequence = sequence;
    entry.ordinal = glide_export.ordinal;
    entry.is_max_address = is_max_address;
    entry.entry_eip = static_cast<std::uint32_t>(win32_context->Eip);
    entry.entry_esp = static_cast<std::uint32_t>(win32_context->Esp);
    entry.return_address = return_address;
    entry.tmu = context->glide_gate_stack[1];
    entry.entry_eax = static_cast<std::uint32_t>(win32_context->Eax);
    entry.return_eax = return_eax;
    entry.planned_return_esp = entry.entry_esp + 2U * sizeof(std::uint32_t);
    context->glide_texture_gate_trace_count = sequence;
    if (sequence > kWin32GlideTextureGateTraceCapacity)
    {
        context->glide_texture_gate_trace_wrapped = true;
    }
}

void DumpTextureToBmp(std::uint32_t start_address, std::uint32_t format, std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& rgba)
{
    static std::uint32_t s_dump_counter = 0;
    try
    {
        std::filesystem::path dump_dir = "build/texture_dumps";
        std::filesystem::create_directories(dump_dir);

        std::ostringstream filename_stream;
        filename_stream << "tex_0x" << std::uppercase << std::hex << start_address
                        << "_fmt" << std::dec << format
                        << "_" << width << "x" << height
                        << "_" << ++s_dump_counter << ".bmp";

        std::filesystem::path filepath = dump_dir / filename_stream.str();
        std::ofstream file(filepath, std::ios::binary);
        if (!file)
        {
            return;
        }

        #pragma pack(push, 1)
        struct BmpFileHeader
        {
            std::uint16_t bfType = 0x4D42;
            std::uint32_t bfSize = 0;
            std::uint16_t bfReserved1 = 0;
            std::uint16_t bfReserved2 = 0;
            std::uint32_t bfOffBits = 54;
        };

        struct BmpInfoHeader
        {
            std::uint32_t biSize = 40;
            std::int32_t biWidth = 0;
            std::int32_t biHeight = 0;
            std::uint16_t biPlanes = 1;
            std::uint16_t biBitCount = 32;
            std::uint32_t biCompression = 0;
            std::uint32_t biSizeImage = 0;
            std::int32_t biXPelsPerMeter = 3780;
            std::int32_t biYPelsPerMeter = 3780;
            std::uint32_t biClrUsed = 0;
            std::uint32_t biClrImportant = 0;
        };
        #pragma pack(pop)

        // Write 24-bit bottom-up BI_RGB, the one BMP form every viewer handles.
        // The previous 32-bit top-down form is legal but fragile: the fourth
        // byte of a 32-bit BI_RGB pixel is officially reserved, so viewers
        // disagree on whether it is alpha, and negative biHeight is uncommon
        // enough that some refuse the file outright -- which reads as "the dump
        // never happened" even though the bytes are correct.
        const std::size_t row_padding = (4U - ((width * 3U) % 4U)) % 4U;
        const std::size_t row_bytes = width * 3U + row_padding;
        std::vector<std::uint8_t> bgr(row_bytes * height, 0U);
        for (std::uint32_t y = 0; y < height; ++y)
        {
            // Bottom-up: BMP row 0 is the image's last row.
            const std::uint32_t source_row = height - 1U - y;
            std::uint8_t* out = bgr.data() + static_cast<std::size_t>(y) *
                row_bytes;
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const std::size_t i =
                    (static_cast<std::size_t>(source_row) * width + x) * 4U;
                out[x * 3U + 0U] = rgba[i + 2U]; // B
                out[x * 3U + 1U] = rgba[i + 1U]; // G
                out[x * 3U + 2U] = rgba[i + 0U]; // R
            }
        }

        BmpFileHeader file_header;
        BmpInfoHeader info_header;
        info_header.biBitCount = 24;
        info_header.biWidth = static_cast<std::int32_t>(width);
        info_header.biHeight = static_cast<std::int32_t>(height);
        info_header.biSizeImage = static_cast<std::uint32_t>(bgr.size());
        file_header.bfSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) +
            info_header.biSizeImage;

        file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
        file.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
        file.write(reinterpret_cast<const char*>(bgr.data()), bgr.size());

        // Alpha is where a texture "not showing" usually hides, and dropping it
        // from the colour dump would lose that. Emit it as a separate grayscale
        // image for formats that carry one.
        const bool has_alpha = format == 8U || format == 11U || format == 12U ||
            format == 13U || format == 14U || format == 2U || format == 4U;
        if (has_alpha)
        {
            std::filesystem::path alpha_path = dump_dir /
                (filename_stream.str().substr(
                     0, filename_stream.str().size() - 4U) + "_alpha.bmp");
            std::ofstream alpha_file(alpha_path, std::ios::binary);
            if (alpha_file)
            {
                std::vector<std::uint8_t> mono(row_bytes * height, 0U);
                for (std::uint32_t y = 0; y < height; ++y)
                {
                    const std::uint32_t source_row = height - 1U - y;
                    std::uint8_t* out = mono.data() +
                        static_cast<std::size_t>(y) * row_bytes;
                    for (std::uint32_t x = 0; x < width; ++x)
                    {
                        const std::uint8_t a = rgba[(static_cast<std::size_t>(
                            source_row) * width + x) * 4U + 3U];
                        out[x * 3U + 0U] = a;
                        out[x * 3U + 1U] = a;
                        out[x * 3U + 2U] = a;
                    }
                }
                alpha_file.write(reinterpret_cast<const char*>(&file_header),
                                 sizeof(file_header));
                alpha_file.write(reinterpret_cast<const char*>(&info_header),
                                 sizeof(info_header));
                alpha_file.write(reinterpret_cast<const char*>(mono.data()),
                                 mono.size());
            }
        }
    }
    catch (...)
    {
        // Fail-safe to avoid crashing the loader on diagnostics
    }
}

} // namespace

void RecordAllocatorControlFlowException(
    EXCEPTION_POINTERS* exception_info,
    ThreadContext* context)
{
    if (exception_info == nullptr || context == nullptr ||
        exception_info->ContextRecord == nullptr)
    {
        return;
    }

    const std::uint32_t eip = static_cast<std::uint32_t>(
        exception_info->ContextRecord->Eip);
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    if (eip < context->runtime_base ||
        static_cast<std::uint64_t>(eip) + 4U > runtime_end)
    {
        return;
    }

    const std::uint32_t eip_offset = eip - context->runtime_base;
    constexpr std::uint32_t kAllocatorTraceBegin = 0x000F7A60U;
    constexpr std::uint32_t kAllocatorTraceEnd = 0x000F7AD5U;
    if (eip_offset < kAllocatorTraceBegin ||
        eip_offset >= kAllocatorTraceEnd)
    {
        return;
    }

    Win32AllocatorControlFlowObservation& observation =
        context->allocator_control_flow;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32AllocatorControlFlowTraceCapacity;
    Win32AllocatorControlFlowTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.eip_offset = eip_offset;
    entry.seh_code = exception_info->ExceptionRecord != nullptr
        ? exception_info->ExceptionRecord->ExceptionCode
        : 0;
    const std::uint8_t* instruction =
        reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(eip));
    std::memcpy(entry.opcode, instruction, sizeof(entry.opcode));
    entry.eax = exception_info->ContextRecord->Eax;
    entry.ebx = exception_info->ContextRecord->Ebx;
    entry.edx = exception_info->ContextRecord->Edx;
    entry.esi = exception_info->ContextRecord->Esi;
    entry.edi = exception_info->ContextRecord->Edi;
    entry.eflags = exception_info->ContextRecord->EFlags;
    entry.pending_valid = context->pending_shadow_allocation_valid;
    entry.pending_size = context->pending_shadow_allocation_size;
    observation.observed_count = sequence;
    if (observation.trace_stored_count <
        kWin32AllocatorControlFlowTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_wrapped = true;
    }
}

bool HandleLinexeFarTransferBoundary(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->linexe_environment_active)
    {
        return false;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    constexpr std::size_t kFarPointerSize = 6U;
    if (IsGuestRangeReadable(context, instruction, kFarPointerSize) &&
        instruction[0] == 0xFFU && instruction[1] == 0x1DU)
    {
        const std::uint32_t pointer_address =
            static_cast<std::uint32_t>(instruction[2]) |
            (static_cast<std::uint32_t>(instruction[3]) << 8U) |
            (static_cast<std::uint32_t>(instruction[4]) << 16U) |
            (static_cast<std::uint32_t>(instruction[5]) << 24U);
        const auto* pointer = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(pointer_address));
        if (IsGuestRangeReadable(context, pointer, kFarPointerSize))
        {
            const std::uint32_t target_offset =
                static_cast<std::uint32_t>(pointer[0]) |
                (static_cast<std::uint32_t>(pointer[1]) << 8U) |
                (static_cast<std::uint32_t>(pointer[2]) << 16U) |
                (static_cast<std::uint32_t>(pointer[3]) << 24U);
            const std::uint16_t target_selector =
                static_cast<std::uint16_t>(pointer[4]) |
                static_cast<std::uint16_t>(pointer[5] << 8U);
            repiu::hle::LinexeService service{};
            ++context->linexe_indirect_far_call_count;
            context->linexe_indirect_far_call_source =
                static_cast<std::uint32_t>(win32_context->Eip);
            context->linexe_indirect_far_call_pointer = pointer_address;
            context->linexe_indirect_far_call_offset = target_offset;
            context->linexe_indirect_far_call_selector = target_selector;
            context->linexe_indirect_far_call_known_export =
                repiu::hle::DecodeLinexeOriginalExport(
                    context->linexe_gate_plan,
                    target_selector,
                    static_cast<std::uint16_t>(target_offset),
                    &service);
        }
        return false;
    }
    constexpr std::uint8_t kFarTransferPrefix[] =
        {0x66U, 0xEAU, 0x04U, 0x00U};
    if (!IsGuestRangeReadable(
            context, instruction, sizeof(kFarTransferPrefix) + 2U) ||
        std::memcmp(instruction,
                    kFarTransferPrefix,
                    sizeof(kFarTransferPrefix)) != 0)
    {
        return false;
    }

    const std::uint16_t target_selector = static_cast<std::uint16_t>(
        win32_context->Edi >> 16U);
    const std::uint16_t target_offset = static_cast<std::uint16_t>(
        win32_context->Edi & 0xFFFFU);
    repiu::hle::LinexeService service{};
    if (!repiu::hle::DecodeLinexeOriginalExport(
            context->linexe_gate_plan,
            target_selector,
            target_offset,
            &service))
    {
        return false;
    }

    ++context->linexe_bridge_entry_count;
    context->linexe_bridge_gate_valid = true;
    context->linexe_bridge_selector = target_selector;
    context->linexe_bridge_offset = target_offset;
    context->linexe_bridge_service = static_cast<std::uint32_t>(service);
    context->linexe_bridge_esp = win32_context->Esp;
    context->linexe_bridge_ebp = win32_context->Ebp;
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context,
                             stack,
                             sizeof(context->linexe_bridge_stack)))
    {
        std::memcpy(context->linexe_bridge_stack,
                    stack,
                    sizeof(context->linexe_bridge_stack));
    }
    std::memset(context->linexe_bridge_argument_text,
                0,
                sizeof(context->linexe_bridge_argument_text));
    std::memset(context->linexe_bridge_stack_text,
                0,
                sizeof(context->linexe_bridge_stack_text));
    for (std::size_t stack_index = 0;
         stack_index < std::size(context->linexe_bridge_stack);
         ++stack_index)
    {
        const auto* candidate = reinterpret_cast<const char*>(
            static_cast<std::uintptr_t>(
                context->linexe_bridge_stack[stack_index]));
        std::size_t text_length = 0;
        for (; text_length + 1U <
                   sizeof(context->linexe_bridge_stack_text[stack_index]);
             ++text_length)
        {
            if (!IsGuestRangeReadable(context, candidate + text_length, 1U))
            {
                break;
            }
            const unsigned char value = static_cast<unsigned char>(
                candidate[text_length]);
            if (value == 0)
            {
                break;
            }
            if (!std::isprint(value))
            {
                text_length = 0;
                break;
            }
            context->linexe_bridge_stack_text[stack_index][text_length] =
                static_cast<char>(value);
        }
        if (text_length == 0)
        {
            context->linexe_bridge_stack_text[stack_index][0] = '\0';
        }
    }
    const auto* argument = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(context->linexe_bridge_stack[9]));
    for (std::size_t index = 0;
         index + 1U < sizeof(context->linexe_bridge_argument_text);
         ++index)
    {
        if (!IsGuestRangeReadable(context, argument + index, 1U))
        {
            break;
        }
        context->linexe_bridge_argument_text[index] = argument[index];
        if (argument[index] == '\0')
        {
            break;
        }
    }

    constexpr std::uint32_t kVirtualGlideModuleHandle = 1U;
    // The bridge consumes its three dwords and restores the ES value saved by
    // the wrapper.  The shared epilogue then owns EBX/ESI/EDI/EBP and RET.
    const bool is_glide_module =
        _stricmp(context->linexe_bridge_argument_text, "glide2x.ovl") == 0;
    const repiu::hle::GlideExportGate* glide_export =
        repiu::hle::FindGlideExportByName(
            context->glide_gate_plan,
            context->linexe_bridge_stack_text[12]);
    if (service == repiu::hle::LinexeService::kGetProcedureAddress &&
        context->linexe_bridge_stack[11] == kVirtualGlideModuleHandle)
    {
        std::strncpy(context->linexe_get_proc_name,
                     context->linexe_bridge_stack_text[12],
                     sizeof(context->linexe_get_proc_name) - 1U);
    }    if (service == repiu::hle::LinexeService::kGetProcedureAddress &&
        context->linexe_bridge_stack[11] == kVirtualGlideModuleHandle &&
        glide_export != nullptr)
    {
        const std::uint32_t result_pointer =
            context->linexe_bridge_stack[13];
        const std::uint32_t gate_address =
            context->linexe_arena_layout.gate_code_base +
            glide_export->gate_offset;
        const std::uint32_t procedure_pointer[2] = {
            gate_address,
            static_cast<std::uint32_t>(win32_context->SegCs),
        };
        if (!WriteGuestBytes(
                context,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(result_pointer)),
                procedure_pointer,
                sizeof(procedure_pointer)))
        {
            return false;
        }

        ++context->linexe_get_proc_count;
        context->linexe_get_proc_result_pointer = gate_address;
        std::strncpy(context->linexe_get_proc_name,
                     glide_export->name.c_str(),
                     sizeof(context->linexe_get_proc_name) - 1U);
        win32_context->Eax = 1U;
        context->guest_es = static_cast<std::uint16_t>(
            context->linexe_bridge_stack[5] & 0xFFFFU);
        win32_context->Ebx = context->linexe_bridge_stack[6];
        win32_context->Esi = context->linexe_bridge_stack[7];
        win32_context->Edi = context->linexe_bridge_stack[8];
        win32_context->Ebp = context->linexe_bridge_stack[9];
        win32_context->Eip = context->linexe_bridge_stack[10];
        win32_context->Esp += 11U * sizeof(std::uint32_t);
        return true;
    }
    if (service != repiu::hle::LinexeService::kLoadModule ||
        !is_glide_module)
    {
        return false;
    }

    ++context->linexe_virtual_module_load_count;
    context->linexe_virtual_module_handle = kVirtualGlideModuleHandle;
    win32_context->Eax = kVirtualGlideModuleHandle;
    context->guest_es = static_cast<std::uint16_t>(
        context->linexe_bridge_stack[3] & 0xFFFFU);
    win32_context->Ebx = context->linexe_bridge_stack[4];
    win32_context->Esi = context->linexe_bridge_stack[5];
    win32_context->Edi = context->linexe_bridge_stack[6];
    win32_context->Ebp = context->linexe_bridge_stack[7];
    win32_context->Eip = context->linexe_bridge_stack[8];
    win32_context->Esp += 9U * sizeof(std::uint32_t);
    return true;
}

bool HandleGlideGateBoundary(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint32_t gate_begin =
        context != nullptr
            ? context->linexe_arena_layout.gate_code_base +
                context->glide_gate_plan.first_gate_offset
            : 0U;
    if (win32_context == nullptr || context == nullptr ||
        !context->linexe_environment_active ||
        win32_context->Eip < gate_begin)
    {
        return false;
    }

    const std::uint32_t gate_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->linexe_arena_layout.gate_code_base;
    const repiu::hle::GlideExportGate* glide_export =
        repiu::hle::DecodeGlideGate(context->glide_gate_plan, gate_offset);
    if (glide_export == nullptr)
    {
        return false;
    }

    ++context->glide_gate_entry_count;
    context->glide_backend.PumpEvents();
    context->glide_gate_ordinal = glide_export->ordinal;
    context->glide_gate_argument_bytes = glide_export->argument_byte_count;
    std::memset(context->glide_gate_name,
                0,
                sizeof(context->glide_gate_name));
    std::strncpy(context->glide_gate_name,
                 glide_export->name.c_str(),
                 sizeof(context->glide_gate_name) - 1U);
    context->glide_gate_esp = win32_context->Esp;
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context,
                             stack,
                             sizeof(context->glide_gate_stack)))
    {
        std::memcpy(context->glide_gate_stack,
                    stack,
                    sizeof(context->glide_gate_stack));
    }
    if (glide_export->ordinal < context->glide_call_counts.size())
    {
        const std::size_t ordinal = glide_export->ordinal;
        if (context->glide_call_counts[ordinal]++ == 0U)
        {
            context->glide_call_names[ordinal] = glide_export->name;
            std::copy(std::begin(context->glide_gate_stack),
                      std::end(context->glide_gate_stack),
                      context->glide_first_stacks[ordinal].begin());
            // Audit diagnostic (env-gated, off by default): emit one line the
            // first time each ordinal is ever called. Bounded by the export
            // count (<=97 lines), so it survives both the 96-entry gate-entry
            // log cap and the timeout path that skips the exit summary --
            // the only way to enumerate the reached API set on a timed run.
            static const bool call_audit_enabled =
                std::getenv("REPIU_GLIDE_CALL_AUDIT") != nullptr;
            if (call_audit_enabled)
            {
                fprintf(stderr,
                        "[repiu-glide-audit] first-call ordinal=%u name=%s"
                        " args=%08X %08X %08X %08X %08X %08X %08X\n",
                        glide_export->ordinal, glide_export->name.c_str(),
                        context->glide_gate_stack[1],
                        context->glide_gate_stack[2],
                        context->glide_gate_stack[3],
                        context->glide_gate_stack[4],
                        context->glide_gate_stack[5],
                        context->glide_gate_stack[6],
                        context->glide_gate_stack[7]);
            }
            if (context->shared_live_telemetry != nullptr)
            {
                volatile long* milestone = nullptr;
                if (glide_export->name == "_GRSSTWINOPEN@28")
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_window_gate_milestone;
                }
                else if (glide_export->name ==
                         "_GRTEXDOWNLOADMIPMAPLEVEL@32")
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_texture_milestone;
                }
                else if (glide_export->name.rfind("_GRDRAW", 0U) == 0U)
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_draw_milestone;
                }
                else if (glide_export->name == "_GRBUFFERSWAP@4")
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_swap_milestone;
                }
                if (milestone != nullptr)
                {
                    InterlockedExchange(milestone, 1L);
                }
            }
        }
    }
    // Task 255 R3 observation (env-gated, off by default): dump the real
    // arguments of the texture/combine gates during content draws so the actual
    // format, dimensions, and combine mode can be confirmed before implementing
    // texture decode/upload/sampling. The gate stack mirror holds only 8 dwords,
    // so read directly from the guest stack for the wider download call.
    {
        static const bool tex_diagnostic_enabled =
            std::getenv("REPIU_GLIDE_TEX_DIAG") != nullptr;
        if (tex_diagnostic_enabled &&
            (glide_export->name == "_GRTEXDOWNLOADMIPMAPLEVEL@32" ||
             glide_export->name == "_GRTEXSOURCE@16" ||
             glide_export->name == "_GRTEXCOMBINE@28" ||
             glide_export->name == "_GRCOLORCOMBINE@20" ||
             glide_export->name == "_GRALPHABLENDFUNCTION@16" ||
             glide_export->name == "_GRALPHATESTFUNCTION@4" ||
             glide_export->name == "_GRALPHACOMBINE@20"))
        {
            static long tex_diag_count = 0;
            const long diag_index = InterlockedIncrement(&tex_diag_count);
            if (diag_index <= 256)
            {
                std::uint32_t args[9] = {};
                const auto* guest_stack =
                    reinterpret_cast<const std::uint32_t*>(
                        static_cast<std::uintptr_t>(win32_context->Esp));
                if (IsGuestRangeReadable(context, guest_stack, sizeof(args)))
                {
                    std::memcpy(args, guest_stack, sizeof(args));
                }
                fprintf(stderr,
                        "[repiu-live-debug] tex-diag #%ld %s args=%08X %08X %08X"
                        " %08X %08X %08X %08X %08X\n",
                        diag_index, glide_export->name.c_str(),
                        args[1], args[2], args[3], args[4],
                        args[5], args[6], args[7], args[8]);
            }
        }
    }
    // Task 246: gate traffic is tiny (tens of calls), so log every entry to
    // pin which call leaks its stack frame (an unhandled entry resumes the
    // caller without the stdcall cleanup, offsetting ESP for the rest of the
    // frame — the confirmed zero-EIP mechanism at 0x0304ED35).
    {
        static long gate_entry_log_count = 0;
        const long entry_index = InterlockedIncrement(&gate_entry_log_count);
        if (entry_index <= 96)
        {
            fprintf(stderr,
                    "[repiu-live-debug] glide gate entry #%ld ordinal=%u"
                    " name=%s ret=0x%08X esp=0x%08X\n",
                    entry_index, glide_export->ordinal,
                    glide_export->name.c_str(), context->glide_gate_stack[0],
                    static_cast<std::uint32_t>(win32_context->Esp));
        }
    }
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ordinal,
            static_cast<long>(glide_export->ordinal));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_esp,
            static_cast<long>(win32_context->Esp));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ebx,
            static_cast<long>(win32_context->Ebx));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ecx,
            static_cast<long>(win32_context->Ecx));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_edx,
            static_cast<long>(win32_context->Edx));
        for (std::size_t index = 0; index < 8U; ++index)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->glide_gate_stack[index],
                static_cast<long>(context->glide_gate_stack[index]));
        }
    }
    const auto reject_gate = [context, win32_context,
                              glide_export](const char* reason) {
        static long gate_reject_log_count = 0;
        const long reject_index =
            InterlockedIncrement(&gate_reject_log_count);
        if (reject_index <= 16)
        {
            fprintf(stderr,
                    "[repiu-live-debug] glide gate rejected #%ld ordinal=%u"
                    " name=%s reason=%s ret=0x%08X esp=0x%08X\n",
                    reject_index, glide_export->ordinal,
                    glide_export->name.c_str(), reason,
                    context->glide_gate_stack[0],
                    static_cast<std::uint32_t>(win32_context->Esp));
        }
        return false;
    };
    const std::uint32_t return_address = context->glide_gate_stack[0];
    if (!IsGuestInstructionPointer(context, return_address))
    {
        return reject_gate("return-address-not-guest");
    }
    const repiu::hle::GlideSignature* signature =
        repiu::hle::FindGlideSignature(glide_export->name);
    if (signature == nullptr ||
        signature->argument_byte_count !=
            glide_export->argument_byte_count)
    {
        return reject_gate("signature-mismatch");
    }
    if (glide_export->name == "_GRHINTS@8")
    {
        // Glide documents this call as optimization advice. Preserve the
        // observed stdcall ABI while the renderer has no verified hint policy.
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRGLIDEINIT@0")
    {
        context->glide_state.initialized = true;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTQUERYHARDWARE@4")
    {
        void* configuration = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
        if (!WriteGuestUInt32(context, configuration, 1U))
        {
            return false;
        }
        ++context->glide_gate_handled_count;
        win32_context->Eax = 1U;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTSELECT@4" &&
        context->glide_gate_stack[1] == 0U)
    {
        context->glide_state.selected_board = 0U;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTWINOPEN@28")
    {
        const std::uint32_t window = context->glide_gate_stack[1];
        const std::uint32_t resolution = context->glide_gate_stack[2];
        const std::uint32_t refresh = context->glide_gate_stack[3];
        const std::uint32_t color_format = context->glide_gate_stack[4];
        const std::uint32_t origin = context->glide_gate_stack[5];
        const std::uint32_t color_buffers = context->glide_gate_stack[6];
        const std::uint32_t auxiliary_buffers =
            context->glide_gate_stack[7];
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        const bool mode_supported = window == 0U && refresh == 0U &&
            repiu::hle::DecodeGlideResolution(
                resolution, &width, &height);
        bool opened = false;
        try
        {
            opened = mode_supported &&
                context->glide_backend.OpenWindowed(
                    width, height, color_buffers, auxiliary_buffers, origin);
        }
        catch (const std::exception& e)
        {
            fprintf(stderr, "[repiu-live-debug] _GRSSTWINOPEN@28 caught standard exception: %s\n", e.what());
        }
        catch (...)
        {
            fprintf(stderr, "[repiu-live-debug] _GRSSTWINOPEN@28 caught unknown exception\n");
        }
        context->glide_backend_message = context->glide_backend.message();
        fprintf(stderr, "[repiu-live-debug] _GRSSTWINOPEN@28: mode_supported=%d opened=%d message=%s\n",
                mode_supported ? 1 : 0, opened ? 1 : 0, context->glide_backend_message.c_str());
        if (opened)
        {
            ++context->glide_window_open_count;
            if (context->shared_live_telemetry != nullptr)
            {
                InterlockedExchange(
                    &context->shared_live_telemetry
                         ->glide_window_open_milestone,
                    1L);
            }
            context->glide_logical_width = width;
            context->glide_logical_height = height;
            context->glide_state.window_open = true;
            context->glide_state.width = width;
            context->glide_state.height = height;
            context->glide_state.color_format = color_format;
            context->glide_state.origin = origin;
            context->glide_state.color_buffer_count = color_buffers;
            context->glide_state.auxiliary_buffer_count = auxiliary_buffers;
        }
        win32_context->Eax = opened ? 1U : 0U;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 8U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTWINCLOSE@0")
    {
        context->glide_backend.Close();
        context->glide_state.window_open = false;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTSCREENWIDTH@0" ||
        glide_export->name == "_GRSSTSCREENHEIGHT@0")
    {
        win32_context->Eax = glide_export->name == "_GRSSTSCREENWIDTH@0"
            ? context->glide_state.width
            : context->glide_state.height;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXTEXTUREMEMREQUIRED@8")
    {
        repiu::hle::GlideTextureInfo info;
        const void* info_address = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(context->glide_gate_stack[2]));
        std::uint32_t required_bytes = 0;
        if (!IsGuestRangeReadable(context, info_address, sizeof(info)) ||
            !repiu::hle::CalculateGlideTextureMemoryRequired(
                context->glide_gate_stack[1], info, &required_bytes))
        {
            return reject_gate("set-state-unreadable-memory");
        }
        ++context->glide_gate_handled_count;
        win32_context->Eax = required_bytes;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXDOWNLOADMIPMAPLEVEL@32")
    {
        // R3: decode and upload the texel image so a later grTexSource can bind
        // it. Args (from the guest stack, the mirror holds only 8 dwords):
        // tmu, startAddress, thisLod, largeLod, aspectRatio, format, evenOdd,
        // data. Preserve the stdcall ABI regardless of upload success.
        std::uint32_t args[9] = {};
        const auto* guest_stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp));
        if (IsGuestRangeReadable(context, guest_stack, sizeof(args)))
        {
            std::memcpy(args, guest_stack, sizeof(args));
            const std::uint32_t start_address = args[2];
            const std::uint32_t large_lod = args[4];
            const std::uint32_t aspect_ratio = args[5];
            const std::uint32_t format = args[6];
            const auto* data = reinterpret_cast<const std::uint8_t*>(
                static_cast<std::uintptr_t>(args[8]));
            repiu::hle::GlideTextureDimensions dimensions;
            const bool dimensions_ok =
                repiu::hle::CalculateGlideTextureDimensions(
                    large_lod, aspect_ratio, &dimensions);
            // Format census (env-gated): count every download per format and
            // record why one was dropped. Palette formats (P_8, AP_88) decode
            // without their table because grTexDownloadTable is still a no-op,
            // and the NCC formats are refused outright -- this separates "the
            // game never uses that format" from "we silently drop it".
            static const bool format_census_enabled =
                std::getenv("REPIU_GLIDE_TEX_CENSUS") != nullptr;
            if (format_census_enabled && format < 16U)
            {
                static long format_counts[16] = {};
                const long seen = InterlockedIncrement(&format_counts[format]);
                if (seen <= 3)
                {
                    fprintf(stderr,
                            "[repiu-tex-census] format=%u lod=%u aspect=%u"
                            " dims=%s%ux%u acceptable=%d addr=0x%08X seen=%ld\n",
                            format, large_lod, aspect_ratio,
                            dimensions_ok ? "" : "INVALID ",
                            dimensions_ok ? dimensions.width : 0U,
                            dimensions_ok ? dimensions.height : 0U,
                            repiu::hle::IsGlideTextureFormatAcceptable(format)
                                ? 1 : 0,
                            start_address, seen);
                }
            }
            if (dimensions_ok)
            {
                const std::size_t bytes_per_texel = format >= 8U ? 2U : 1U;
                const std::size_t source_size =
                    static_cast<std::size_t>(dimensions.width) *
                    dimensions.height * bytes_per_texel;
                if (source_size != 0U &&
                    IsGuestRangeReadable(context, data, source_size))
                {
                    const bool stored = context->glide_backend.StoreTexture(
                        start_address, format, large_lod, aspect_ratio, data,
                        source_size);
                    context->glide_backend_message =
                        context->glide_backend.message();
                    if (format_census_enabled && !stored)
                    {
                        static long store_fail_log = 0;
                        if (InterlockedIncrement(&store_fail_log) <= 12)
                        {
                            fprintf(stderr,
                                    "[repiu-tex-census] STORE FAILED format=%u"
                                    " %ux%u addr=0x%08X reason=%s\n",
                                    format, dimensions.width, dimensions.height,
                                    start_address,
                                    context->glide_backend_message.c_str());
                        }
                    }

                    // Diagnostic BMP Dump
                    const char* env_dump = std::getenv("REPIU_DUMP_TEXTURE_BMP");
                    if (env_dump != nullptr && std::string_view(env_dump) == "1")
                    {
                        std::vector<std::uint8_t> rgba8;
                        if (repiu::hle::DecodeGlideTextureToRgba8(
                                format, dimensions.width, dimensions.height, data,
                                source_size, nullptr, &rgba8))
                        {
                            DumpTextureToBmp(start_address, format, dimensions.width,
                                             dimensions.height, rgba8);
                        }
                    }
                }
            }
        }
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 9U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXSOURCE@16")
    {
        // R3: select the current texture (args: tmu, startAddress, evenOdd,
        // GrTexInfo*). A missing texture simply leaves the previous binding.
        context->glide_backend.SourceTexture(context->glide_gate_stack[2]);
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t) +
            glide_export->argument_byte_count;
        return true;
    }
    if (glide_export->name == "_GRTEXCOMBINE@28" ||
        glide_export->name == "_GRTEXCLAMPMODE@12" ||
        glide_export->name == "_GRTEXFILTERMODE@12" ||
        glide_export->name == "_GRTEXMIPMAPMODE@12")
    {
        // Texture sampler parameters stay within the rendering boundary; the
        // observed filter/clamp/mipmap modes are handled by the backend texture
        // defaults for now.
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t) +
            glide_export->argument_byte_count;
        return true;
    }
    if (glide_export->name == "_GRTEXMINADDRESS@4" &&
        context->glide_gate_stack[1] == 0U)
    {
        RecordGlideTextureGateTrace(context, win32_context, *glide_export, return_address, 0U, false);
        ++context->glide_gate_handled_count;
        win32_context->Eax = 0U;
        win32_context->Eip = return_address;
        // stdcall: pop return address and the one dword argument. fxTMInit is
        // the sole caller and issues `push arg; call grTexMin; push arg; call
        // grTexMax; mov eax,[esp]` with no caller-side cleanup, so the gate
        // must clean the argument or the reload reads a leftover (NULL gc).
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXMAXADDRESS@4" &&
        context->glide_gate_stack[1] == 0U)
    {
        std::uint32_t maximum_address = 0;
        if (!repiu::hle::CalculateGlideTextureMaxAddress(
                context->glide_state.texture_memory_bytes,
                &maximum_address))
        {
            return reject_gate("texture-max-address-calculation-failure");
        }
        RecordGlideTextureGateTrace(context, win32_context, *glide_export, return_address, maximum_address, true);
        ++context->glide_gate_handled_count;
        win32_context->Eax = maximum_address;
        win32_context->Eip = return_address;
        // stdcall: pop return address and the one dword argument (see the
        // grTexMinAddress note above; fxTMInit relies on callee cleanup).
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCOLORMASK@8")
    {
        const bool rgb = context->glide_gate_stack[1] != 0U;
        const bool alpha = context->glide_gate_stack[2] != 0U;
        if (!context->glide_backend.SetColorMask(rgb, alpha))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("color-mask-backend-failure");
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRRENDERBUFFER@4")
    {
        if (!context->glide_backend.SetRenderBuffer(
                context->glide_gate_stack[1]))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("render-buffer-backend-failure");
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHBIASLEVEL@4")
    {
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHMASK@4")
    {
        if (!context->glide_backend.SetDepthMask(
                context->glide_gate_stack[1] != 0U))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("depth-mask-backend-failure");
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHBUFFERMODE@4")
    {
        if (!context->glide_backend.SetDepthBufferMode(
                context->glide_gate_stack[1]))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("depth-buffer-mode-backend-failure");
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRLFBWRITECOLORFORMAT@4")
    {
        context->glide_state.lfb_write_color_format =
            context->glide_gate_stack[1];
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRALPHACOMBINE@20")
    {
        repiu::hle::GlideAlphaCombineState state;
        state.function = context->glide_gate_stack[1];
        state.factor = context->glide_gate_stack[2];
        state.local = context->glide_gate_stack[3];
        state.other = context->glide_gate_stack[4];
        state.invert = context->glide_gate_stack[5] != 0U;
        state.valid = true;
        if (!context->glide_backend.SetAlphaCombine(state))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            // Same retain policy as color combine (design 237, Task 247):
            // an equation the GLSL translator merely does not support yet
            // must still preserve the stdcall ABI, or the unhandled gate
            // leaks its 24-byte frame and the caller epilogue returns to 0
            // (the Task 245/246 zero-EIP root cause).
            if (context->glide_backend_message !=
                "unsupported Glide alpha-combine equation")
            {
                return reject_gate("alpha-combine-backend-failure");
            }
        }
        context->glide_state.alpha_combine = state;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 6U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCOLORCOMBINE@20")
    {
        repiu::hle::GlideColorCombineState state;
        state.function = context->glide_gate_stack[1];
        state.factor = context->glide_gate_stack[2];
        state.local = context->glide_gate_stack[3];
        state.other = context->glide_gate_stack[4];
        state.invert = context->glide_gate_stack[5] != 0U;
        state.valid = true;
        // R3: function 3 (GR_COMBINE_FUNCTION_SCALE_OTHER) with other=1
        // (GR_COMBINE_OTHER_TEXTURE) routes the fragment output to the sourced
        // texture; function 1 (LOCAL) is the iterated vertex color. Toggle the
        // backend texture-combine path accordingly (observed content draws use
        // function 3, init uses function 1).
        constexpr std::uint32_t kCombineFunctionScaleOther = 3U;
        constexpr std::uint32_t kCombineOtherTexture = 1U;
        context->glide_backend.SetTextureCombineEnabled(
            state.function == kCombineFunctionScaleOther &&
            state.other == kCombineOtherTexture);
        if (!context->glide_backend.SetColorCombine(state))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            if (context->glide_backend_message !=
                "unsupported Glide color-combine equation")
            {
                return reject_gate("color-combine-backend-failure");
            }
        }
        context->glide_state.color_combine = state;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 6U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRALPHABLENDFUNCTION@16")
    {
        repiu::hle::GlideAlphaBlendState state;
        state.rgb_source = context->glide_gate_stack[1];
        state.rgb_destination = context->glide_gate_stack[2];
        state.alpha_source = context->glide_gate_stack[3];
        state.alpha_destination = context->glide_gate_stack[4];
        state.valid = true;
        if (!context->glide_backend.SetAlphaBlend(state))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            // Same retain policy as the combine gates (design 237): a blend
            // function the backend does not express yet must still preserve
            // the stdcall ABI, or the unhandled gate leaks its frame (the
            // Task 246 corruption chain).
            if (context->glide_backend_message !=
                "unsupported Glide alpha-blend function")
            {
                return reject_gate("alpha-blend-backend-failure");
            }
        }
        context->glide_state.alpha_blend = state;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 5U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRALPHATESTFUNCTION@4")
    {
        const std::uint32_t function = context->glide_gate_stack[1];
        if (!context->glide_backend.SetAlphaTestFunction(function))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("alpha-test-function-backend-failure");
        }
        context->glide_state.alpha_test_function = function;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHBUFFERFUNCTION@4")
    {
        const std::uint32_t function = context->glide_gate_stack[1];
        if (!context->glide_backend.SetDepthBufferFunction(function))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("depth-buffer-function-backend-failure");
        }
        context->glide_state.depth_buffer_function = function;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRFOGMODE@4")
    {
        const std::uint32_t mode = context->glide_gate_stack[1];
        if (!context->glide_backend.SetFogMode(mode))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("fog-mode-backend-failure");
        }
        context->glide_state.fog_mode = mode;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCLIPWINDOW@16")
    {
        const std::uint32_t min_x = context->glide_gate_stack[1];
        const std::uint32_t min_y = context->glide_gate_stack[2];
        const std::uint32_t max_x = context->glide_gate_stack[3];
        const std::uint32_t max_y = context->glide_gate_stack[4];
        if (!context->glide_backend.SetClipWindow(
                min_x, min_y, max_x, max_y))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("clip-window-backend-failure");
        }
        context->glide_state.clip_min_x = min_x;
        context->glide_state.clip_min_y = min_y;
        context->glide_state.clip_max_x = max_x;
        context->glide_state.clip_max_y = max_y;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 5U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCULLMODE@4")
    {
        const std::uint32_t mode = context->glide_gate_stack[1];
        if (!context->glide_backend.SetCullMode(mode))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("cull-mode-backend-failure");
        }
        context->glide_state.cull_mode = mode;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRGLIDEGETSTATE@4")
    {
        repiu::hle::GlideStateImage image;
        void* output = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
            context->glide_gate_stack[1]));
        if (!repiu::hle::BuildGlideStateImage(context->glide_state, &image) ||
            !WriteGuestBytes(context, output, image.data(), image.size()))
        {
            return reject_gate("get-state-serialization-failure");
        }
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRGLIDESETSTATE@4")
    {
        repiu::hle::GlideStateImage image;
        const void* input = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
        repiu::hle::GlideLogicalState restored = context->glide_state;
        if (!IsGuestRangeReadable(context, input, image.size()))
        {
            return reject_gate("set-state-unreadable-memory");
        }
        std::memcpy(image.data(), input, image.size());
        if (!repiu::hle::ParseGlideStateImage(image, &restored))
        {
            return reject_gate("set-state-deserialization-failure");
        }
        context->glide_state = restored;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDITHERMODE@4")
    {
        const std::uint32_t mode = context->glide_gate_stack[1];
        if (!context->glide_backend.SetDitherMode(mode))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return reject_gate("dither-mode-backend-failure");
        }
        context->glide_state.dither_mode = mode;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRBUFFERCLEAR@12")
    {
        const std::uint32_t color = context->glide_gate_stack[1];
        const std::uint32_t alpha = context->glide_gate_stack[2];
        const std::uint32_t depth = context->glide_gate_stack[3];
        if (!context->glide_backend.BufferClear(color, alpha, depth))
        {
            context->glide_backend_message = context->glide_backend.message();
            return reject_gate("buffer-clear-backend-failure");
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 4U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRBUFFERSWAP@4")
    {
        const std::uint32_t swap_interval = context->glide_gate_stack[1];
        if (!context->glide_backend.BufferSwap(swap_interval))
        {
            context->glide_backend_message = context->glide_backend.message();
            return reject_gate("buffer-swap-backend-failure");
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRBUFFERNUMPENDING@0")
    {
        ++context->glide_gate_handled_count;
        win32_context->Eax = 0U;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDRAWLINE@8")
    {
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDRAWPOINT@4")
    {
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDRAWTRIANGLE@12")
    {
        Win32GlideTriangleObservation& triangle =
            context->glide_first_triangle;
        if (!triangle.valid)
        {
            triangle.valid = true;
            for (std::size_t index = 0; index < 3U; ++index)
            {
                triangle.pointers[index] = context->glide_gate_stack[index + 1U];
                const auto* vertex = reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(triangle.pointers[index]));
                triangle.pointer_readable[index] = IsGuestRangeReadable(
                    context, vertex, sizeof(triangle.dwords[index]));
                if (triangle.pointer_readable[index])
                {
                    std::memcpy(triangle.dwords[index], vertex,
                                sizeof(triangle.dwords[index]));
                }
            }
            fprintf(stderr,
                    "[repiu-live] Glide first triangle vertices: %08X/%d %08X/%d %08X/%d\\n",
                    triangle.pointers[0], triangle.pointer_readable[0],
                    triangle.pointers[1], triangle.pointer_readable[1],
                    triangle.pointers[2], triangle.pointer_readable[2]);
            for (std::size_t index = 0; index < 3U; ++index)
            {
                fprintf(stderr, "[repiu-live] Glide first triangle vertex %u dwords:",
                        static_cast<unsigned>(index));
                for (std::uint32_t dword : triangle.dwords[index])
                {
                    fprintf(stderr, " %08X", dword);
                }
                fprintf(stderr, "\n");
            }
        }
        const std::uint32_t sequence = ++context->glide_triangle_trace_count;
        Win32GlideTriangleTraceEntry& trace = context->glide_triangle_trace[
            (sequence - 1U) % kWin32GlideTriangleTraceCapacity];
        trace = Win32GlideTriangleTraceEntry{};
        trace.valid = true;
        trace.sequence = sequence;
        for (std::size_t index = 0; index < 3U; ++index)
        {
            trace.pointers[index] = context->glide_gate_stack[index + 1U];
            const auto* vertex = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(trace.pointers[index]));
            trace.pointer_readable[index] = IsGuestRangeReadable(
                context, vertex, sizeof(trace.dwords[index]));
            if (trace.pointer_readable[index])
            {
                std::memcpy(trace.dwords[index], vertex,
                            sizeof(trace.dwords[index]));
            }
        }
        if (sequence > kWin32GlideTriangleTraceCapacity)
        {
            context->glide_triangle_trace_wrapped = true;
        }
        // Decode the confirmed 60-byte 2-TMU GrVertex fields: x/y (dwords 0/1),
        // iterated color r/g/b/a in [0..255] (dwords 3/4/5/7), and TMU0 texture
        // coordinates sow/tow (dwords 9/10). Color is normalized to [0,1].
        GlideDrawVertex vertices[3] = {};
        for (std::size_t index = 0; index < 3U; ++index)
        {
            if (!trace.pointer_readable[index]) return reject_gate("compact-triangle-unreadable-vertex");
            float fields[15] = {};
            std::memcpy(fields, trace.dwords[index], sizeof(fields));
            GlideDrawVertex& vertex = vertices[index];
            vertex.x = fields[0];
            vertex.y = fields[1];
            vertex.r = fields[3] / 255.0F;
            vertex.g = fields[4] / 255.0F;
            vertex.b = fields[5] / 255.0F;
            vertex.a = fields[7] / 255.0F;
            vertex.s = fields[9];
            vertex.t = fields[10];
        }
        // Draw diagnostic (env-gated): the swap-driven pixel sampler only fires
        // on selected swap indices, so it can miss the content phase entirely.
        // Log the decoded vertices and read the back buffer straight after the
        // draw, which separates "geometry is degenerate", "state kills the
        // fragment", and "it draws but something later erases it".
        static const bool draw_diagnostic_enabled =
            std::getenv("REPIU_GLIDE_DRAW_DIAG") != nullptr;
        // A missing full-screen background would never appear in a first-N
        // sample if the game submits it after the UI text, so also diagnose any
        // triangle large enough to be background geometry regardless of when it
        // arrives.
        const float min_x = std::min({vertices[0].x, vertices[1].x, vertices[2].x});
        const float max_x = std::max({vertices[0].x, vertices[1].x, vertices[2].x});
        const float min_y = std::min({vertices[0].y, vertices[1].y, vertices[2].y});
        const float max_y = std::max({vertices[0].y, vertices[1].y, vertices[2].y});
        const bool is_large = (max_x - min_x) >= 100.0F && (max_y - min_y) >= 100.0F;
        static long large_diag_count = 0;
        const bool diagnose_large =
            is_large && InterlockedIncrement(&large_diag_count) <= 12;
        std::size_t before_non_black = 0;
        const bool diagnose_this_draw =
            draw_diagnostic_enabled && (sequence <= 12U || diagnose_large);
        if (diagnose_this_draw)
        {
            std::vector<std::uint8_t> before;
            if (context->glide_backend.ReadbackFramebuffer(
                    context->glide_state.width, context->glide_state.height,
                    &before))
            {
                for (std::size_t i = 0; i + 3U < before.size(); i += 4U)
                {
                    if (before[i] > 8U || before[i + 1U] > 8U ||
                        before[i + 2U] > 8U)
                    {
                        ++before_non_black;
                    }
                }
            }
        }
        if (!context->glide_backend.DrawTriangle(vertices[0], vertices[1], vertices[2]))
        {
            context->glide_backend_message = context->glide_backend.message();
            return reject_gate("compact-triangle-backend-failure");
        }
        if (diagnose_this_draw)
        {
            std::vector<std::uint8_t> after;
            std::size_t after_non_black = 0;
            if (context->glide_backend.ReadbackFramebuffer(
                    context->glide_state.width, context->glide_state.height,
                    &after))
            {
                for (std::size_t i = 0; i + 3U < after.size(); i += 4U)
                {
                    if (after[i] > 8U || after[i + 1U] > 8U ||
                        after[i + 2U] > 8U)
                    {
                        ++after_non_black;
                    }
                }
            }
            fprintf(stderr,
                    "[repiu-live-debug] tri #%u xy=(%.2f,%.2f)(%.2f,%.2f)"
                    "(%.2f,%.2f) rgba0=(%.3f,%.3f,%.3f,%.3f) st0=(%.2f,%.2f)"
                    " combine=%u/other=%u texEnabled=%d nonblack %zu->%zu\n",
                    sequence, vertices[0].x, vertices[0].y, vertices[1].x,
                    vertices[1].y, vertices[2].x, vertices[2].y,
                    vertices[0].r, vertices[0].g, vertices[0].b,
                    vertices[0].a, vertices[0].s, vertices[0].t,
                    context->glide_state.color_combine.function,
                    context->glide_state.color_combine.other,
                    context->glide_backend.is_texture_combine_enabled() ? 1 : 0,
                    before_non_black, after_non_black);
        }
        // Triangle census (env-gated): aggregate every draw by the combine mode
        // and size bucket it used. The first-N sample cannot answer "why is the
        // background missing" because the background may be many small tiles
        // submitted after the UI text -- a histogram over all draws can.
        {
            static const bool tri_census_enabled =
                std::getenv("REPIU_GLIDE_TRI_CENSUS") != nullptr;
            if (tri_census_enabled)
            {
                struct Bucket
                {
                    std::uint32_t function;
                    std::uint32_t other;
                    bool textured;
                    long count;
                    float max_w;
                    float max_h;
                };
                static Bucket buckets[16] = {};
                static long bucket_count = 0;
                static long census_draws = 0;
                const std::uint32_t fn =
                    context->glide_state.color_combine.function;
                const std::uint32_t ot = context->glide_state.color_combine.other;
                const bool textured =
                    context->glide_backend.is_texture_combine_enabled();
                bool found = false;
                for (long b = 0; b < bucket_count; ++b)
                {
                    if (buckets[b].function == fn && buckets[b].other == ot &&
                        buckets[b].textured == textured)
                    {
                        ++buckets[b].count;
                        buckets[b].max_w = std::max(buckets[b].max_w, max_x - min_x);
                        buckets[b].max_h = std::max(buckets[b].max_h, max_y - min_y);
                        found = true;
                        break;
                    }
                }
                if (!found && bucket_count < 16)
                {
                    buckets[bucket_count] = {fn, ot, textured, 1,
                                             max_x - min_x, max_y - min_y};
                    ++bucket_count;
                }
                if (++census_draws % 400 == 0 && census_draws <= 4000)
                {
                    fprintf(stderr,
                            "[repiu-tri-census] after %ld draws:\n", census_draws);
                    for (long b = 0; b < bucket_count; ++b)
                    {
                        fprintf(stderr,
                                "    combine fn=%u other=%u textured=%d"
                                " count=%ld max=%.0fx%.0f\n",
                                buckets[b].function, buckets[b].other,
                                buckets[b].textured ? 1 : 0, buckets[b].count,
                                buckets[b].max_w, buckets[b].max_h);
                    }
                }
            }
        }
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 4U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDRAWPLANARPOLYGON@12" ||
        glide_export->name == "_GRDRAWPOLYGON@12")
    {
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 4U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDRAWPLANARPOLYGONVERTEXLIST@8")
    {
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCONSTANTCOLORVALUE@4")
    {
        // R4: retain the constant color so a later CONSTANT combine source can
        // read it. Observed value during the content phase is 0xFFFFFFFF.
        context->glide_state.constant_color = context->glide_gate_stack[1];
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRLFBLOCK@24")
    {
        // grLfbLock(type, buffer, writeMode, origin, pixelPipeline, info*).
        // The observed content-phase lock is (WRITE_ONLY, BACKBUFFER, 565,
        // UPPER_LEFT, FXTRUE), which is exactly what this path implements.
        const std::uint32_t type = context->glide_gate_stack[1];
        const std::uint32_t buffer = context->glide_gate_stack[2];
        const std::uint32_t write_mode = context->glide_gate_stack[3];
        const std::uint32_t origin = context->glide_gate_stack[4];
        const std::uint32_t info_pointer = context->glide_gate_stack[6];
        auto* info = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(info_pointer));

        const auto fail_lock = [&](const char* reason) {
            // Retain policy (design 237): an unsupported but ABI-valid lock
            // returns FXFALSE rather than leaking the frame through a reject.
            static long lfb_decline_log_count = 0;
            const long index = InterlockedIncrement(&lfb_decline_log_count);
            if (index <= 16)
            {
                fprintf(stderr,
                        "[repiu-live-debug] grLfbLock declined #%ld reason=%s"
                        " type=%u buffer=%u writeMode=%u origin=%u\n",
                        index, reason, type, buffer, write_mode, origin);
            }
            ++context->glide_gate_handled_count;
            win32_context->Eax = 0U;
            win32_context->Eip = return_address;
            win32_context->Esp += 7U * sizeof(std::uint32_t);
            return true;
        };

        if (!IsGuestRangeWritable(context, info,
                                  repiu::hle::kGlideLfbInfoByteCount))
        {
            return reject_gate("lfb-lock-info-pointer-unwritable");
        }
        if (write_mode != repiu::hle::kGlideLfbWriteMode565 ||
            buffer != repiu::hle::kGlideBufferBackBuffer)
        {
            return fail_lock("unsupported-writemode-or-buffer");
        }
        const std::uint32_t width = context->glide_state.width;
        const std::uint32_t height = context->glide_state.height;
        if (!context->glide_lfb_surface.Resize(width, height))
        {
            return fail_lock("surface-resize-failure");
        }
        if (!context->glide_lfb_surface.BeginLock(type, buffer, write_mode,
                                                  origin))
        {
            return fail_lock("lock-already-outstanding");
        }
        // Seed the staging surface from the current render target for *every*
        // lock, not just read locks. On real hardware the LFB is the live
        // framebuffer, so a write lock that touches only some pixels leaves the
        // rest untouched. Handing out a zero-filled buffer instead makes unlock
        // blit black over everything already drawn -- observed erasing the
        // triangles submitted immediately before the lock.
        {
            std::vector<std::uint8_t> rgba8;
            const bool read_ok = context->glide_backend.ReadbackFramebuffer(
                width, height, &rgba8);
            bool encode_ok = false;
            if (read_ok)
            {
                encode_ok = repiu::hle::EncodeRgba8ToGlideLfb565(
                    rgba8.data(), rgba8.size(), width, height,
                    context->glide_lfb_surface.pixels(),
                    context->glide_lfb_surface.byte_count());
            }
            static long seed_log_count = 0;
            const long seed_index = InterlockedIncrement(&seed_log_count);
            if (seed_index <= 4)
            {
                std::size_t seed_non_black = 0;
                for (std::size_t i = 0; i + 3U < rgba8.size(); i += 4U)
                {
                    if (rgba8[i] > 8U || rgba8[i + 1U] > 8U ||
                        rgba8[i + 2U] > 8U)
                    {
                        ++seed_non_black;
                    }
                }
                fprintf(stderr,
                        "[repiu-live-debug] grLfbLock seed #%ld read=%d"
                        " encode=%d framebuffer non-black=%zu\n",
                        seed_index, read_ok ? 1 : 0, encode_ok ? 1 : 0,
                        seed_non_black);
            }
        }

        // The size field is caller-supplied; echo it back but record a mismatch
        // so an unexpected GrLfbInfo_t layout becomes visible instead of silent.
        std::uint32_t caller_size = 0;
        std::memcpy(&caller_size, info, sizeof(caller_size));
        {
            // Dump what the caller staged in the struct before we touch it.
            // Glide 2.4's GrLfbInfo_t starts with a caller-set `size`; PIU
            // leaves offset 0 at zero, so this dump is what distinguishes "the
            // game simply never sets size" from "this build's GrLfbInfo_t has
            // no size field and every field we write is off by one dword".
            static long lfb_size_log_count = 0;
            const long index = InterlockedIncrement(&lfb_size_log_count);
            if (index <= 4)
            {
                std::uint32_t before[8] = {};
                const auto* raw = reinterpret_cast<const std::uint32_t*>(
                    static_cast<std::uintptr_t>(info_pointer));
                if (IsGuestRangeReadable(context, raw, sizeof(before)))
                {
                    std::memcpy(before, raw, sizeof(before));
                }
                fprintf(stderr,
                        "[repiu-live-debug] grLfbLock GrLfbInfo_t caller size=%u"
                        " (expected %u) pre-call dwords="
                        "%08X %08X %08X %08X %08X %08X %08X %08X\n",
                        caller_size,
                        repiu::hle::kGlideLfbInfoExpectedSize,
                        before[0], before[1], before[2], before[3],
                        before[4], before[5], before[6], before[7]);
            }
        }

        std::uint8_t image[repiu::hle::kGlideLfbInfoByteCount] = {};
        const auto staging_pointer = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(
                context->glide_lfb_surface.pixels()));
        // grLfbLock owns every output field, `size` included: it reports the
        // GrLfbInfo_t layout it actually filled. Echoing the caller's value back
        // (PIU leaves it 0) would tell the guest nothing was written.
        if (!repiu::hle::BuildGlideLfbInfoImage(
                repiu::hle::kGlideLfbInfoExpectedSize, staging_pointer,
                context->glide_lfb_surface.stride_in_bytes(), write_mode,
                origin, image, sizeof(image)) ||
            !WriteGuestBytes(context, info, image, sizeof(image)))
        {
            context->glide_lfb_surface.EndLock();
            return fail_lock("info-write-failure");
        }

        ++context->glide_lfb_lock_count;
        if (context->glide_lfb_lock_count <= 4U)
        {
            fprintf(stderr,
                    "[repiu-live-debug] grLfbLock granted #%u type=%u"
                    " buffer=%u writeMode=%u origin=%u lfbPtr=0x%08X"
                    " stride=%u %ux%u\n",
                    context->glide_lfb_lock_count, type, buffer, write_mode,
                    origin, staging_pointer,
                    context->glide_lfb_surface.stride_in_bytes(), width,
                    height);
        }
        ++context->glide_gate_handled_count;
        win32_context->Eax = 1U;
        win32_context->Eip = return_address;
        win32_context->Esp += 7U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRLFBUNLOCK@8")
    {
        // grLfbUnlock(type, buffer): a write lock's staging content becomes the
        // back-buffer image here; the next grBufferSwap presents it.
        const std::uint32_t type = context->glide_gate_stack[1];
        if (context->glide_lfb_surface.locked() &&
            type == repiu::hle::kGlideLfbWriteOnly)
        {
            // Did the guest actually write into the surface we handed it? A
            // non-zero count proves the lfbPtr round-trip works end to end and
            // separates "blit is broken" from "guest never wrote".
            {
                static long unlock_probe_count = 0;
                const long probe = InterlockedIncrement(&unlock_probe_count);
                if (probe <= 8)
                {
                    const std::uint8_t* pixels =
                        context->glide_lfb_surface.pixels();
                    const std::size_t total =
                        context->glide_lfb_surface.byte_count();
                    std::size_t non_zero = 0;
                    for (std::size_t i = 0; i < total; ++i)
                    {
                        if (pixels[i] != 0U)
                        {
                            ++non_zero;
                        }
                    }
                    fprintf(stderr,
                            "[repiu-live-debug] grLfbUnlock #%ld non-zero"
                            " staging bytes=%zu/%zu first-texels="
                            "%02X%02X %02X%02X %02X%02X\n",
                            probe, non_zero, total, pixels[1], pixels[0],
                            pixels[3], pixels[2], pixels[5], pixels[4]);
                }
            }
            std::vector<std::uint8_t> rgba8;
            if (repiu::hle::DecodeGlideLfb565ToRgba8(
                    context->glide_lfb_surface.pixels(),
                    context->glide_lfb_surface.byte_count(),
                    context->glide_lfb_surface.width(),
                    context->glide_lfb_surface.height(), &rgba8))
            {
                const bool flip_v =
                    context->glide_lfb_surface.lock_origin() ==
                    repiu::hle::kGlideOriginLowerLeft;
                if (context->glide_backend.PresentLfbSurface(
                        rgba8.data(), context->glide_lfb_surface.width(),
                        context->glide_lfb_surface.height(), flip_v))
                {
                    ++context->glide_lfb_present_count;
                    // The swap-driven pixel diagnostic cannot help here: the
                    // guest may never swap again. Sample the back buffer right
                    // after the blit so the blit itself is verifiable.
                    if (context->glide_lfb_present_count <= 4U)
                    {
                        std::vector<std::uint8_t> after;
                        if (context->glide_backend.ReadbackFramebuffer(
                                context->glide_lfb_surface.width(),
                                context->glide_lfb_surface.height(), &after))
                        {
                            std::size_t non_black = 0;
                            for (std::size_t i = 0; i + 3U < after.size();
                                 i += 4U)
                            {
                                if (after[i] > 8U || after[i + 1U] > 8U ||
                                    after[i + 2U] > 8U)
                                {
                                    ++non_black;
                                }
                            }
                            fprintf(stderr,
                                    "[repiu-live-debug] LFB blit #%u back-buffer"
                                    " non-black=%zu/%zu\n",
                                    context->glide_lfb_present_count, non_black,
                                    after.size() / 4U);
                        }
                    }
                }
                context->glide_backend_message =
                    context->glide_backend.message();
            }
        }
        context->glide_lfb_surface.EndLock();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    // Default handler for unhandled but cataloged gates (Phase R0)
    ++context->glide_gate_handled_count;
    if (signature->return_kind != repiu::hle::GlideReturnKind::kVoid)
    {
        win32_context->Eax = 0U;
    }
    win32_context->Eip = return_address;
    win32_context->Esp += sizeof(std::uint32_t) + signature->argument_byte_count;

    static long unhandled_gate_log_count = 0;
    const long log_index = InterlockedIncrement(&unhandled_gate_log_count);
    if (log_index <= 32)
    {
        fprintf(stderr,
                "[repiu-live-debug] glide gate unhandled (default) #%ld ordinal=%u"
                " name=%s\n",
                log_index, glide_export->ordinal,
                glide_export->name.c_str());
    }
    return true;
}

// Lightweight VEH transfer paths run without an ExceptionDispatchScope, so
// these counters must be mirrored into shared telemetry here to stay
// externally observable during dispatch-silent phases.

} // namespace repiu::platform::win32
