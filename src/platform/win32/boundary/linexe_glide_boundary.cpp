#include "linexe_glide_boundary.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "instruction_emulation.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
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
                    width, height, color_buffers, auxiliary_buffers);
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
        // Texture upload is a rendering-boundary operation. The current OpenGL
        // backend has no texture-image storage yet, so preserve the original
        // stdcall ABI and let subsequent guest logic continue while recording
        // the observed gate call through the existing telemetry.
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 9U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXCOMBINE@28" ||
        glide_export->name == "_GRTEXCLAMPMODE@12" ||
        glide_export->name == "_GRTEXFILTERMODE@12" ||
        glide_export->name == "_GRTEXMIPMAPMODE@12" ||
        glide_export->name == "_GRTEXSOURCE@16")
    {
        // Texture sampler and source selection stay within the rendering
        // boundary until the OpenGL texture-image backend is implemented.
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
