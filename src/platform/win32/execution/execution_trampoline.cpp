#include "repiu/platform/win32/execution_trampoline.h"
#include "native_fast_path.h"
#include "native_linear_span.h"
#include "verified_region_analyzer.h"
#include "native_phase_sampler.h"
#include "repiu/platform/win32/live_telemetry.h"
#include "repiu/hle/linexe_call_gate.h"
#include "repiu/hle/glide_hle.h"
#include "repiu/platform/win32/glide_opengl_backend.h"
#include "repiu/platform/win32/cd_audio_wave_out.h"
#include "repiu/platform/win32/aot_page_coherence_win32.h"
#include "repiu/media/chd_cd_image.h"
#include "repiu/runtime/dos_low_memory.h"
#include "repiu/runtime/selector_table.h"

#include <Zydis.h>

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <optional>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

#include "thread_context.h"
#include "linexe_glide_boundary.h"
#include "timer_interrupt_boundary.h"
#include "aot_dbt_call_step_probe.h"
#include "aot_dbt_dispatch.h"
#include "aot_runtime_dispatch.h"
#include "instruction_emulation.h"
#include "dpmi_mscdex_services.h"
#include "dos_int21_services.h"
#include "guest_memory_access.h"
#include "win32_thread_api.h"
#include "execution_internal.h"
#include "port_io_emulator.h"
#include "breakpoint_evidence_win32.h"
#include "exception_rescue_win32.h"
#include "live_telemetry_snapshot.h"

namespace repiu::platform::win32
{
extern "C" ThreadContext* g_repiu_active_thread_context = nullptr;
extern "C" std::uint32_t g_repiu_dbt_host_esp = 0;
extern "C" std::uint32_t g_repiu_dbt_host_stack_base = 0;
extern "C" std::uint32_t g_repiu_dbt_host_stack_limit = 0;
extern "C" std::uint32_t g_repiu_dbt_guest_stack_base = 0;
extern "C" std::uint32_t g_repiu_dbt_guest_stack_limit = 0;
namespace
{



bool IsDirectX86ExecutionSupported()
{
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    return true;
#else
    return false;
#endif
}

bool IsGuestStackSwitchSupported()
{
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
    return true;
#else
    return false;
#endif
}

#if defined(_WIN32)
// Guest execution is serialized to one worker per loader process. Keeping the
// VEH context outside Win32 TLS prevents a guest-modified FS selector from
// escaping into compiler-generated TLS access during host recovery.
std::uint32_t g_recovery_host_fs = 0;
std::uint32_t g_recovery_host_ds = 0;
std::uint32_t g_recovery_host_es = 0;
std::uint32_t g_recovery_host_gs = 0;
std::uint32_t g_recovery_host_stack_base = 0;
std::uint32_t g_recovery_host_stack_limit = 0;


std::vector<std::uint8_t> BuildDosEnvironmentBlock()
{
    std::vector<std::uint8_t> block;

#if defined(_WIN32)
    LPCH environment = GetEnvironmentStringsA();
    if (environment != nullptr)
    {
        const char* cursor = environment;
        while (*cursor != '\0')
        {
            const char* entry_begin = cursor;
            while (*cursor != '\0')
            {
                ++cursor;
            }

            bool before_equals = true;
            for (const char* current = entry_begin; current != cursor;
                 ++current)
            {
                unsigned char byte = static_cast<unsigned char>(*current);
                if (before_equals && byte == '=')
                {
                    before_equals = false;
                }
                else if (before_equals)
                {
                    byte = static_cast<unsigned char>(
                        std::toupper(static_cast<unsigned char>(byte)));
                }
                block.push_back(static_cast<std::uint8_t>(byte));
            }
            block.push_back(0);
            ++cursor;
        }
        FreeEnvironmentStringsA(environment);
    }
#endif

    if (block.empty() || block.back() != 0)
    {
        block.push_back(0);
    }
    block.push_back(0);
    return block;
}

int CaptureException(EXCEPTION_POINTERS* exception_info,
                     ThreadContext* context)
{
    if (exception_info != nullptr && context != nullptr)
    {
        if (exception_info->ExceptionRecord->ExceptionCode == 0xe06d7363U)
        {
            fprintf(stderr, "[repiu-live-debug] Caught C++ Exception (0xe06d7363) at address 0x%p\n",
                    exception_info->ExceptionRecord->ExceptionAddress);
            void* stack[64];
            USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
            fprintf(stderr, "[repiu-live-debug] Host Stack trace (%d frames):\n", frames);
            for (USHORT i = 0; i < frames; ++i)
            {
                fprintf(stderr, "  [%d] 0x%p\n", i, stack[i]);
            }
            HMODULE modules[256];
            DWORD cbNeeded;
            if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &cbNeeded))
            {
                fprintf(stderr, "[repiu-live-debug] Loaded Modules:\n");
                for (size_t i = 0; i < cbNeeded / sizeof(HMODULE); ++i)
                {
                    char name[MAX_PATH];
                    if (GetModuleFileNameA(modules[i], name, sizeof(name)))
                    {
                        MODULEINFO info;
                        if (GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info)))
                        {
                            fprintf(stderr, "  base=0x%p size=0x%X path=%s\n",
                                    info.lpBaseOfDll, info.SizeOfImage, name);
                        }
                    }
                }
            }
        }
        context->exception_caught = true;
        context->exception_code =
            exception_info->ExceptionRecord->ExceptionCode;
        context->exception_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(
                exception_info->ExceptionRecord->ExceptionAddress));
        MEMORY_BASIC_INFORMATION instruction_page = {};
        if (VirtualQuery(exception_info->ExceptionRecord->ExceptionAddress,
                         &instruction_page, sizeof(instruction_page)) ==
            sizeof(instruction_page))
        {
            std::uint8_t bytes[16] = {};
            std::memcpy(bytes, exception_info->ExceptionRecord->ExceptionAddress,
                        sizeof(bytes));
            fprintf(stderr,
                    "[repiu-live-debug] exception instruction region "
                    "base=0x%p alloc=0x%p size=0x%zX protect=0x%X "
                    "alloc_protect=0x%X bytes=",
                    instruction_page.BaseAddress,
                    instruction_page.AllocationBase,
                    instruction_page.RegionSize,
                    instruction_page.Protect,
                    instruction_page.AllocationProtect);
            for (std::uint8_t byte : bytes)
            {
                fprintf(stderr, "%02X", byte);
            }
            fprintf(stderr, "\n");
        }        if (exception_info->ExceptionRecord->NumberParameters >= 2U)
        {
            context->exception_access_kind = static_cast<std::uint32_t>(
                exception_info->ExceptionRecord->ExceptionInformation[0]);
            context->exception_fault_va = static_cast<std::uint32_t>(
                exception_info->ExceptionRecord->ExceptionInformation[1]);
            MEMORY_BASIC_INFORMATION fault_page = {};
            if (VirtualQuery(reinterpret_cast<const void*>(
                                 static_cast<std::uintptr_t>(
                                     context->exception_fault_va)),
                             &fault_page, sizeof(fault_page)) ==
                sizeof(fault_page))
            {
                context->exception_fault_region_base =
                    static_cast<std::uint32_t>(
                        reinterpret_cast<std::uintptr_t>(
                            fault_page.BaseAddress));
                context->exception_fault_alloc_base =
                    static_cast<std::uint32_t>(
                        reinterpret_cast<std::uintptr_t>(
                            fault_page.AllocationBase));
                context->exception_fault_state = fault_page.State;
                context->exception_fault_protect = fault_page.Protect;
                context->exception_fault_region_size =
                    static_cast<std::uint32_t>(fault_page.RegionSize);
            }
        }
#if defined(_M_IX86)
        CopySnapshotFromContextRecord(*exception_info->ContextRecord,
                                      &context->exception_snapshot);
        context->exception_eax = exception_info->ContextRecord->Eax;
        context->exception_ebx = exception_info->ContextRecord->Ebx;
        context->exception_ecx = exception_info->ContextRecord->Ecx;
        context->exception_edx = exception_info->ContextRecord->Edx;
        context->exception_esi = exception_info->ContextRecord->Esi;
        context->exception_edi = exception_info->ContextRecord->Edi;
        for (std::uint32_t index = 0; index < 8U; ++index)
        {
            const std::uintptr_t source =
                static_cast<std::uintptr_t>(
                    exception_info->ContextRecord->Esi) +
                0x20U + index * 4U;
            SIZE_T copied = 0;
            if (ReadProcessMemory(GetCurrentProcess(),
                                  reinterpret_cast<const void*>(source),
                                  &context->exception_esi_dwords[index],
                                  sizeof(std::uint32_t), &copied) != 0 &&
                copied == sizeof(std::uint32_t))
            {
                context->exception_esi_dword_valid_mask |= 1U << index;
            }
        }
        // Capture the ASCII string that each GPR points at (up to 32 bytes).
        // Null-pointer and string frontiers (e.g. a stricmp fed a filename with
        // no extension) are diagnosed by seeing the actual string a register
        // holds; ESI in particular keeps the callee-saved source pointer.
        const std::uint32_t exception_register_values[6] = {
            exception_info->ContextRecord->Eax,
            exception_info->ContextRecord->Ebx,
            exception_info->ContextRecord->Ecx,
            exception_info->ContextRecord->Edx,
            exception_info->ContextRecord->Esi,
            exception_info->ContextRecord->Edi,
        };
        for (std::uint32_t reg = 0; reg < 6U; ++reg)
        {
            SIZE_T copied = 0;
            if (exception_register_values[reg] != 0 &&
                ReadProcessMemory(
                    GetCurrentProcess(),
                    reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                        exception_register_values[reg])),
                    context->exception_register_strings[reg],
                    sizeof(context->exception_register_strings[reg]),
                    &copied) != 0 &&
                copied != 0)
            {
                context->exception_register_string_valid_mask |= 1U << reg;
            }
        }
        // Capture a window of the guest stack starting at the fault-time ESP.
        // Arguments passed to the faulting function sit above ESP; the caller
        // return address lives just below the lowest argument slot. Reading
        // this window is what lets a terminal fault's wild-pointer argument be
        // traced back to the caller that supplied it.
        context->exception_stack_base =
            static_cast<std::uint32_t>(exception_info->ContextRecord->Esp);
        context->exception_stack_dword_count = 0;
        for (std::uint32_t index = 0;
             index < kWin32ExceptionStackDwordCapacity; ++index)
        {
            const std::uintptr_t source =
                static_cast<std::uintptr_t>(context->exception_stack_base) +
                index * 4U;
            SIZE_T copied = 0;
            if (ReadProcessMemory(GetCurrentProcess(),
                                  reinterpret_cast<const void*>(source),
                                  &context->exception_stack_dwords[index],
                                  sizeof(std::uint32_t), &copied) == 0 ||
                copied != sizeof(std::uint32_t))
            {
                break;
            }
            context->exception_stack_dword_count = index + 1U;
        }
        // Optional: dump the runtime (dynamic) AOT cache bytes for a configured
        // guest address (REPIU_AOT_PROBE_GUEST). This lets a terminal fault
        // compare the on-demand translation of a block against the static plan,
        // to tell whether a runtime dynamic-cache divergence explains a
        // corrupted guest value.
        if (context->aot_placement != nullptr)
        {
            const char* probe_text = std::getenv("REPIU_AOT_PROBE_GUEST");
            if (probe_text != nullptr && *probe_text != '\0')
            {
                context->aot_probe_guest_address =
                    static_cast<std::uint32_t>(
                        std::strtoul(probe_text, nullptr, 0));
                std::uint32_t cache_address = 0;
                if (context->aot_probe_guest_address != 0 &&
                    FindAotCacheAddress(*context->aot_placement,
                                        context->aot_probe_guest_address,
                                        &cache_address))
                {
                    context->aot_probe_cache_address = cache_address;
                    SIZE_T copied = 0;
                    if (ReadProcessMemory(
                            GetCurrentProcess(),
                            reinterpret_cast<const void*>(
                                static_cast<std::uintptr_t>(cache_address)),
                            context->aot_probe_cache_bytes,
                            sizeof(context->aot_probe_cache_bytes),
                            &copied) != 0 &&
                        copied == sizeof(context->aot_probe_cache_bytes))
                    {
                        context->aot_probe_cache_valid = 1;
                    }
                }
            }
        }
#endif
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

#if defined(_MSC_VER) && defined(_M_IX86)
static_assert(offsetof(StackSwitchCallState, entry_address) == 0);
static_assert(offsetof(StackSwitchCallState, initial_esp) == 4);
static_assert(offsetof(StackSwitchCallState, host_esp) == 8);
static_assert(offsetof(StackSwitchCallState, guest_return_esp) == 12);
static_assert(offsetof(StackSwitchCallState, result_code) == 16);
static_assert(offsetof(StackSwitchCallState, enable_single_step_trace) == 20);
static_assert(offsetof(StackSwitchCallState, host_fs) == 24);
static_assert(offsetof(StackSwitchCallState, host_ds) == 28);
static_assert(offsetof(StackSwitchCallState, host_es) == 32);
static_assert(offsetof(StackSwitchCallState, host_gs) == 36);
static_assert(offsetof(StackSwitchCallState, host_ss) == 40);

extern "C" void RecoverHostStackException();

extern "C" __declspec(naked) std::uint32_t __stdcall
CallGuestEntryWithStack(StackSwitchCallState* state)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi

        mov ecx, [ebp + 8]
        mov eax, [ecx + 0]
        mov edx, [ecx + 4]

        // Save host stack base/limit
        mov ebx, dword ptr fs:[4]
        mov [ecx + 52], ebx
        mov g_recovery_host_stack_base, ebx
        mov g_repiu_dbt_host_stack_base, ebx
        mov ebx, dword ptr fs:[8]
        mov [ecx + 56], ebx
        mov g_recovery_host_stack_limit, ebx
        mov g_repiu_dbt_host_stack_limit, ebx

        // Set guest stack base/limit
        mov ebx, [ecx + 44]
        mov dword ptr fs:[4], ebx
        mov g_repiu_dbt_guest_stack_base, ebx
        mov ebx, [ecx + 48]
        mov dword ptr fs:[8], ebx
        mov g_repiu_dbt_guest_stack_limit, ebx

        xor ebx, ebx
        mov bx, fs
        mov [ecx + 24], ebx
        mov g_recovery_host_fs, ebx
        mov bx, ds
        mov [ecx + 28], ebx
        mov g_recovery_host_ds, ebx
        mov bx, es
        mov [ecx + 32], ebx
        mov g_recovery_host_es, ebx
        mov bx, gs
        mov [ecx + 36], ebx
        mov g_recovery_host_gs, ebx
        mov bx, ss
        mov [ecx + 40], ebx
        mov [ecx + 8], esp
        mov g_repiu_dbt_host_esp, esp

        mov esp, edx
        cmp dword ptr [ecx + 20], 0
        je no_single_step_trace
        pushfd
        or dword ptr [esp], 100h
        popfd
 no_single_step_trace:
        push ecx
        call eax
        pop ecx

        // Restore host stack base/limit
        mov ebx, [ecx + 52]
        mov dword ptr fs:[4], ebx
        mov ebx, [ecx + 56]
        mov dword ptr fs:[8], ebx

        mov [ecx + 12], esp
        mov esp, [ecx + 8]
        mov dword ptr [ecx + 16], 0
        xor eax, eax

        pop edi
        pop esi
        pop ebx
        pop ebp
        ret 4
    }
}

extern "C" __declspec(naked) void __stdcall
RecoverGuestStackException()
{
    __asm
    {
        mov eax, dword ptr cs:[g_recovery_host_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr cs:[g_recovery_host_stack_limit]
        mov dword ptr fs:[8], eax

        mov eax, dword ptr cs:[g_recovery_host_fs]
        mov fs, ax
        mov eax, dword ptr cs:[g_recovery_host_gs]
        mov gs, ax
        mov eax, dword ptr cs:[g_recovery_host_es]
        mov es, ax
        mov eax, dword ptr cs:[g_recovery_host_ds]
        mov ds, ax
        pop edi
        pop esi
        pop ebx
        pop ebp
        mov eax, 2
        ret 4
    }
}

void RecoverToHost(CONTEXT* context, ThreadContext* thread_context)
{
    context->Eip = reinterpret_cast<DWORD_PTR>(&RecoverGuestStackException);
    context->EFlags &= ~0x00000100U;
    context->EFlags &= ~0x00000400U;
    if (thread_context->active_call_state != nullptr)
    {
        context->Ecx = reinterpret_cast<DWORD_PTR>(
            thread_context->active_call_state);
        context->SegFs = static_cast<DWORD>(
            thread_context->active_call_state->host_fs);
        context->SegDs = static_cast<DWORD>(
            thread_context->active_call_state->host_ds);
        context->SegEs = static_cast<DWORD>(
            thread_context->active_call_state->host_es);
        context->SegGs = static_cast<DWORD>(
            thread_context->active_call_state->host_gs);
        context->SegSs = static_cast<DWORD>(
            thread_context->active_call_state->host_ss);
    }
    std::uint32_t host_esp = thread_context->host_esp;
    if (host_esp == 0 && thread_context->active_call_state != nullptr)
    {
        host_esp = thread_context->active_call_state->host_esp;
    }
    thread_context->host_esp = host_esp;
    context->Esp = host_esp;
}


bool HandlePrivilegedTrapInstruction(CONTEXT* win32_context,
                                     ThreadContext* context);
bool HandleSelectorLimitInstruction(CONTEXT* win32_context,
                                    ThreadContext* context);
struct AotPlacementPlan;
bool FindAotGuestAddress(const AotPlacementPlan& placement,
                         std::uint32_t host_address,
                         std::uint32_t* guest_address);

bool HandleSelectorLimitInstruction(CONTEXT* win32_context,
                                    ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(context, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(win32_context->Eip)), 3U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x0F ||
        (instruction[1] != 0x02 && instruction[1] != 0x03))
    {
        return false;
    }
    const std::uint8_t modrm = instruction[2];
    const std::uint8_t mod = (modrm >> 6) & 0x03U;
    const std::uint8_t destination_register = (modrm >> 3) & 0x07U;
    std::uint16_t selector = 0;
    std::uint32_t instruction_size = 3;
    if (mod == 0x03U)
    {
        selector = ReadRegister16(*win32_context, modrm & 0x07U);
    }
    else
    {
        std::uint32_t source = 0;
        std::uint32_t unprefixed_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + 1,
                                      &source,
                                      &unprefixed_size))
        {
            return false;
        }
        const void* source_pointer = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(source));
        if (!IsGuestRangeReadable(context, source_pointer, sizeof(selector)))
        {
            return false;
        }
        std::memcpy(&selector, source_pointer, sizeof(selector));
        instruction_size = 1U + unprefixed_size;
    }

    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    const repiu::runtime::GuestDescriptor* descriptor =
        repiu::runtime::FindDescriptor(context->selector_table, selector);
    if (descriptor != nullptr && descriptor->present)
    {
        const std::uint32_t value = instruction[1] == 0x03
            ? descriptor->limit
            : (descriptor->flags & 0xFFFFU) << 8;
        WriteGeneralRegister32(win32_context, destination_register, value);
        win32_context->EFlags |= kZeroFlag;
    }
    else
    {
        win32_context->EFlags &= ~kZeroFlag;
    }
    win32_context->Eip += instruction_size;
    return true;
}



bool HandleGuestLowMemoryReadFault(CONTEXT* win32_context,
                                   ThreadContext* context,
                                   std::uint32_t fault_va,
                                   std::uint32_t decode_eip);
bool HandleDosMemoryAccess(CONTEXT* win32_context,
                           ThreadContext* context);



// Shared guest-instruction HLE dispatch (Task 266). Runs the same handler chain
// the single-step path uses to emulate one sensitive guest instruction at the
// current EIP, advancing EIP past it, WITHOUT touching the trap flag. Callers
// decide whether to re-arm single-step (HandleSingleStepTrace) or stay native
// (the region executor). Mirrors the former inline chain in HandleSingleStepTrace.
bool DispatchGuestHleHandlers(CONTEXT* win32_context, ThreadContext* context)
{
    constexpr std::uint32_t kMaximumX86InstructionBytes = 15U;
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            kMaximumX86InstructionBytes))
    {
        return false;
    }

    const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    std::uint32_t offset = 0U;
    if (ptr[0] == 0x66U || ptr[0] == 0x67U)
    {
        offset = 1U;
    }
    const std::uint8_t opcode = ptr[offset];

    // Opcode-directed Fast Dispatcher (Task 312)
    switch (opcode)
    {
        case 0xECU: case 0xEDU: case 0xEEU: case 0xEFU:
            if (context->enable_privileged_trap_hle && HandlePortIoInstruction(win32_context, context)) return true;
            break;
        case 0x8EU:
            if (context->enable_segment_load_hle && HandleSegmentLoadInstruction(win32_context, context)) return true;
            break;
        case 0x8CU:
            if (context->enable_segment_load_hle && HandleSegmentStoreInstruction(win32_context, context)) return true;
            break;
        case 0x07U: case 0x1FU:
            if (context->enable_segment_load_hle && HandleSegmentPopInstruction(win32_context, context)) return true;
            break;
        case 0xCDU:
            if (context->enable_traced_dos_hle &&
                (HandleTracedDosInterrupt21(win32_context, context) ||
                 HandleTracedDosInterrupt2F(win32_context, context) ||
                 HandleTracedDpmiInterrupt31(win32_context, context) ||
                 HandleTracedMouseInterrupt33(win32_context, context))) return true;
            break;
        case 0xFAU: case 0xFBU:
            if (context->enable_privileged_trap_hle && HandlePrivilegedTrapInstruction(win32_context, context)) return true;
            break;
        case 0xABU:
            if (context->enable_segment_load_hle && HandleRepStosdInstruction(win32_context, context)) return true;
            break;
        case 0xA4U: case 0xA5U:
            if (context->enable_segment_load_hle && HandleRepMovsInstruction(win32_context, context)) return true;
            break;
        case 0x64U: case 0x65U: case 0x26U: case 0x2EU: case 0x36U: case 0x3EU:
            if (context->enable_segment_load_hle &&
                (HandleSegmentOverrideMemoryLoadInstruction(win32_context, context) ||
                 HandleSegmentOverrideByteLoadInstruction(win32_context, context) ||
                 HandleFsSegmentWordLoadInstruction(win32_context, context))) return true;
            break;
        default:
            break;
    }

    if (context->enable_privileged_trap_hle &&
        (HandleSelectorLimitInstruction(win32_context, context) ||
         HandlePrivilegedTrapInstruction(win32_context, context) ||
         HandlePortIoInstruction(win32_context, context)))
    {
        return true;
    }
    if (context->enable_traced_dos_hle &&
        (HandleTracedDosInterrupt21(win32_context, context) ||
         HandleTracedDosInterrupt2F(win32_context, context) ||
         HandleTracedDpmiInterrupt31(win32_context, context) ||
         HandleTracedMouseInterrupt33(win32_context, context)))
    {
        return true;
    }
    if (context->enable_segment_load_hle &&
        (HandleSegmentLoadInstruction(win32_context, context) ||
         HandleSegmentPopInstruction(win32_context, context) ||
         HandleRepStosdInstruction(win32_context, context) ||
         HandleRepMovsInstruction(win32_context, context) ||
         HandleRepCmpsbInstruction(win32_context, context) ||
         HandleLodsbInstruction(win32_context, context) ||
         HandleSegmentStoreInstruction(win32_context, context) ||
         HandleSegmentOverrideMemoryLoadInstruction(win32_context, context) ||
         HandleSegmentOverrideByteLoadInstruction(win32_context, context) ||
         HandleFsSegmentWordLoadInstruction(win32_context, context) ||
         HandleSegmentMemoryCompareInstruction(win32_context, context) ||
         HandleSegmentMemoryLoadInstruction(win32_context, context) ||
         HandleTracedMemoryLoadInstruction(win32_context, context) ||
         HandleTracedMemoryAddInstruction(win32_context, context) ||
         HandleTracedMemoryOrInstruction(win32_context, context) ||
         HandleTracedMemoryCompareByteInstruction(win32_context, context) ||
         HandleTracedMemoryStoreInstruction(win32_context, context) ||
         HandleTracedMemoryTestInstruction(win32_context, context) ||
         HandleTracedFpuMemoryInstruction(win32_context, context) ||
         HandleDosMemoryAccess(win32_context, context)))
    {
        HandleSegmentStoreInstruction(win32_context, context);
        HandleSegmentOverrideMemoryLoadInstruction(win32_context, context);
        return true;
    }
    return false;
}

// Route A native region execution (Task 266). Opt-in via REPIU_NATIVE_REGION.
// See docs/design/20260723-266-native-region-execution.md.
bool RouteANativeRegionEnabled()
{
    static const bool enabled = []() {
        char value[2] = {};
        return GetEnvironmentVariableA(
                   "REPIU_NATIVE_REGION", value, sizeof(value)) > 0;
    }();
    return enabled;
}

// Tear down an active native region: restore the debug registers, re-arm
// single-step, and clear the active flag. No guest byte is ever modified (the
// sensitive instructions are trapped with hardware breakpoints, not INT3), so
// there is nothing to unpatch.
void LeaveNativeRegion(CONTEXT* win32_context, ThreadContext* context,
                       bool returned)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    if (!state->region_active)
    {
        return;
    }
    win32_context->Dr0 = state->region_saved_dr0;
    win32_context->Dr1 = state->region_saved_dr1;
    win32_context->Dr2 = state->region_saved_dr2;
    win32_context->Dr3 = state->region_saved_dr3;
    win32_context->Dr6 = state->region_saved_dr6;
    win32_context->Dr7 = state->region_saved_dr7;
    win32_context->EFlags |= 0x00000100U;
    state->region_active = false;
    if (returned)
    {
        state->region_return_count.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        state->region_cancel_count.fetch_add(1, std::memory_order_relaxed);
    }
}

// A hardware breakpoint (Dr1-Dr3) at a sensitive instruction faults BEFORE the
// instruction executes, with EIP at the instruction. HLE-emulate it via the
// shared dispatch (which advances EIP past it) and keep running natively with
// the breakpoints armed. Returns false if the instruction is at an unexpected
// address or has no handler, so the caller tears the region down and lets the
// single-step path take it (matching the single-step native fall-through).
bool HandleNativeRegionSensitiveDr(CONTEXT* win32_context,
                                   ThreadContext* context)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    const std::uint32_t eip = static_cast<std::uint32_t>(win32_context->Eip);
    bool ours = false;
    for (std::uint32_t i = 0; i < state->region_sensitive_slots; ++i)
    {
        if (state->region_sensitive_addr[i] == eip)
        {
            ours = true;
            break;
        }
    }
    if (!ours)
    {
        return false;
    }
    win32_context->EFlags &= ~0x00000100U;  // stay native
    const bool handled = DispatchGuestHleHandlers(win32_context, context);
    const std::uint32_t after = static_cast<std::uint32_t>(win32_context->Eip);
    if (!handled || after == eip)
    {
        return false;
    }
    state->region_sensitive_hit_count.fetch_add(1, std::memory_order_relaxed);
    win32_context->Dr6 = 0;
    win32_context->EFlags &= ~0x00000100U;  // remain native
    return true;
}

// Try to enter a native region at the current EIP. Same entry condition as the
// clean fast path (current EIP is the target of a direct `call rel32`). The
// region may contain up to kMaxRegionSensitive HLE-sensitive instructions, which
// are trapped with hardware execution breakpoints (Dr1-Dr3); Dr0 breakpoints the
// caller return address. Regions with more sensitive instructions than hardware
// slots are declined (the single-step path keeps handling them).
bool TryEnterNativeRegion(CONTEXT* win32_context, ThreadContext* context)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    const std::uint32_t runtime_base = context->runtime_base;
    const std::uint32_t runtime_size = context->runtime_size;
    const std::uint32_t previous_eip = state->previous_eip;
    state->previous_eip = static_cast<std::uint32_t>(win32_context->Eip);
    if (state->region_active || state->active)
    {
        return false;
    }
    const auto in_range = [&](std::uint32_t a, std::uint32_t n) {
        const std::uint32_t end = a + n;
        const std::uint32_t rt_end = runtime_base + runtime_size;
        return end >= a && rt_end >= runtime_base && a >= runtime_base &&
               end <= rt_end;
    };
    if (!in_range(static_cast<std::uint32_t>(win32_context->Esp),
                  sizeof(std::uint32_t)) ||
        !in_range(previous_eip, 5U))
    {
        return false;
    }
    const auto* previous = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(previous_eip));
    std::int32_t displacement = 0;
    std::memcpy(&displacement, previous + 1, sizeof(displacement));
    const std::uint32_t entry = static_cast<std::uint32_t>(win32_context->Eip);
    if (previous[0] != 0xE8U || previous_eip + 5U + displacement != entry)
    {
        return false;
    }
    const std::uint32_t return_address = *reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (!in_range(return_address, 1U))
    {
        return false;
    }
    const auto cached_reject = state->region_analyzable_cache.find(entry);
    if (cached_reject != state->region_analyzable_cache.end() &&
        cached_reject->second == -1)
    {
        return false;
    }
    std::vector<std::uint32_t>* sensitive = nullptr;
    const auto cached = state->region_sensitive_cache.find(entry);
    if (cached != state->region_sensitive_cache.end())
    {
        sensitive = &cached->second;
    }
    else
    {
        constexpr std::uint32_t kMaxSensitive = 256;
        std::vector<std::uint32_t> found;
        if (!detail::ScanNativeRegionWithZydis(
                entry, runtime_base, runtime_size, kMaxSensitive, &found))
        {
            state->region_analyzable_cache[entry] = -1;
            state->region_reject_count.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        state->region_analyzable_cache[entry] = 1;
        sensitive = &(state->region_sensitive_cache[entry] = std::move(found));
    }
    if (sensitive->size() >
        detail::NativeFastPathState::kMaxRegionSensitive)
    {
        // More sensitive instructions than hardware breakpoint slots: decline so
        // the single-step path keeps handling this region.
        state->region_analyzable_cache[entry] = -1;
        state->region_reject_count.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    for (std::uint32_t addr : *sensitive)
    {
        if (!in_range(addr, 1U))
        {
            state->region_analyzable_cache[entry] = -1;
            return false;
        }
    }
    state->region_sensitive_slots = static_cast<std::uint32_t>(sensitive->size());
    for (std::uint32_t i = 0;
         i < detail::NativeFastPathState::kMaxRegionSensitive; ++i)
    {
        state->region_sensitive_addr[i] =
            i < state->region_sensitive_slots ? (*sensitive)[i] : 0U;
    }
    state->region_return_address = return_address;
    state->region_saved_dr0 = static_cast<std::uint32_t>(win32_context->Dr0);
    state->region_saved_dr1 = static_cast<std::uint32_t>(win32_context->Dr1);
    state->region_saved_dr2 = static_cast<std::uint32_t>(win32_context->Dr2);
    state->region_saved_dr3 = static_cast<std::uint32_t>(win32_context->Dr3);
    state->region_saved_dr6 = static_cast<std::uint32_t>(win32_context->Dr6);
    state->region_saved_dr7 = static_cast<std::uint32_t>(win32_context->Dr7);
    win32_context->Dr0 = return_address;
    std::uint32_t enable_bits = 0x1U;  // L0 for the return-address breakpoint
    if (state->region_sensitive_slots >= 1)
    {
        win32_context->Dr1 = state->region_sensitive_addr[0];
        enable_bits |= 0x1U << 2;  // L1
    }
    if (state->region_sensitive_slots >= 2)
    {
        win32_context->Dr2 = state->region_sensitive_addr[1];
        enable_bits |= 0x1U << 4;  // L2
    }
    if (state->region_sensitive_slots >= 3)
    {
        win32_context->Dr3 = state->region_sensitive_addr[2];
        enable_bits |= 0x1U << 6;  // L3
    }
    win32_context->Dr6 = 0;
    // Clear all four slots' enable (bits 0-7) and R/W+LEN control (bits 16-31),
    // leaving R/W=00 (execute) and LEN=00 (1 byte), then set the enables.
    win32_context->Dr7 =
        (static_cast<std::uint32_t>(win32_context->Dr7) & ~0xFFFF00FFU) |
        enable_bits;
    win32_context->EFlags &= ~0x00000100U;
    state->region_active = true;
    state->region_entry_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

// Route A sizing instrumentation. Decodes the instruction at `eip` and reports
// whether it is HLE-sensitive under selective-breakpoint region execution: a
// segment-override memory access, a segment-register move/push/pop, an FS/GS
// read-write, an INT/IO/string/privileged/system instruction. Everything else
// (ALU, plain mov/lea, register push/pop, direct branch/call/ret) can run
// natively between traps. Mirrors detail::IsSensitive in the verified region
// analyzer so the measured ceiling matches what the native fast path enforces.
bool ClassifyRouteASensitive(std::uint32_t eip, bool* is_segment)
{
    if (is_segment != nullptr)
    {
        *is_segment = false;
    }
    static thread_local ZydisDecoder decoder;
    static thread_local bool decoder_ready = false;
    if (!decoder_ready)
    {
        if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder,
                                           ZYDIS_MACHINE_MODE_LEGACY_32,
                                           ZYDIS_STACK_WIDTH_32)))
        {
            return false;
        }
        decoder_ready = true;
    }
    ZydisDecodedInstruction instruction{};
    const auto* bytes = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(eip));
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(
            &decoder, nullptr, bytes, ZYDIS_MAX_INSTRUCTION_LENGTH,
            &instruction)))
    {
        return false;
    }
    const bool segment =
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) != 0 ||
        instruction.meta.category == ZYDIS_CATEGORY_SEGOP ||
        instruction.meta.category == ZYDIS_CATEGORY_RDWRFSGS;
    if (is_segment != nullptr)
    {
        *is_segment = segment;
    }
    if (segment ||
        (instruction.attributes & ZYDIS_ATTRIB_IS_PRIVILEGED) != 0)
    {
        return true;
    }
    switch (instruction.meta.category)
    {
    case ZYDIS_CATEGORY_INTERRUPT:
    case ZYDIS_CATEGORY_IO:
    case ZYDIS_CATEGORY_IOSTRINGOP:
    case ZYDIS_CATEGORY_STRINGOP:
    case ZYDIS_CATEGORY_SYSCALL:
    case ZYDIS_CATEGORY_SYSRET:
    case ZYDIS_CATEGORY_SYSTEM:
    case ZYDIS_CATEGORY_UINTR:
        return true;
    default:
        return false;
    }
}

// Extends the same int3-sentinel single-step probe as RecordExecutionProbe,
// but logs every single-stepped instruction inside a guest code RANGE
// (rather than one exact-match address, and without a single-shot gate) into
// a wrapping ring. `value_at_esp_offset` is read relative to the live ESP at
// each capture, not a hardcoded absolute address. See
// docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md §8.

// Clears ThreadContext::active_hotspot_scope on every exit path of
// HandleSingleStepTrace so a stale pointer can never outlive its sample.
class ActiveHotspotScopeReset
{
public:
    explicit ActiveHotspotScopeReset(ThreadContext* context)
        : context_(context)
    {
    }

    ~ActiveHotspotScopeReset()
    {
        if (context_ != nullptr)
        {
            context_->active_hotspot_scope = nullptr;
        }
    }

    ActiveHotspotScopeReset(const ActiveHotspotScopeReset&) = delete;
    ActiveHotspotScopeReset& operator=(const ActiveHotspotScopeReset&) = delete;

private:
    ThreadContext* context_ = nullptr;
};

// Diagnostic instrumentation that runs unconditionally on every single step:
// execution probe/trace capture, the LINEXE resolution EIP ladder, the shadow
// register mirror read by the telemetry supervisor, and Route A classification.
// Extracted from HandleSingleStepTrace in Task 322 so its cost can be attributed
// to SingleStepProfileStage::kPrologueTrace without re-indenting the body. The
// body is unchanged; only the enclosing function boundary moved.
void RecordSingleStepDiagnostics(CONTEXT* win32_context, ThreadContext* context)
{
    RecordExecutionProbe(win32_context, context);
    RecordExecutionTrace(win32_context, context);
    const std::uint32_t eip_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (eip_offset >= 0x000F38F6U && eip_offset <= 0x000F3902U)
    {
        constexpr std::uint32_t stages[] = {
            0x000F38F6U, 0x000F38FAU, 0x000F38FEU,
            0x000F3900U, 0x000F3902U};
        for (std::uint32_t index = 0; index < 5; ++index)
        {
            if (eip_offset == stages[index])
            {
                context->linexe_export_name_stage_mask |= 1U << index;
            }
        }
    }
    if (eip_offset == 0x000F37E8U)
    {
        ++context->linexe_scan_entry_count;
    }
    else if (eip_offset == 0x000F382DU)
    {
        ++context->linexe_module_candidate_count;
        const auto* selector = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x10U));
        if (IsGuestRangeReadable(context, selector, sizeof(*selector)))
        {
            context->linexe_module_selector_stack_value = *selector;
        }
        const auto* offset = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x08U));
        if (IsGuestRangeReadable(context, offset, sizeof(*offset)))
        {
            context->linexe_module_offset_stack_value = *offset;
        }
    }
    else if (eip_offset == 0x000F3818U)
    {
        context->linexe_root_selector_eax = win32_context->Eax;
        context->linexe_root_read_gs = context->guest_gs;
    }
    else if (eip_offset == 0x000F3889U)
    {
        ++context->linexe_module_match_count;
    }
    else if (eip_offset == 0x000F384CU)
    {
        ++context->linexe_name_pointer_valid_count;
    }
    else if (eip_offset == 0x000F3853U)
    {
        ++context->linexe_name_byte_instruction_count;
    }
    else if (eip_offset == 0x000F393FU)
    {
        ++context->linexe_export_match_count;
    }
    else if (eip_offset == 0x000F38BEU)
    {
        ++context->linexe_export_entry_loop_count;
    }
    else if (eip_offset == 0x000F3974U)
    {
        ++context->linexe_export_compare_count;
        context->linexe_export_compare_eax = win32_context->Eax;
        context->linexe_export_compare_ecx = win32_context->Ecx;
        context->linexe_export_compare_eflags = win32_context->EFlags;
    }
    else if (eip_offset == 0x000F396DU)
    {
        context->linexe_export_count_load_edx = win32_context->Edx;
        context->linexe_export_count_load_gs = context->guest_gs;
    }
    else if (eip_offset == 0x000F3963U)
    {
        const auto* offset = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x08U));
        const auto* selector = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x10U));
        if (IsGuestRangeReadable(context, offset, sizeof(*offset)))
        {
            context->linexe_export_offset_stack_value = *offset;
        }
        if (IsGuestRangeReadable(context, selector, sizeof(*selector)))
        {
            context->linexe_export_selector_stack_value = *selector;
        }
    }
    else if (eip_offset == 0x000F38B9U ||
             eip_offset == 0x000F395FU)
    {
        const auto* offset = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x08U));
        const auto* selector = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x10U));
        const std::uint32_t module_offset =
            IsGuestRangeReadable(context, offset, sizeof(*offset)) ?
                *offset : 0;
        const std::uint16_t module_selector =
            IsGuestRangeReadable(context, selector, sizeof(*selector)) ?
                *selector : 0;
        if (eip_offset == 0x000F38B9U)
        {
            context->linexe_export_jump_source_esp = win32_context->Esp;
            context->linexe_export_jump_source_module_offset = module_offset;
            context->linexe_export_jump_source_module_selector =
                module_selector;
        }
        else
        {
            context->linexe_export_jump_target_esp = win32_context->Esp;
            context->linexe_export_jump_target_module_offset = module_offset;
            context->linexe_export_jump_target_module_selector =
                module_selector;
        }
    }
    else if (eip_offset == 0x000F3900U)
    {
        ++context->linexe_export_name_compare_count;
        context->linexe_export_name_compare_gs = context->guest_gs;
        context->linexe_export_name_compare_edi = win32_context->Edi;
        context->linexe_export_name_compare_esi = win32_context->Esi;
        std::uint8_t actual = 0;
        if (ReadSegmentByte(context, 5, context->guest_gs,
                            win32_context->Edi, &actual))
        {
            context->linexe_export_name_actual_byte = actual;
        }
        const auto* expected = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(win32_context->Esi));
        if (IsGuestRangeReadable(context, expected, sizeof(*expected)))
        {
            context->linexe_export_name_expected_byte = *expected;
        }
    }
    else if (eip_offset == 0x000F37A5U)
    {
        ++context->linexe_bridge_entry_count;
        context->linexe_bridge_selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        context->linexe_bridge_offset = win32_context->Edi;
        context->linexe_bridge_esp = win32_context->Esp;
        context->linexe_bridge_ebp = win32_context->Ebp;
        repiu::hle::LinexeService service{};
        context->linexe_bridge_gate_valid =
            context->linexe_bridge_selector ==
                context->linexe_gate_plan.linexe_code_selector &&
            repiu::hle::DecodeLinexeCallGate(
                context->linexe_gate_plan,
                context->linexe_bridge_offset,
                &service);
        if (context->linexe_bridge_gate_valid)
        {
            context->linexe_bridge_service =
                static_cast<std::uint32_t>(service);
        }
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
    }
    else if (eip_offset == 0x000F39A6U)
    {
        ++context->linexe_scan_return_count;
        context->linexe_scan_return_eax = win32_context->Eax;
        context->linexe_scan_return_ebp = win32_context->Ebp;
    }
    else if (eip_offset == 0x000F3F9BU)
    {
        context->linexe_scan_caller_eax = win32_context->Eax;
    }
    else if (eip_offset == 0x000F3FD2U)
    {
        context->linexe_selector_init_results[0] = win32_context->Eax;
    }
    else if (eip_offset == 0x000F3FE1U)
    {
        context->linexe_selector_init_results[1] = win32_context->Eax;
    }
    else if (eip_offset == 0x000F3FF0U)
    {
        context->linexe_selector_init_results[2] = win32_context->Eax;
    }
    const std::uint32_t eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    if (IsGuestInstructionPointer(context, eip))
    {
        context->single_step_eip.store(eip, std::memory_order_relaxed);
        context->single_step_eax.store(win32_context->Eax,
                                       std::memory_order_relaxed);
        context->single_step_ebx.store(win32_context->Ebx,
                                       std::memory_order_relaxed);
        context->single_step_ecx.store(win32_context->Ecx,
                                       std::memory_order_relaxed);
        context->single_step_edx.store(win32_context->Edx,
                                       std::memory_order_relaxed);
        context->single_step_esi.store(win32_context->Esi,
                                       std::memory_order_relaxed);
        context->single_step_edi.store(win32_context->Edi,
                                       std::memory_order_relaxed);
        context->single_step_esp.store(win32_context->Esp,
                                       std::memory_order_relaxed);
        context->single_step_ebp.store(win32_context->Ebp,
                                       std::memory_order_relaxed);
        context->single_step_eflags.store(win32_context->EFlags,
                                          std::memory_order_relaxed);
        context->single_step_cs.store(win32_context->SegCs,
                                      std::memory_order_relaxed);
        context->single_step_ds.store(win32_context->SegDs,
                                      std::memory_order_relaxed);
        context->single_step_es.store(win32_context->SegEs,
                                      std::memory_order_relaxed);
        context->single_step_ss.store(win32_context->SegSs,
                                      std::memory_order_relaxed);
        context->single_step_fs.store(win32_context->SegFs,
                                      std::memory_order_relaxed);
        context->single_step_gs.store(win32_context->SegGs,
                                      std::memory_order_relaxed);
        context->single_step_trace_count.fetch_add(
            1,
            std::memory_order_relaxed);
        bool routea_segment = false;
        if (ClassifyRouteASensitive(eip, &routea_segment))
        {
            context->routea_sensitive_count.fetch_add(
                1, std::memory_order_relaxed);
            if (routea_segment)
            {
                context->routea_segment_sensitive_count.fetch_add(
                    1, std::memory_order_relaxed);
            }
        }
    }
}

bool HandleSingleStepTrace(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->enable_single_step_trace)
    {
        return false;
    }
    const std::uint32_t profile_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    Win32SingleStepHotspotProfile* hotspot_profile =
        context->single_step_hotspot_profile != nullptr &&
        IsGuestInstructionPointer(context, profile_eip)
            ? context->single_step_hotspot_profile.get()
            : nullptr;
    SingleStepHotspotCycleScope hotspot_scope(
        hotspot_profile, profile_eip);
    // Task 323: publish the open sample so TryResumeAotAfterHandledHle can
    // attribute its sub-stages into it from another translation unit.
    context->active_hotspot_scope = &hotspot_scope;
    const ActiveHotspotScopeReset active_scope_reset(context);
    {
        SingleStepHotspotStageScope stage_scope(
            hotspot_scope, SingleStepProfileStage::kPrologueTrace);
        RecordSingleStepDiagnostics(win32_context, context);
    }

    const std::uint32_t hle_entry_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    bool handled_hle = false;
    {
        SingleStepHotspotStageScope stage_scope(
            hotspot_scope, SingleStepProfileStage::kHleDispatch);
        handled_hle = DispatchGuestHleHandlers(win32_context, context);
    }
    if (handled_hle)
    {
        hotspot_scope.SetOutcome(
            SingleStepProfileOutcome::kHandledHle);
        if (static_cast<std::uint32_t>(win32_context->Eip) != hle_entry_eip)
        {
            bool resumed = false;
            {
                SingleStepHotspotStageScope stage_scope(
                    hotspot_scope, SingleStepProfileStage::kAotResume);
                resumed = TryResumeAotAfterHandledHle(
                    win32_context, context, hle_entry_eip);
            }
            if (resumed)
            {
                return true;
            }
        }
        win32_context->EFlags |= 0x00000100U;
        return true;
    }

    // Task 301: deliver a coalesced timer request only after the existing VEH
    // path has reconciled AOT/HLE state to a guest instruction boundary.
    // PollThreadUntilExit never forces TF or changes the guest thread context.
    const std::uint32_t interrupt_boundary_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    {
        SingleStepHotspotStageScope stage_scope(
            hotspot_scope, SingleStepProfileStage::kInterruptInjection);
        InjectPendingInterrupts(win32_context, context);
    }
    if (static_cast<std::uint32_t>(win32_context->Eip) != interrupt_boundary_eip)
    {
        hotspot_scope.SetOutcome(
            SingleStepProfileOutcome::kTimerInterrupt);
        return true;
    }
    const bool call_step_return_watch =
        AotDbtCallStepReturnWatchActive(context);
    bool entered_native = false;
    {
        SingleStepHotspotStageScope stage_scope(
            hotspot_scope, SingleStepProfileStage::kNativeEntry);
        if (!call_step_return_watch && RouteANativeRegionEnabled())
        {
            entered_native = TryEnterNativeRegion(win32_context, context);
        }
        else if (!call_step_return_watch)
        {
            entered_native = detail::TryEnterNativeFastPath(
                win32_context,
                &context->native_fast_path,
                context->runtime_base,
                context->runtime_size);
        }
        if (!call_step_return_watch &&
            !entered_native &&
            NativeLinearSpanEnabled(context->execution_backend))
        {
            entered_native = TryEnterNativeLinearSpan(win32_context, context);
        }
    }
    if (entered_native)
    {
        hotspot_scope.SetOutcome(
            SingleStepProfileOutcome::kNativeExecution);
        return true;
    }
    win32_context->EFlags |= 0x00000100U;
    return true;
}






void RecordHandledHleTrap(CONTEXT* win32_context,
                          ThreadContext* context,
                          std::uint8_t opcode)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_hle_trap_count;
    context->last_hle_trap_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_hle_trap_opcode = opcode;
}



bool HandleOriginalFatalBreakpoint(EXCEPTION_POINTERS* exception_info,
                                   CONTEXT* win32_context,
                                   ThreadContext* context)
{
    if (exception_info == nullptr ||
        exception_info->ExceptionRecord == nullptr ||
        exception_info->ExceptionRecord->ExceptionCode !=
            EXCEPTION_BREAKPOINT ||
        win32_context == nullptr || context == nullptr ||
        win32_context->Eip == 0)
    {
        return false;
    }

    const std::uint32_t context_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    std::uint32_t breakpoint = context_eip;
    bool advance_to_continuation = false;
    if (IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(context_eip)),
            1U) &&
        *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(context_eip)) == 0xCC)
    {
        advance_to_continuation = true;
    }
    else
    {
        breakpoint = context_eip - 1U;
    }
    if (!IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(breakpoint)),
            8U))
    {
        return false;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(breakpoint));
    if (instruction[0] != 0xCC || instruction[1] != 0x52 ||
        instruction[2] != 0xE8 || instruction[7] != 0xF4)
    {
        return false;
    }

    context->handled_fatal_breakpoint_count += 1U;
    context->last_fatal_breakpoint_address = breakpoint;
    context->last_fatal_message_address =
        static_cast<std::uint32_t>(win32_context->Edx);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->fatal_breakpoint_count,
            static_cast<long>(context->handled_fatal_breakpoint_count));
        InterlockedExchange(
            &context->shared_live_telemetry->fatal_message_address,
            static_cast<long>(context->last_fatal_message_address));
    }
    context->last_fatal_message.clear();
    ReadGuestAsciz(context,
                   context->last_fatal_message_address,
                   512U,
                   &context->last_fatal_message);
    context->fatal_breakpoint_continued = true;
    if (advance_to_continuation)
    {
        win32_context->Eip += 1U;
    }
    return true;
}



bool HandlePrivilegedTrapInstruction(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(context, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(win32_context->Eip)), 1U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (*instruction == 0xFA)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        win32_context->EFlags &= ~kEFlagsInterruptEnable;
        ++win32_context->Eip;
        return true;
    }
    if (*instruction == 0xFB)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        win32_context->EFlags |= kEFlagsInterruptEnable;
        ++win32_context->Eip;
        return true;
    }
    if (*instruction == 0xF4 && context->fatal_breakpoint_continued)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        context->fatal_halt_reached = true;
        RecoverFromHleExit(win32_context, context);
        return true;
    }

    return false;
}


bool HandleDosHleInstruction(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] == 0xCD && instruction[1] == 0x21)
    {
        return HandleDosInterrupt21(win32_context, context);
    }
    if (instruction[0] == 0xCD && instruction[1] == 0x2F)
    {
        return HandleDosInterrupt2F(win32_context, context);
    }
    if (instruction[0] == 0xCD && instruction[1] == 0x31)
    {
        return HandleDpmiInterrupt31(win32_context, context);
    }
    if (instruction[0] == 0xCD && instruction[1] == 0x33)
    {
        return HandleMouseInterrupt33(win32_context, context);
    }

    if (instruction[0] == 0xCD)
    {
        std::ostringstream stream;
        stream << "unsupported DOS interrupt 0x"
               << std::hex << static_cast<unsigned>(instruction[1]);
        context->hle_message = stream.str();
    }
    return false;
}

std::uint32_t ReadRegisterValueForAddress(const CONTEXT* win32_context, ZydisRegister reg)
{
    if (reg == ZYDIS_REGISTER_NONE)
    {
        return 0;
    }
    switch (reg)
    {
        case ZYDIS_REGISTER_EAX: return win32_context->Eax;
        case ZYDIS_REGISTER_ECX: return win32_context->Ecx;
        case ZYDIS_REGISTER_EDX: return win32_context->Edx;
        case ZYDIS_REGISTER_EBX: return win32_context->Ebx;
        case ZYDIS_REGISTER_ESP: return win32_context->Esp;
        case ZYDIS_REGISTER_EBP: return win32_context->Ebp;
        case ZYDIS_REGISTER_ESI: return win32_context->Esi;
        case ZYDIS_REGISTER_EDI: return win32_context->Edi;
        default: return 0;
    }
}

void WriteRegisterFromZydis(CONTEXT* win32_context, ZydisRegister reg, std::uint32_t value)
{
    switch (reg)
    {
        case ZYDIS_REGISTER_EAX: win32_context->Eax = value; break;
        case ZYDIS_REGISTER_ECX: win32_context->Ecx = value; break;
        case ZYDIS_REGISTER_EDX: win32_context->Edx = value; break;
        case ZYDIS_REGISTER_EBX: win32_context->Ebx = value; break;
        case ZYDIS_REGISTER_ESP: win32_context->Esp = value; break;
        case ZYDIS_REGISTER_EBP: win32_context->Ebp = value; break;
        case ZYDIS_REGISTER_ESI: win32_context->Esi = value; break;
        case ZYDIS_REGISTER_EDI: win32_context->Edi = value; break;

        case ZYDIS_REGISTER_AX: win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_CX: win32_context->Ecx = (win32_context->Ecx & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_DX: win32_context->Edx = (win32_context->Edx & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_BX: win32_context->Ebx = (win32_context->Ebx & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_SP: win32_context->Esp = (win32_context->Esp & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_BP: win32_context->Ebp = (win32_context->Ebp & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_SI: win32_context->Esi = (win32_context->Esi & 0xFFFF0000U) | (value & 0xFFFFU); break;
        case ZYDIS_REGISTER_DI: win32_context->Edi = (win32_context->Edi & 0xFFFF0000U) | (value & 0xFFFFU); break;

        case ZYDIS_REGISTER_AL: win32_context->Eax = (win32_context->Eax & 0xFFFFFF00U) | (value & 0xFFU); break;
        case ZYDIS_REGISTER_CL: win32_context->Ecx = (win32_context->Ecx & 0xFFFFFF00U) | (value & 0xFFU); break;
        case ZYDIS_REGISTER_DL: win32_context->Edx = (win32_context->Edx & 0xFFFFFF00U) | (value & 0xFFU); break;
        case ZYDIS_REGISTER_BL: win32_context->Ebx = (win32_context->Ebx & 0xFFFFFF00U) | (value & 0xFFU); break;

        case ZYDIS_REGISTER_AH: win32_context->Eax = (win32_context->Eax & 0xFFFF00FFU) | ((value & 0xFFU) << 8); break;
        case ZYDIS_REGISTER_CH: win32_context->Ecx = (win32_context->Ecx & 0xFFFF00FFU) | ((value & 0xFFU) << 8); break;
        case ZYDIS_REGISTER_DH: win32_context->Edx = (win32_context->Edx & 0xFFFF00FFU) | ((value & 0xFFU) << 8); break;
        case ZYDIS_REGISTER_BH: win32_context->Ebx = (win32_context->Ebx & 0xFFFF00FFU) | ((value & 0xFFU) << 8); break;

        default: break;
    }
}

bool HandleGuestLowMemoryReadFault(CONTEXT* win32_context, ThreadContext* context, std::uint32_t fault_va, std::uint32_t decode_eip)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return false;
    }

    context->debug_emulate_stage = 1; // Started



    // Decode the instruction that is actually about to execute, not the guest
    // address it maps back to. On a fault EIP points at the faulting
    // instruction, so this is also what we must step over afterwards; under AOT
    // the two addresses differ and only this one describes the running code.
    const std::uint32_t execute_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    const std::uint8_t* instruction_ptr = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(execute_eip));

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        context->debug_emulate_stage = 2; // Decoder init failed
        return false;
    }

    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};

    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, instruction_ptr, 15, &instruction, operands)))
    {
        context->debug_emulate_stage = 3; // Decode failed
        return false;
    }

    if (instruction.mnemonic != ZYDIS_MNEMONIC_MOV &&
        instruction.mnemonic != ZYDIS_MNEMONIC_MOVZX &&
        instruction.mnemonic != ZYDIS_MNEMONIC_MOVSX)
    {
        context->debug_emulate_stage = 4; // Wrong mnemonic
        context->debug_emulate_decode_result = instruction.mnemonic;
        return false;
    }

    if (instruction.operand_count_visible < 2)
    {
        context->debug_emulate_stage = 5; // Invisible operands too few
        return false;
    }

    if (operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        operands[1].type != ZYDIS_OPERAND_TYPE_MEMORY)
    {
        context->debug_emulate_stage = 6; // Operand types mismatch
        return false;
    }

    const auto& mem = operands[1].mem;
    std::uint32_t base_val = ReadRegisterValueForAddress(win32_context, mem.base);
    std::uint32_t index_val = ReadRegisterValueForAddress(win32_context, mem.index);
    std::uint32_t scale = mem.scale;
    std::uint32_t displacement = mem.disp.has_displacement ? static_cast<std::uint32_t>(mem.disp.value) : 0;
    
    std::uint32_t calculated_address = base_val + index_val * scale + displacement;

    if (calculated_address >= repiu::runtime::kDosLowMemorySize)
    {
        context->debug_emulate_stage = 7; // Out of range calculated address
        context->debug_emulate_calculated_address = calculated_address;
        return false;
    }

    // Runaway guard. This must not fire on *legitimate* repetition: the known
    // caller is a byte load inside a shared stricmp, and a single filename check
    // calls it once per candidate extension ("tga", "pcx", "ptx", "rgb"), so the
    // same EIP and address recur in a tight burst by design. While this handler
    // rewrote the instruction as NOPs the case could not arise -- the first hit
    // destroyed the instruction -- so the guard only became reachable once the
    // instruction was preserved, and it then aborted the run as a false
    // positive. Count only repeats that made no forward progress, which is what
    // "runaway" actually means now that we step EIP over the load.
    std::uint32_t current_tick = GetTickCount();
    std::uint32_t elapsed_ticks = current_tick - context->last_low_memory_fault_tick;
    context->last_low_memory_fault_tick = current_tick;

    // Stepping EIP past the load makes forward progress structural: the guest
    // cannot be pinned on this instruction by us, only by its own control flow.
    // So the guard no longer needs a tight time-based trip. Keep a high absolute
    // cap purely as a pathology backstop, and reset it whenever the site moves.
    constexpr std::uint32_t kLowMemoryRunawayCap = 100000U;
    (void)elapsed_ticks;

    if (execute_eip == context->last_low_memory_fault_eip &&
        calculated_address == context->last_low_memory_fault_address)
    {
        context->low_memory_fault_repeat_count++;
        if (context->low_memory_fault_repeat_count >= kLowMemoryRunawayCap)
        {
            context->debug_emulate_stage = 99; // Runaway abort
            return false;
        }
    }
    else
    {
        context->last_low_memory_fault_eip = execute_eip;
        context->last_low_memory_fault_address = calculated_address;
        context->low_memory_fault_repeat_count = 1;
    }

    std::uint32_t read_width_bits = operands[1].size;
    std::uint32_t read_width_bytes = read_width_bits / 8;
    if (read_width_bytes == 0)
    {
        read_width_bytes = 1;
    }

    std::uint32_t raw_val = 0;
    if (read_width_bytes == 1)
    {
        std::uint8_t byte_val = 0;
        repiu::runtime::ReadDosLowMemoryUInt8(context->dos_low_memory, calculated_address, &byte_val);
        raw_val = byte_val;
    }
    else if (read_width_bytes == 2)
    {
        std::uint16_t word_val = 0;
        repiu::runtime::ReadDosLowMemoryUInt16(context->dos_low_memory, calculated_address, &word_val);
        raw_val = word_val;
    }
    else if (read_width_bytes == 4)
    {
        std::uint32_t dword_val = 0;
        repiu::runtime::ReadDosLowMemoryUInt32(context->dos_low_memory, calculated_address, &dword_val);
        raw_val = dword_val;
    }

    std::uint32_t final_val = raw_val;
    if (instruction.mnemonic == ZYDIS_MNEMONIC_MOVSX)
    {
        if (read_width_bytes == 1)
        {
            final_val = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(raw_val)));
        }
        else if (read_width_bytes == 2)
        {
            final_val = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(raw_val)));
        }
    }

    WriteRegisterFromZydis(win32_context, operands[0].reg.value, final_val);

    // Diagnostic (env-gated): this path permanently rewrites the guest
    // instruction as NOPs below. That is safe only if the instruction belongs
    // to code that never legitimately reads a non-zero address -- and the known
    // caller is `mov al,[ebx]` inside a *shared* stricmp, where NOPing the byte
    // load would break every later comparison, not just the null one. Log which
    // instruction is being destroyed and how often so the blast radius is
    // measurable before changing the strategy.
    // Step over the emulated load instead of rewriting it. The previous version
    // overwrote the instruction with NOPs, which resumed execution but did so by
    // destroying guest code permanently. The only observed site is
    // `mov al,[ebx]` inside a *shared* stricmp, so one null-string comparison
    // removed the byte load for every later call -- silently corrupting every
    // filename-extension comparison in the game rather than crashing.
    win32_context->Eip = execute_eip + instruction.length;

    {
        static const bool low_mem_trace_enabled =
            std::getenv("REPIU_LOWMEM_TRACE") != nullptr;
        if (low_mem_trace_enabled)
        {
            static long low_mem_trace_count = 0;
            static std::uint32_t seen_eips[32] = {};
            static long seen_count = 0;
            const long index = InterlockedIncrement(&low_mem_trace_count);
            bool first_for_eip = true;
            for (long i = 0; i < seen_count; ++i)
            {
                if (seen_eips[i] == execute_eip)
                {
                    first_for_eip = false;
                    break;
                }
            }
            if (first_for_eip && seen_count < 32)
            {
                seen_eips[seen_count++] = execute_eip;
            }
            if (first_for_eip || index <= 40)
            {
                fprintf(stderr,
                        "[repiu-lowmem] #%ld %s guest_eip=0x%08X exec_eip=0x%08X"
                        " fault_va=0x%08X len=%u value=0x%X -> stepped over\n",
                        index, first_for_eip ? "NEW-SITE" : "repeat",
                        decode_eip, execute_eip, calculated_address,
                        instruction.length, final_val);
            }
        }
    }

    context->debug_emulate_stage = 100; // Success
    context->debug_emulate_decode_result = instruction.length;
    context->debug_emulate_calculated_address = operands[0].reg.value;

    context->low_memory_read_emulate_count++;
    context->last_low_memory_read_emulate_address = calculated_address;
    context->last_low_memory_read_emulate_eip = decode_eip;
    context->last_low_memory_read_emulate_value = final_val;
    context->last_low_memory_read_emulate_reg = operands[0].reg.value;

    RecordLowMemoryAccess(win32_context, context, instruction_ptr[0], calculated_address, final_val);

    return true;
}

bool HandleDosMemoryAccess(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] == 0x66 && instruction[1] == 0x26 &&
        instruction[2] == 0x8B && instruction[3] == 0x0D)
    {
        const std::uint32_t offset =
            static_cast<std::uint32_t>(instruction[4]) |
            (static_cast<std::uint32_t>(instruction[5]) << 8) |
            (static_cast<std::uint32_t>(instruction[6]) << 16) |
            (static_cast<std::uint32_t>(instruction[7]) << 24);
        std::uint16_t value = 0;
        if (offset == 0x2C)
        {
            value = context->linexe_environment_active ? 0x002CU : 0;
        }
        win32_context->Ecx =
            (win32_context->Ecx & 0xFFFF0000U) | value;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[2],
                              offset,
                              value);
        win32_context->Eip += 8;
        return true;
    }
    if (instruction[0] == 0x66 && instruction[1] == 0x26 &&
        instruction[2] == 0x8C && instruction[3] == 0x1D)
    {
        const std::uint32_t offset =
            static_cast<std::uint32_t>(instruction[4]) |
            (static_cast<std::uint32_t>(instruction[5]) << 8) |
            (static_cast<std::uint32_t>(instruction[6]) << 16) |
            (static_cast<std::uint32_t>(instruction[7]) << 24);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[2],
                              offset,
                              0);
        win32_context->Eip += 8;
        return true;
    }
    if (instruction[0] == 0x26 && instruction[1] == 0x8A &&
        instruction[2] == 0x4F && instruction[3] == 0xFF)
    {
        win32_context->Ecx &= 0xFFFFFF00U;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[1],
                              win32_context->Edi - 1,
                              0);
        win32_context->Eip += 4;
        return true;
    }
    if (instruction[0] == 0x8B && instruction[1] == 0x06 &&
        win32_context->Esi < 0x10000)
    {
        win32_context->Eax = 0;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Esi,
                              0);
        win32_context->Eip += 2;
        return true;
    }
    if (instruction[0] == 0x80 && instruction[1] == 0x3E &&
        instruction[2] == 0x00 && win32_context->Esi < 0x10000)
    {
        win32_context->EFlags |= 0x40U;
        win32_context->EFlags &= ~1U;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Esi,
                              0);
        win32_context->Eip += 3;
        return true;
    }
    if (instruction[0] == 0xAC && win32_context->Esi < 0x10000)
    {
        win32_context->Eax &= 0xFFFFFF00U;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Esi,
                              0);
        ++win32_context->Esi;
        ++win32_context->Eip;
        return true;
    }
    if (instruction[0] == 0xA4 && win32_context->Esi < 0x10000)
    {
        char* destination = reinterpret_cast<char*>(
            static_cast<std::uintptr_t>(win32_context->Edi));
        if (destination != nullptr &&
            IsGuestRangeReadable(context, destination, 1))
        {
            *destination = '\0';
        }
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Edi,
                              0);
        ++win32_context->Esi;
        ++win32_context->Edi;
        ++win32_context->Eip;
        return true;
    }

    return false;
}

extern "C" __declspec(naked) void
RecoverHostStackException()
{
    __asm
    {
        xor eax, eax
        ret
    }
}


#endif

#if defined(_MSC_VER) && defined(_M_IX86)
// Task 323 denominator: the whole guest execution window on this thread. The
// scope lives here rather than in GuestEntryThreadProc because that function
// uses __try, and MSVC rejects objects requiring unwinding in the same function
// (C2712).
void CallGuestEntryWithStackTimed(StackSwitchCallState* state,
                                  ThreadContext* context)
{
    const ExecutionTimeScope guest_run_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kGuestRunTotal);
    CallGuestEntryWithStack(state);
}

// Same denominator for the non-stack-switching entry path. Both branches must
// be instrumented or the guest-run total silently stays zero.
void CallGuestEntryDirectTimed(ThreadContext* context)
{
    const ExecutionTimeScope guest_run_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kGuestRunTotal);
    using EntryFunction = void (*)();
    EntryFunction entry = reinterpret_cast<EntryFunction>(
        static_cast<std::uintptr_t>(context->entry_address));
    entry();
}
#endif

DWORD WINAPI GuestEntryThreadProc(void* parameter)
{
    ThreadContext* context = static_cast<ThreadContext*>(parameter);
    if (context == nullptr)
    {
        return 1;
    }
    context->guest_thread_id = GetCurrentThreadId();

    __try
    {
        if (context->use_guest_stack)
        {
#if defined(_MSC_VER) && defined(_M_IX86)
            StackSwitchCallState state;
            state.entry_address = context->entry_address;
            state.initial_esp = context->guest_initial_esp;
            state.enable_single_step_trace =
                context->enable_single_step_trace ? 1U : 0U;

            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<void*>(context->guest_initial_esp - 4), &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                state.guest_stack_limit = reinterpret_cast<std::uint32_t>(mbi.AllocationBase);
                state.guest_stack_base = context->guest_initial_esp;
            }
            context->active_call_state = &state;
            g_repiu_active_thread_context = context;
            context->vectored_handler = AddVectoredExceptionHandler(
                1, GuestStackVectoredExceptionHandler);
            if (context->vectored_handler == nullptr)
            {
                g_repiu_active_thread_context = nullptr;
                context->active_call_state = nullptr;
                return 5;
            }

            CallGuestEntryWithStackTimed(&state, context);

            g_repiu_active_thread_context = nullptr;
            context->active_call_state = nullptr;
            context->host_esp = state.host_esp;
            if (context->guest_return_esp == 0)
            {
                context->guest_return_esp = state.guest_return_esp;
            }
            if (context->exception_caught)
            {
                return 2;
            }
            if (context->process_exit)
            {
                return 0;
            }
#else
            return 4;
#endif
        }
        else
        {
#if defined(_MSC_VER) && defined(_M_IX86)
            g_repiu_active_thread_context = context;
            context->vectored_handler = AddVectoredExceptionHandler(
                1, GuestStackVectoredExceptionHandler);
            if (context->vectored_handler == nullptr)
            {
                g_repiu_active_thread_context = nullptr;
                return 5;
            }
#endif
            CallGuestEntryDirectTimed(context);
#if defined(_MSC_VER) && defined(_M_IX86)
            g_repiu_active_thread_context = nullptr;
            if (context->process_exit)
            {
                return 0;
            }
#endif
        }
        context->returned = true;
        return 0;
    }
    __except (CaptureException(GetExceptionInformation(), context))
    {
        return 2;
    }
}
#endif

}  // namespace

bool DispatchGuestHleInstruction(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    return DispatchGuestHleHandlers(win32_context, context);
}

// Execution probe/trace + guest-IP helpers promoted to external linkage for
// aot_runtime_dispatch.cpp (relocated out of the anonymous namespace).
// Task 342. Restores the pre-Task-342 policy for A/B in one binary.
bool QuarantineOnFirstWriteEnabled()
{
    static const bool enabled = []() {
        const char* value = std::getenv("REPIU_AOT_QUARANTINE_FIRST_WRITE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

// Task 342. Counts how often a page has been written from itself and answers
// whether this write should quarantine. The first writes only retire, which
// already stops the cache executing stale bytes; quarantine is the churn
// defence and waits for evidence of repetition. A page evicted from the table
// quarantines on sight, so overflow degrades to the old policy.
bool ShouldQuarantineWrittenPage(ThreadContext* context,
                                 std::uint32_t page,
                                 std::uint32_t destination)
{
    // Task 344: quarantine when the same address is rewritten repeatedly, which
    // is real churn, or when a page accumulates far more writes than a handful
    // of one-shot patches would explain, which bounds the pathological case a
    // per-address rule alone would miss.
    constexpr std::uint32_t kRepeatWriteThreshold = 4U;
    constexpr std::uint32_t kPageWriteCeiling = 32U;
    if (context == nullptr || QuarantineOnFirstWriteEnabled())
    {
        return true;
    }
    for (std::uint32_t index = 0;
         index < context->guest_page_write_history_size; ++index)
    {
        ThreadContext::GuestPageWriteRecord& record =
            context->guest_page_write_history[index];
        if (record.page != page)
        {
            continue;
        }
        ++record.count;
        if (record.last_destination == destination)
        {
            ++record.repeat_count;
        }
        else
        {
            record.last_destination = destination;
            record.repeat_count = 1U;
        }
        if (record.repeat_count >= kRepeatWriteThreshold ||
            record.count >= kPageWriteCeiling)
        {
            return true;
        }
        ++context->quarantine_deferred_count;
        return false;
    }
    if (context->guest_page_write_history_size >=
        ThreadContext::kGuestPageWriteHistoryCapacity)
    {
        ++context->guest_page_write_history_overflow;
        return true;
    }
    context->guest_page_write_history[
        context->guest_page_write_history_size] = {page, 1U, destination, 1U};
    ++context->guest_page_write_history_size;
    ++context->quarantine_deferred_count;
    return false;
}

// Task 337. Buckets are 1, 2, 3, 4, 5-8, 9-16, 17-32, 33+, so a uniform cost
// and a long tail are told apart without storing every run.
std::uint32_t SingleStepRunBucket(std::uint32_t length)
{
    if (length <= 4U)
    {
        return length - 1U;
    }
    if (length <= 8U)
    {
        return 4U;
    }
    if (length <= 16U)
    {
        return 5U;
    }
    if (length <= 32U)
    {
        return 6U;
    }
    return 7U;
}

void RecordVehExceptionCensus(ThreadContext* context, DWORD code)
{
    if (context == nullptr)
    {
        return;
    }
    // Task 372: the exception code is validated by the time the census runs, so
    // this is where the gap the VEH scope banked gets its class. Single step is
    // the class that reads as a pure kernel round trip, since the guest executes
    // exactly one instruction between two consecutive single steps.
    RecordVehExceptionGap(
        context->execution_time_profile.get(),
        code == EXCEPTION_SINGLE_STEP  ? VehGapClass::kSingleStep
            : code == EXCEPTION_BREAKPOINT ? VehGapClass::kBreakpoint
                                           : VehGapClass::kOther);
    if (code == EXCEPTION_SINGLE_STEP)
    {
        ++context->veh_single_step_exception_count;
        ++context->veh_single_step_run_length;
        return;
    }
    if (code == EXCEPTION_BREAKPOINT)
    {
        ++context->veh_breakpoint_exception_count;
    }
    else if (code == EXCEPTION_ACCESS_VIOLATION)
    {
        ++context->veh_access_violation_exception_count;
    }
    else
    {
        ++context->veh_other_exception_count;
        // Task 343: name the code rather than bucketing it as "other".
        bool recorded = false;
        for (std::uint32_t index = 0;
             index < ThreadContext::kOtherExceptionCodeCapacity; ++index)
        {
            if (context->veh_other_exception_code_counts[index] == 0U ||
                context->veh_other_exception_codes[index] ==
                    static_cast<std::uint32_t>(code))
            {
                context->veh_other_exception_codes[index] =
                    static_cast<std::uint32_t>(code);
                ++context->veh_other_exception_code_counts[index];
                recorded = true;
                break;
            }
        }
        if (!recorded)
        {
            ++context->veh_other_exception_code_overflow;
        }
    }
    if (context->veh_single_step_run_length != 0U)
    {
        const std::uint32_t length = context->veh_single_step_run_length;
        context->veh_single_step_run_buckets[SingleStepRunBucket(length)] += 1U;
        ++context->veh_single_step_run_total;
        context->veh_single_step_run_max =
            context->veh_single_step_run_max > length
                ? context->veh_single_step_run_max : length;
        context->veh_single_step_run_length = 0U;
    }
}

bool IsGuestInstructionPointer(const ThreadContext* context,
                               std::uint32_t eip)
{
    if (context == nullptr || context->runtime_size == 0)
    {
        return false;
    }

    const std::uint32_t runtime_end =
        context->runtime_base + context->runtime_size;
    return eip >= context->runtime_base &&
           runtime_end >= context->runtime_base &&
           eip < runtime_end;
}

void RecordExecutionProbe(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->execution_probe_configured || context->execution_probe_hit ||
        win32_context->Eip < context->runtime_base ||
        static_cast<std::uint32_t>(win32_context->Eip) -
                context->runtime_base != context->execution_probe_offset)
    {
        return;
    }
    context->execution_probe_hit = true;
    CopySnapshotFromContextRecord(*win32_context,
                                  &context->execution_probe_snapshot);
    const void* stack = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context, stack,
                             sizeof(context->execution_probe_stack)))
    {
        std::memcpy(context->execution_probe_stack, stack,
                    sizeof(context->execution_probe_stack));
    }
}

void RecordExecutionTrace(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->execution_trace_configured ||
        win32_context->Eip < context->runtime_base)
    {
        return;
    }
    const std::uint32_t offset = static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (offset < context->execution_trace_start_offset ||
        offset > context->execution_trace_end_offset)
    {
        return;
    }
    Win32ExecutionTraceEntry entry{};
    entry.sequence = context->execution_trace_hit_count;
    entry.eip = static_cast<std::uint32_t>(win32_context->Eip);
    entry.esp = static_cast<std::uint32_t>(win32_context->Esp);
    const void* value_address = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(entry.esp + context->execution_trace_esp_offset));
    if (IsGuestRangeReadable(context, value_address, sizeof(entry.value_at_esp_offset)))
    {
        std::memcpy(&entry.value_at_esp_offset, value_address,
                    sizeof(entry.value_at_esp_offset));
    }
    const std::uint32_t slot = context->execution_trace_hit_count %
        kWin32ExecutionTraceCapacity;
    context->execution_trace[slot] = entry;
    ++context->execution_trace_hit_count;
}

// ResolveSegmentLinearRange promoted to external linkage (relocated out of the
// anonymous namespace) for dpmi_mscdex_services.cpp and segment handlers.
bool ResolveSegmentLinearRange(ThreadContext* context,
                               std::uint16_t selector,
                               std::uint32_t offset,
                               std::uint32_t byte_count,
                               bool writable,
                               std::uint32_t* linear_address)
{
    if (context == nullptr || linear_address == nullptr)
    {
        return false;
    }

    std::uint32_t translated = 0;
    if (!repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            byte_count,
            &translated))
    {
        translated = offset;
    }
    void* pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(translated));
    const bool valid = writable
        ? IsGuestRangeWritable(context, pointer, byte_count)
        : IsGuestRangeReadable(context, pointer, byte_count);
    if (!valid)
    {
        translated = offset;
        pointer = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(translated));
        const bool direct_valid = writable
            ? IsGuestRangeWritable(context, pointer, byte_count)
            : IsGuestRangeReadable(context, pointer, byte_count);
        if (!direct_valid)
        {
            return false;
        }
    }
    *linear_address = translated;
    return true;
}


// Shared trap/record substrate promoted to external linkage (relocated out of
// the anonymous namespace) for use by DOS/DPMI/MSCDEX/segment modules.
// Declared in execution_internal.h.

void RecoverFromHleExit(CONTEXT* win32_context,
                        ThreadContext* thread_context)
{
    thread_context->process_exit = true;
    thread_context->returned = true;
    if (thread_context->use_guest_stack)
    {
        RecoverToHost(win32_context, thread_context);
    }
    else
    {
        win32_context->Eip =
            reinterpret_cast<DWORD_PTR>(&RecoverHostStackException);
    }
}

void RecordHandledDosInterrupt(ThreadContext* context,
                               std::uint8_t vector,
                               std::uint16_t ax)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_interrupt_count;
    context->last_dos_interrupt_vector = vector;
    context->last_dos_interrupt_ah = static_cast<std::uint8_t>(ax >> 8);
    context->last_dos_interrupt_ax = ax;
}

void RecordLowMemoryAccess(CONTEXT* win32_context,
                           ThreadContext* context,
                           std::uint8_t opcode,
                           std::uint32_t destination,
                           std::uint32_t value)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_low_memory_access_count;
    context->last_low_memory_access_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_low_memory_access_opcode = opcode;
    context->last_low_memory_access_esi = win32_context->Esi;
    context->last_low_memory_access_edi = win32_context->Edi;
    context->last_low_memory_access_destination = destination;
    context->last_low_memory_access_value = value;
}

std::uint16_t ReadGuestSegmentSelector(const ThreadContext& context,
                                       std::uint8_t segment_register,
                                       const CONTEXT* win32_context)
{
    std::uint16_t shadow = 0;
    switch (segment_register)
    {
        case 0:
            shadow = context.guest_es;
            break;
        case 2:
            shadow = context.guest_ss;
            break;
        case 3:
            shadow = context.guest_ds;
            break;
        case 4:
            shadow = context.guest_fs;
            break;
        case 5:
            shadow = context.guest_gs;
            break;
        default:
            return 0;
    }
    if (win32_context == nullptr)
    {
        return shadow;
    }

    std::uint16_t physical = shadow;
    std::uint32_t host_entry = 0;
    switch (segment_register)
    {
        case 0:
            physical = static_cast<std::uint16_t>(win32_context->SegEs);
            host_entry = g_recovery_host_es;
            break;
        case 2:
            return static_cast<std::uint16_t>(win32_context->SegSs);
        case 3:
            physical = static_cast<std::uint16_t>(win32_context->SegDs);
            host_entry = g_recovery_host_ds;
            break;
        case 4:
            physical = static_cast<std::uint16_t>(win32_context->SegFs);
            host_entry = g_recovery_host_fs;
            break;
        case 5:
            physical = static_cast<std::uint16_t>(win32_context->SegGs);
            host_entry = g_recovery_host_gs;
            break;
        default:
            return shadow;
    }
    if (physical == shadow)
    {
        return physical;
    }
    if (context.shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context.shared_live_telemetry->seg_divergence_count);
        InterlockedExchange(
            &context.shared_live_telemetry->seg_divergence_reg_physical,
            static_cast<long>((static_cast<std::uint32_t>(segment_register)
                               << 16) | physical));
        InterlockedExchange(
            &context.shared_live_telemetry->seg_divergence_shadow,
            static_cast<long>(shadow));
    }
    // A selector that only exists in the software SelectorTable (for example
    // the DOS/4G client-data selector installed by INT 21h AX=FF00h) can
    // never be loaded into the hardware register, so the physical value
    // still holding the host's entry-time selector means the HLE shadow is
    // authoritative. A physical value that moved away from the host entry
    // selector was loaded by the guest (natively or via write-through) and
    // wins over a possibly stale shadow.
    if (physical == static_cast<std::uint16_t>(host_entry & 0xFFFFU) &&
        shadow != 0)
    {
        const repiu::runtime::GuestDescriptor* descriptor =
            repiu::runtime::FindDescriptor(context.selector_table, shadow);
        if (descriptor != nullptr && descriptor->present)
        {
            return shadow;
        }
    }
    return physical;
}

// NoteSuccessfulAotGuestWrite (AOT write hook) promoted to external linkage
// for guest_memory_access.cpp; relocated out of the anonymous namespace.
bool NoteSuccessfulAotGuestWrite(ThreadContext* context,
                                 std::uint32_t destination,
                                 std::uint32_t byte_count)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        context->aot_translation_thread == nullptr || byte_count == 0U)
    {
        return true;
    }
    constexpr std::uint32_t kPageMask = 0xFFFFF000U;
    const std::uint32_t first_page = destination & kPageMask;
    const std::uint64_t write_end =
        static_cast<std::uint64_t>(destination) + byte_count;
    if (write_end == 0U ||
        write_end > static_cast<std::uint64_t>(
                        std::numeric_limits<std::uint32_t>::max()) + 1U)
    {
        return false;
    }
    const std::uint32_t last_page = static_cast<std::uint32_t>(
        (write_end - 1U) & kPageMask);
    bool relevant = Win32AotGuestRangeHasActiveTranslation(
        *context->aot_placement, destination, byte_count);
    for (std::uint32_t page = first_page;
         !relevant; page += 0x1000U)
    {
        relevant = IsWin32AotGuestPageRetired(
                       *context->aot_placement, page) ||
            IsWin32AotGuestPageQuarantined(
                *context->aot_placement, page);
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    if (!relevant)
    {
        return true;
    }
    const std::uint32_t source = AotGuestAddressForExecutionAddress(
        context,
        context->exception_dispatch_last_eip.load(
            std::memory_order_relaxed));
    bool observed = false;
    bool retired_provenance = false;
    for (std::uint32_t page = first_page;; page += 0x1000U)
    {
        const std::uint32_t range_begin = std::max(destination, page);
        const std::uint64_t page_end =
            static_cast<std::uint64_t>(page) + 0x1000U;
        const std::uint32_t range_size = static_cast<std::uint32_t>(
            std::min(write_end, page_end) - range_begin);
        const bool active = Win32AotGuestRangeHasActiveTranslation(
            *context->aot_placement, range_begin, range_size);
        retired_provenance = retired_provenance ||
            IsWin32AotGuestPageRetired(*context->aot_placement, page) ||
            IsWin32AotGuestPageQuarantined(
                *context->aot_placement, page);
        if (active)
        {
            const bool writes_own_page = source == 0U ||
                (source & kPageMask) == page;
            // Task 342: quarantine on repetition, not on the first write. See
            // docs/design/20260728-342-quarantine-on-repeat-write.md section 2
            // for why this does not weaken correctness.
            const bool same_page = writes_own_page &&
                ShouldQuarantineWrittenPage(context, page, destination);
            // Task 341: record what quarantines a page, since four of them
            // block 80.24% of post-HLE returns. An unknown source is counted
            // separately because it quarantines by default rather than by
            // evidence of self-modification.
            if (same_page)
            {
                if (source == 0U)
                {
                    ++context->quarantine_unknown_source_count;
                }
                if (context->quarantine_trace_count <
                    ThreadContext::kQuarantineTraceCapacity)
                {
                    context->quarantine_trace[
                        context->quarantine_trace_count] = {
                        page, source, destination, byte_count};
                }
                ++context->quarantine_trace_count;
            }
            BumpAotPageRetireAttemptCount(context);
            if (!RequestAotGuestPageRetirement(
                    context, page, same_page))
            {
                context->aot_terminal_failure.store(
                    true, std::memory_order_release);
                return false;
            }
            BumpAotPageRetireSuccessCount(context);
            context->aot_last_retired_page.store(
                page, std::memory_order_relaxed);
            if (context->shared_live_telemetry != nullptr)
            {
                InterlockedExchange(
                    &context->shared_live_telemetry->aot_last_retired_page,
                    static_cast<long>(page));
            }
            if (same_page)
            {
                BumpAotQuarantineCount(context);
            }
            observed = true;
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    if (observed || retired_provenance)
    {
        context->aot_code_write_count.fetch_add(
            1, std::memory_order_relaxed);
        context->aot_last_code_write_source.store(
            source, std::memory_order_relaxed);
        context->aot_last_code_write_destination.store(
            destination, std::memory_order_relaxed);
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->aot_last_code_write_source,
                static_cast<long>(source));
            InterlockedExchange(
                &context->shared_live_telemetry
                     ->aot_last_code_write_destination,
                static_cast<long>(destination));
        }
    }
    return true;
}

std::uint32_t InjectPendingInterrupts(CONTEXT* win32_context,
                                      ThreadContext* context)
{
    if (!context->timer_interrupt_pending.load(std::memory_order_acquire))
    {
        return 0U;
    }

    const DpmiInterruptVectorShadow& shadow = context->dpmi_interrupt_vectors[0x08];
    if (!shadow.valid)
    {
        context->timer_interrupt_pending.store(false, std::memory_order_relaxed);
        ClearAotTimerSafePointRequest(context);
        context->timer_interrupt_due_ticks.store(
            0U, std::memory_order_relaxed);
        // Task 366: delivery is abandoned with no vector to reach, so the owed
        // ticks are accounted rather than silently vanishing from the identity.
        RecordTimerTickBacklogCleared(&context->timer_tick_delivery);
        return 0U;
    }

    // Task 366: these two are delays, not losses -- the pending state survives
    // and the tick lands at the next safe moment. Counting them separately keeps
    // them out of the loss figure.
    if (!IsGuestInstructionPointer(context, static_cast<std::uint32_t>(win32_context->Eip)) &&
        !IsAotCacheAddress(context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        RecordTimerTickDeferred(&context->timer_tick_delivery);
        return 0U;
    }

    if ((win32_context->EFlags & kEFlagsInterruptEnable) == 0U)
    {
        RecordTimerTickDeferred(&context->timer_tick_delivery);
        return 0U;
    }

    // Task 366: with the backlog opt-in off this returns false and the two lines
    // below behave exactly as before -- one injection, flag cleared. With it on,
    // a still-owed tick keeps delivery armed so the backlog drains one interrupt
    // per safe point rather than bursting into the guest stack.
    const bool keep_armed = RecordTimerTickInjected(
        &context->timer_tick_delivery, TimerTickBacklogEnabled());
    context->timer_interrupt_pending.store(keep_armed,
                                           std::memory_order_relaxed);
    if (keep_armed)
    {
        ArmAotTimerSafePoint(context);
    }
    else
    {
        ClearAotTimerSafePointRequest(context);
    }
    const std::uint32_t consumed_ticks =
        context->timer_interrupt_due_ticks.exchange(
            0U, std::memory_order_acq_rel);

    std::uint32_t eflags = win32_context->EFlags;
    std::uint32_t segcs = win32_context->SegCs;
    std::uint32_t eip = win32_context->Eip;

    std::uint32_t esp = win32_context->Esp;
    esp -= 4;
    WriteGuestBytes(context, reinterpret_cast<void*>(static_cast<std::uintptr_t>(esp)), &eflags, 4);
    esp -= 4;
    WriteGuestBytes(context, reinterpret_cast<void*>(static_cast<std::uintptr_t>(esp)), &segcs, 4);
    esp -= 4;
    WriteGuestBytes(context, reinterpret_cast<void*>(static_cast<std::uintptr_t>(esp)), &eip, 4);

    win32_context->Esp = esp;
    win32_context->SegCs = shadow.selector;
    win32_context->Eip = shadow.offset;
    win32_context->EFlags &= ~(kEFlagsInterruptEnable | 0x100U);

    // The timer tick is injected continuously while the guest runs, so this
    // line floods the console and drowns out everything else. Keep it as an
    // opt-in diagnostic (REPIU_TIMER_INJECT_LOG=1) instead of default output.
    static const bool timer_injection_log_enabled =
        std::getenv("REPIU_TIMER_INJECT_LOG") != nullptr;
    if (timer_injection_log_enabled)
    {
        fprintf(stderr, "[repiu-live] Injected INT 8, jump to %04X:%08X, return frame %08X, ticks=%u\n",
                shadow.selector, shadow.offset, esp,
                context->last_timer_injection_ticks.load(
                    std::memory_order_relaxed));
    }
    return consumed_ticks;
}

struct AotHleTranslationScope
{
    CONTEXT* win32_context;
    ThreadContext* context;
    std::uint32_t original_aot_eip;
    std::uint32_t guest_eip;
    bool is_aot_exception;

    AotHleTranslationScope(CONTEXT* wc, ThreadContext* ctx)
        : win32_context(wc), context(ctx), original_aot_eip(0U), guest_eip(0U), is_aot_exception(false)
    {
        if (context != nullptr && context->aot_placement != nullptr &&
            IsAotCacheAddress(context, static_cast<std::uint32_t>(win32_context->Eip)))
        {
            original_aot_eip = static_cast<std::uint32_t>(win32_context->Eip);
            std::uint32_t resolved_guest_eip = 0U;
            if (FindAotGuestAddress(*context->aot_placement, original_aot_eip, &resolved_guest_eip))
            {
                guest_eip = resolved_guest_eip;
                win32_context->Eip = guest_eip;
                is_aot_exception = true;
            }
        }
    }

    ~AotHleTranslationScope()
    {
        if (is_aot_exception)
        {
            if (static_cast<std::uint32_t>(win32_context->Eip) != guest_eip)
            {
                std::uint32_t new_cache_address = 0U;
                if (FindAotCacheAddress(*context->aot_placement,
                                        static_cast<std::uint32_t>(win32_context->Eip),
                                        &new_cache_address))
                {
                    win32_context->Eip = new_cache_address;
                }
            }
            else
            {
                win32_context->Eip = original_aot_eip;
            }
        }
    }
};

// Task 296: syscall-free plausibility gate for the exception-dispatch hot path.
// A real Windows EXCEPTION_POINTERS/CONTEXT/EXCEPTION_RECORD pointer is always
// well above the 64KB NULL-reserve region and pointer-aligned; the observed
// corruption (ContextRecord=0x23, a selector value used as a pointer) fails
// both tests. This runs on every VEH dispatch (up to ~166K/s during
// single-step-heavy phases), so it must stay branch-only -- the authoritative
// VirtualQuery check below is reserved for the rare implausible case.
static inline bool IsPlausibleHostPointer(const void* pointer)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
    return value >= 0x10000U && (value & 0x3U) == 0U;
}

// Task 296: VirtualQuery-based readability check for arbitrary host pointers.
// Unlike IsGuestRangeReadable (which only covers the guest arena), this can
// validate Windows-allocated structures such as CONTEXT/EXCEPTION_RECORD before
// they are dereferenced. Only invoked when IsPlausibleHostPointer already flags
// a pointer as suspicious, so its kernel-transition cost stays off the hot path.
static bool IsHostPointerReadable(const void* pointer, std::size_t byte_count)
{
    if (pointer == nullptr || byte_count == 0)
    {
        return false;
    }
    const std::uint8_t* cursor = static_cast<const std::uint8_t*>(pointer);
    const std::uint8_t* end = cursor + byte_count;
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(cursor, &info, sizeof(info)) == 0)
        {
            return false;
        }
        if (info.State != MEM_COMMIT)
        {
            return false;
        }
        const DWORD protect = info.Protect;
        if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0)
        {
            return false;
        }
        const DWORD access = protect & 0xFFU;
        const bool readable =
            access == PAGE_READONLY || access == PAGE_READWRITE ||
            access == PAGE_WRITECOPY || access == PAGE_EXECUTE_READ ||
            access == PAGE_EXECUTE_READWRITE ||
            access == PAGE_EXECUTE_WRITECOPY;
        if (!readable)
        {
            return false;
        }
        const std::uint8_t* region_end =
            static_cast<const std::uint8_t*>(info.BaseAddress) + info.RegionSize;
        if (region_end <= cursor)
        {
            return false; // defensive: guarantee forward progress
        }
        cursor = region_end;
    }
    return true;
}

// Task 296: record a malformed EXCEPTION_POINTERS and emit a one-line
// diagnostic. Safe to call even when the embedded pointers are unreadable.
static void RecordMalformedExceptionPointers(ThreadContext* context,
                                             EXCEPTION_POINTERS* exception_info)
{
    const std::uint32_t count =
        context->exception_dispatch_malformed_count.fetch_add(
            1U, std::memory_order_relaxed) +
        1U;
    std::uint32_t bad_context = 0U;
    std::uint32_t bad_record = 0U;
    if (IsHostPointerReadable(exception_info, sizeof(EXCEPTION_POINTERS)))
    {
        bad_context = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(exception_info->ContextRecord));
        bad_record = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(exception_info->ExceptionRecord));
    }
    context->exception_dispatch_last_bad_context.store(
        bad_context, std::memory_order_relaxed);
    context->exception_dispatch_last_bad_record.store(
        bad_record, std::memory_order_relaxed);
    fprintf(stderr,
            "[repiu-live] Malformed EXCEPTION_POINTERS at VEH: info=%p "
            "ExceptionRecord=0x%08X ContextRecord=0x%08X (count=%u) -- failing "
            "closed to surface primary exception\n",
            static_cast<void*>(exception_info), bad_record, bad_context, count);
}

// VEH dispatch logic relocated out of the anonymous namespace (external
// linkage) so the thin GuestStackVectoredExceptionHandler entry in
// exception_rescue_win32.cpp can forward to it. Declared in that header.
LONG DispatchGuestException(EXCEPTION_POINTERS* exception_info)
{
    ThreadContext* context = g_repiu_active_thread_context;
    if (context == nullptr || exception_info == nullptr)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Task 323: the single choke point for every exception the guest thread
    // takes -- single steps, INT3 boundaries, and access violations alike.
    // Measured here rather than per handler so no exception path escapes.
    const ExecutionTimeScope veh_time_scope(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kVehTotal);
    // Task 325: validation, call-step probe, and span/region teardown. Reset
    // explicitly once the prologue ends rather than relying on block scope,
    // because the prologue is not a single lexical block.
    std::optional<ExecutionTimeScope> prologue_time_scope;
    prologue_time_scope.emplace(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kVehPrologue);

    // Task 296: Windows can hand the VEH a malformed EXCEPTION_POINTERS (a
    // non-null but unreadable ContextRecord/ExceptionRecord) when a fault is
    // dispatched while the thread runs on the guest stack (TIB stack bounds
    // still point at the host stack). Dereferencing it caused a secondary
    // access violation inside this dispatcher (win32_context->Eip, CONTEXT
    // offset 0xB8) that masked the original guest exception. Validate before
    // any dereference and fail closed so the primary exception propagates to
    // the outer handler and is recorded.
    //
    // The common (valid) case is settled with branch-only arithmetic so the hot
    // exception path pays no kernel transition; only an implausible pointer
    // falls through to the authoritative VirtualQuery check. The `&&`
    // short-circuit keeps reads of exception_info->{Context,Exception}Record
    // guarded by the preceding IsPlausibleHostPointer(exception_info).
    if (!IsPlausibleHostPointer(exception_info) ||
        !IsPlausibleHostPointer(exception_info->ContextRecord) ||
        !IsPlausibleHostPointer(exception_info->ExceptionRecord))
    {
        if (!IsHostPointerReadable(exception_info, sizeof(EXCEPTION_POINTERS)) ||
            !IsHostPointerReadable(exception_info->ContextRecord,
                                   sizeof(CONTEXT)) ||
            !IsHostPointerReadable(exception_info->ExceptionRecord,
                                   sizeof(EXCEPTION_RECORD)))
        {
            RecordMalformedExceptionPointers(context, exception_info);
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    if (GetCurrentThreadId() != context->guest_thread_id)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (context->use_guest_stack &&
        (context->active_call_state == nullptr ||
         context->active_call_state->host_esp == 0))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    CONTEXT* win32_context = exception_info->ContextRecord;
    // Task 337: classify every exception exactly once, before any handler can
    // consume it, so the census is exclusive by construction. A single-step run
    // is closed by the next non-single-step exception, which is what makes the
    // bucket "instructions walked under TF between two boundaries".
    RecordVehExceptionCensus(context,
                             exception_info->ExceptionRecord->ExceptionCode);
    Win32UnhandledBreakpointEvidence breakpoint_evidence;
    if (exception_info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        breakpoint_evidence =
            CaptureBreakpointEvidence(exception_info, context);
    }

    if (win32_context->Eip == 0U)
    {
        win32_context->EFlags &= ~0x00000100U;
        DumpZeroReturnEvidence(
            win32_context, context, "zero-eip-fail-closed",
            context->exception_dispatch_last_eip.load(
                std::memory_order_relaxed));
        CaptureException(exception_info, context);
        context->guest_return_esp =
            static_cast<std::uint32_t>(win32_context->Esp);
        if (context->use_guest_stack)
        {
            RecoverToHost(win32_context, context);
        }
        else
        {
            win32_context->Eip =
                reinterpret_cast<DWORD_PTR>(&RecoverHostStackException);
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (HandleAotDbtCallStepProbe(
            exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    // Task 275 linear spans use Dr0 only. On the expected boundary, restore
    // debug state and deliberately continue through the normal #DB chain so the
    // boundary instruction receives the exact existing single-step/HLE policy.
    // Any other exception cancels the span and follows the same fail-closed path.
    if (NativeLinearSpanEnabled(context->execution_backend) &&
        context->native_fast_path.linear_span_active)
    {
        const DWORD span_code = exception_info->ExceptionRecord != nullptr
            ? exception_info->ExceptionRecord->ExceptionCode
            : 0U;
        const bool reached_boundary =
            span_code == EXCEPTION_SINGLE_STEP &&
            (static_cast<std::uint32_t>(win32_context->Dr6) & 0x1U) != 0U &&
            static_cast<std::uint32_t>(win32_context->Eip) ==
                context->native_fast_path.linear_span_boundary;
        const bool write_fault_cancel =
            span_code == EXCEPTION_ACCESS_VIOLATION &&
            exception_info->ExceptionRecord->NumberParameters >= 2U &&
            exception_info->ExceptionRecord->ExceptionInformation[0] == 1U &&
            exception_info->ExceptionRecord->ExceptionInformation[1] <=
                std::numeric_limits<std::uint32_t>::max() &&
            IsWin32AotGuestPageWriteWatched(
                context->aot_page_write_watch,
                static_cast<std::uint32_t>(
                    exception_info->ExceptionRecord
                        ->ExceptionInformation[1]));
        LeaveNativeLinearSpan(
            win32_context, context, reached_boundary, write_fault_cancel,
            static_cast<std::uint32_t>(span_code));
    }
    // Route A native region (Task 266): while a region runs natively, only its
    // hardware breakpoints trap -- Dr0 at the caller return address and Dr1-Dr3
    // at the region's sensitive instructions -- all reported as #DB. Handle those
    // here before any other consumer.
    if (RouteANativeRegionEnabled() && context->native_fast_path.region_active)
    {
        const DWORD region_code = exception_info->ExceptionRecord != nullptr
            ? exception_info->ExceptionRecord->ExceptionCode
            : 0U;
        const std::uint32_t region_eip =
            static_cast<std::uint32_t>(win32_context->Eip);
        const std::uint32_t region_dr6 =
            static_cast<std::uint32_t>(win32_context->Dr6);
        if (region_code == EXCEPTION_SINGLE_STEP)
        {
            if ((region_dr6 & 0x1U) != 0U &&
                region_eip ==
                    context->native_fast_path.region_return_address)
            {
                LeaveNativeRegion(win32_context, context, true);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if ((region_dr6 & 0x0EU) != 0U &&
                HandleNativeRegionSensitiveDr(win32_context, context))
            {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        // Unexpected exception (unhandled sensitive instruction, guest fault, or
        // stray debug event): restore the debug registers and fall through to
        // normal single-step handling with EIP unchanged.
        LeaveNativeRegion(win32_context, context, false);
    }
    const auto stop_for_aot_terminal_failure = [context, win32_context]() {
        if (!context->aot_terminal_failure.load(std::memory_order_acquire))
        {
            return false;
        }
        win32_context->EFlags &= ~0x00000100U;
        return true;
    };
    prologue_time_scope.reset();
    // Task 325: AOT transfer resolution. The scope closes on every path,
    // including the early returns, because it is an ordinary block-scoped
    // object. Handler order and the interleaved terminal-failure checks are
    // unchanged.
    {
        const ExecutionTimeScope aot_transfer_time_scope(
            context->execution_time_profile.get(),
            ExecutionTimeBucket::kVehAotTransfer);
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotGuestCodeWriteCompletion(
                exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotGuestCodeWriteFault(
                exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotTimerSafePoint(
                exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotReentry(exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotIndirectTransfer(exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotConditionalTransfer(
                exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (HandleAotReturnTransfer(exception_info, win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (stop_for_aot_terminal_failure())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
    if (context->native_fast_path.active)
    {
        const bool returned =
            exception_info->ExceptionRecord != nullptr &&
            exception_info->ExceptionRecord->ExceptionCode ==
                EXCEPTION_SINGLE_STEP &&
            win32_context->Eip ==
                context->native_fast_path.return_address &&
            (win32_context->Dr6 & 0x1U) != 0;
        detail::LeaveNativeFastPath(win32_context,
                                    &context->native_fast_path,
                                    returned);
    }
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_fs,
            static_cast<long>(g_recovery_host_fs));
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_ds,
            static_cast<long>(g_recovery_host_ds));
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_es,
            static_cast<long>(g_recovery_host_es));
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_gs,
            static_cast<long>(g_recovery_host_gs));
    }
    constexpr DWORD kVisualCppThreadNameException = 0x406D1388U;
    constexpr DWORD kDebugPrintExceptionAnsi = 0x40010006U;
    constexpr DWORD kDebugPrintExceptionWide = 0x4001000AU;
    if (exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_SINGLE_STEP &&
        !IsGuestInstructionPointer(
            context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        // Task 376: this discard is 70.1% of all single-step exceptions and had
        // no counter, because both existing instruments gate on
        // IsGuestInstructionPointer and so never saw it. Behaviour is unchanged;
        // the population is merely named. Counter increments only -- this is the
        // exception path.
        {
            const std::uint32_t discarded_eip =
                static_cast<std::uint32_t>(win32_context->Eip);
            OutOfArenaStepLocation location = OutOfArenaStepLocation::kOther;
            if (context->aot_placement != nullptr &&
                context->aot_placement->placed &&
                discarded_eip >= context->aot_placement->base_address &&
                discarded_eip < context->aot_placement->base_address +
                    context->aot_placement->capacity)
            {
                location = OutOfArenaStepLocation::kAotCodeCache;
            }
            RecordOutOfArenaStep(&context->out_of_arena_step_census,
                                 discarded_eip, location,
                                 context->enable_single_step_trace,
                                 context->aot_reentry_pending);
        }
        win32_context->EFlags &= ~0x00000100U;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception_info->ExceptionRecord != nullptr &&
        (exception_info->ExceptionRecord->ExceptionCode ==
             kDebugPrintExceptionAnsi ||
         exception_info->ExceptionRecord->ExceptionCode ==
             kDebugPrintExceptionWide) &&
        !IsGuestInstructionPointer(
            context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            kVisualCppThreadNameException &&
        !IsGuestInstructionPointer(
            context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    // Task 325: nine InterlockedExchange writes plus allocator recording run on
    // every exception regardless of kind, so they are measured as fixed cost.
    std::optional<ExecutionTimeScope> telemetry_time_scope;
    telemetry_time_scope.emplace(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kVehTelemetry);
    if (context->shared_live_telemetry != nullptr &&
        exception_info->ExceptionRecord != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->last_exception_code,
            static_cast<long>(
                exception_info->ExceptionRecord->ExceptionCode));
        if (IsGuestInstructionPointer(
                context,
                static_cast<std::uint32_t>(win32_context->Eip)))
        {
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_eip,
                static_cast<long>(win32_context->Eip));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_eax,
                static_cast<long>(win32_context->Eax));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_ebx,
                static_cast<long>(win32_context->Ebx));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_ecx,
                static_cast<long>(win32_context->Ecx));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_edx,
                static_cast<long>(win32_context->Edx));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_esi,
                static_cast<long>(win32_context->Esi));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_edi,
                static_cast<long>(win32_context->Edi));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_esp,
                static_cast<long>(win32_context->Esp));
            InterlockedExchange(
                &context->shared_live_telemetry->guest_handler_phase,
                1);
        }
    }
    if (context->enable_single_step_trace)
    {
        win32_context->EFlags |= 0x00000100U;
    }
    ExceptionDispatchScope dispatch_scope(
        context,
        static_cast<std::uint32_t>(win32_context->Eip));
    RecordAllocatorControlFlowException(exception_info, context);
    telemetry_time_scope.reset();
    if (HandleGlideGateBoundary(win32_context, context))
    {
        InjectPendingInterrupts(win32_context, context);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    // Task 325: the non-Glide boundary gates. Glide keeps its own bucket from
    // Task 323 so the render path stays separable.
    {
        const ExecutionTimeScope boundary_gate_time_scope(
            context->execution_time_profile.get(),
            ExecutionTimeBucket::kVehBoundaryGates);
        if (HandleTimerInterruptChainBoundary(win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (HandleLinexeFarTransferBoundary(win32_context, context))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    // Task 376: `single_step_trace_count` only advances inside
    // HandleSingleStepTrace, which returns immediately when trace mode is off.
    // Comparing it against the exception census therefore measured two different
    // things, not a discarded population. This records the split.
    if (exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_SINGLE_STEP)
    {
        RecordSingleStepTraceDisposition(&context->out_of_arena_step_census,
                                         context->enable_single_step_trace);
    }
    if (exception_info->ExceptionRecord != nullptr &&
        (exception_info->ExceptionRecord->ExceptionCode ==
             EXCEPTION_SINGLE_STEP ||
         (context->aot_reentry_pending &&
          exception_info->ExceptionRecord->ExceptionCode ==
             EXCEPTION_BREAKPOINT)) &&
        HandleSingleStepTrace(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    // Task 325: everything past this point is the sequential HLE handler chain
    // reached by exceptions the single-step path did not take -- principally
    // INT3 AOT boundaries. Held open to the end of the function so every exit
    // path is attributed.
    const ExecutionTimeScope hle_chain_time_scope(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kVehHleChain);
    AotHleTranslationScope aot_hle_translation_scope(win32_context, context);
    constexpr std::uint32_t kMaximumX86InstructionBytes = 15U;
    const bool guest_decode_window_readable = IsGuestRangeReadable(
        context,
        reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(win32_context->Eip)),
        kMaximumX86InstructionBytes);
    if (context->use_guest_stack && !guest_decode_window_readable)
    {
        // Task 300: preserve the primary guest exception. Calling opcode
        // probes with an invalid EIP creates a host AV that masks the original
        // code/fault VA, as observed at HandleTracedDosInterrupt21.
        win32_context->EFlags &= ~0x00000100U;
        CommitUnhandledBreakpointEvidence(
            breakpoint_evidence, win32_context, context);
        CaptureException(exception_info, context);
        context->guest_return_esp =
            static_cast<std::uint32_t>(win32_context->Esp);
        context->host_esp = context->active_call_state->host_esp;
        RecoverToHost(win32_context, context);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (context->enable_privileged_trap_hle &&
        HandlePrivilegedTrapInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_privileged_trap_hle &&
        HandlePortIoInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_traced_dos_hle &&
        (HandleTracedDosInterrupt21(win32_context, context) ||
         HandleTracedDosInterrupt2F(win32_context, context) ||
         HandleTracedDpmiInterrupt31(win32_context, context) ||
         HandleTracedMouseInterrupt33(win32_context, context)))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentPopInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleRepStosdInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentStoreInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentOverrideMemoryLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentOverrideByteLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleFsSegmentWordLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentMemoryCompareInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentMemoryLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryAddInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryOrInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryCompareByteInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryStoreInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryTestInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedFpuMemoryInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_dos_hle &&
        HandleDosHleInstruction(win32_context, context))
    {
        InjectPendingInterrupts(win32_context, context);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    const bool hle_active = context->enable_dos_hle || context->enable_traced_dos_hle;
    if (hle_active &&
        exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_ACCESS_VIOLATION)
    {
        bool handled = false;
        if (exception_info->ExceptionRecord->NumberParameters >= 2)
        {
            const std::uint32_t access_kind = static_cast<std::uint32_t>(
                exception_info->ExceptionRecord->ExceptionInformation[0]);
            const std::uint32_t fault_va = static_cast<std::uint32_t>(
                exception_info->ExceptionRecord->ExceptionInformation[1]);
            
            bool is_aot_address = ((fault_va >= 0x0A000000U) && (fault_va < 0x0E000000U));

            if (access_kind == 8 && is_aot_address)
            {
                std::uint32_t decode_eip = 0;
                if (context->aot_placement != nullptr)
                {
                    const std::uint32_t offset = fault_va - context->aot_placement->base_address;
                    for (const runtime::AotAddressMapEntry& entry : context->aot_placement->address_map)
                    {
                        if (offset >= entry.cache_offset &&
                            offset < entry.cache_offset + entry.emitted_length)
                        {
                            decode_eip = entry.guest_address;
                            break;
                        }
                    }
                }
                if (decode_eip == 0)
                {
                    const std::uint32_t* esp_ptr = reinterpret_cast<const std::uint32_t*>(
                        static_cast<std::uintptr_t>(win32_context->Esp));
                    MEMORY_BASIC_INFORMATION mbi = {};
                    if (VirtualQuery(esp_ptr, &mbi, sizeof(mbi)) == sizeof(mbi) &&
                        (mbi.State == MEM_COMMIT) &&
                        ((mbi.Protect & PAGE_NOACCESS) == 0))
                    {
                        for (int i = 0; i < 32; ++i)
                        {
                            const std::uint32_t* cur_ptr = &esp_ptr[i];
                            if (reinterpret_cast<std::uintptr_t>(cur_ptr) < reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize)
                            {
                                std::uint32_t stack_val = *cur_ptr;
                                if (IsGuestInstructionPointer(context, stack_val))
                                {
                                    decode_eip = stack_val;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (decode_eip == 0)
                {
                    const std::uint32_t* esp_ptr = reinterpret_cast<const std::uint32_t*>(
                        static_cast<std::uintptr_t>(win32_context->Esp));
                    if (IsGuestInstructionPointer(context, esp_ptr[0]))
                    {
                        decode_eip = esp_ptr[0];
                    }
                }
                if (decode_eip != 0)
                {
                    win32_context->Eip = decode_eip;
                    context->aot_legacy_fallback = true;
                    context->enable_single_step_trace = true;
                    context->aot_reentry_pending = false;
                    handled = true;
                }
            }
            else if (access_kind == 0)
            {
                std::uint32_t decode_eip = win32_context->Eip;
                if (IsAotCacheAddress(context, win32_context->Eip))
                {
                    if (context->aot_placement != nullptr)
                    {
                        FindAotGuestAddress(*context->aot_placement, win32_context->Eip, &decode_eip);
                    }
                }

                if (decode_eip == 0x0304DD7DU)
                {
                    win32_context->EFlags |= 0x00000040U;
                    win32_context->Eip = decode_eip + 7;
                    context->aot_legacy_fallback = true;
                    context->enable_single_step_trace = true;
                    context->aot_reentry_pending = false;
                    handled = true;
                }
                else if (fault_va < 0x10000)
                {
                    std::uint32_t decode_eip = win32_context->Eip;
                    bool is_aot = false;
                    if (IsAotCacheAddress(context, win32_context->Eip))
                    {
                        is_aot = true;
                        if (context->aot_placement != nullptr)
                        {
                            FindAotGuestAddress(*context->aot_placement, win32_context->Eip, &decode_eip);
                        }
                    }

                    if (is_aot || IsGuestInstructionPointer(context, win32_context->Eip))
                    {
                        handled = HandleGuestLowMemoryReadFault(win32_context, context, fault_va, decode_eip);
                    }
                }
            }
        }
        
        if (!handled)
        {
            handled = HandleDosMemoryAccess(win32_context, context);
        }

        if (handled)
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    if (context->enable_segment_load_hle &&
        HandleRepMovsInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleRepCmpsbInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleLodsbInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->aot_terminal_failure.load(std::memory_order_acquire))
    {
        win32_context->EFlags &= ~0x00000100U;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleOriginalFatalBreakpoint(exception_info,
                                      win32_context,
                                      context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (context->aot_reentry_pending &&
        exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_BREAKPOINT)
    {
        // Indirect transfers, returns, and LOOP-family instructions execute
        // once from the original image under TF, then re-enter the cache.
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (!context->use_guest_stack &&
        (*instruction == 0xCC || instruction[-1] == 0xCC))
    {
        const std::uint32_t byte_count = exception_info->ContextRecord->Ecx;
        const void* source = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(exception_info->ContextRecord->Edx));
        AppendConsoleOutput(context, source, byte_count);
        RecoverFromHleExit(exception_info->ContextRecord, context);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    const std::uint32_t exception_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    if (context->aot_placement != nullptr &&
        IsAotCacheAddress(context, exception_address) &&
        FindAotGuestAddress(*context->aot_placement,
                            exception_address,
                            &context->aot_exception_guest_address))
    {
        context->aot_exception_mapping_valid = true;
        context->aot_exception_cache_address = exception_address;
        const std::uint32_t cache_offset = exception_address -
            context->aot_placement->base_address;
        const std::uint32_t cache_bytes = std::min<std::uint32_t>(
            sizeof(context->aot_exception_cache_bytes),
            context->aot_placement->size - cache_offset);
        std::memcpy(
            context->aot_exception_cache_bytes,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(exception_address)),
            cache_bytes);
        const std::uint64_t guest_end =
            static_cast<std::uint64_t>(
                context->aot_exception_guest_address) +
            sizeof(context->aot_exception_guest_bytes);
        const std::uint64_t runtime_end =
            static_cast<std::uint64_t>(context->runtime_base) +
            context->runtime_size;
        if (context->aot_exception_guest_address >=
                context->runtime_base &&
            guest_end <= runtime_end)
        {
            std::memcpy(
                context->aot_exception_guest_bytes,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                    context->aot_exception_guest_address)),
                sizeof(context->aot_exception_guest_bytes));
        }
    }

    win32_context->EFlags &= ~0x00000100U;
    CommitUnhandledBreakpointEvidence(
        breakpoint_evidence, win32_context, context);
    CaptureException(exception_info, context);
    context->guest_return_esp =
        static_cast<std::uint32_t>(exception_info->ContextRecord->Esp);

    if (context->use_guest_stack)
    {
        context->host_esp = context->active_call_state->host_esp;
        RecoverToHost(exception_info->ContextRecord, context);
    }
    else
    {
        exception_info->ContextRecord->Eip =
            reinterpret_cast<DWORD_PTR>(&RecoverHostStackException);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Boundary definitions promoted to external linkage (out of the anonymous
// namespace) so extracted win32 execution modules can call them across
// translation units. Declared in execution_internal.h.
bool WriteGuestBytes(ThreadContext* context,
                     void* destination,
                     const void* source,
                     std::size_t byte_count)
{
    if (source == nullptr ||
        !IsGuestRangeWritable(context, destination, byte_count))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        byte_count,
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest byte store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, source, byte_count);

    DWORD ignored_protect = 0;
    if (!VirtualProtect(destination,
                        byte_count,
                        previous_protect,
                        &ignored_protect))
    {
        return false;
    }
    if (byte_count > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    return NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        static_cast<std::uint32_t>(byte_count));
}

bool IsAotCacheAddress(const ThreadContext* context, std::uint32_t address)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        !context->aot_placement->placed)
    {
        return false;
    }
    const std::uint64_t end =
        static_cast<std::uint64_t>(context->aot_placement->base_address) +
        context->aot_placement->size;
    return address >= context->aot_placement->base_address && address < end;
}

bool RunWin32ExecutionThread(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t guest_initial_esp,
    bool use_guest_stack,
    bool enable_privileged_trap_hle,
    bool enable_traced_dos_hle,
    bool enable_segment_load_hle,
    bool enable_dos_hle,
    bool enable_single_step_trace,
    const hle::DosVirtualFileSystemState* dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    const std::filesystem::path* sound_rom_zip_path,
    Win32AotCodeCachePlacement* aot_placement,
    runtime::ExecutionBackend execution_backend,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    *attempt = Win32MinimalExecutionAttempt{};
    attempt->entry_address = entry_address;
    attempt->supported = IsDirectX86ExecutionSupported();
    attempt->guest_stack_switch_supported = IsGuestStackSwitchSupported();
    attempt->guest_stack_initial_esp = guest_initial_esp;

    if (!attempt->supported)
    {
        attempt->valid = true;
#if defined(_WIN32)
        attempt->message =
            "minimal original entry execution requires a 32-bit host";
#else
        attempt->message =
            "minimal original entry execution requires Win32 host APIs";
#endif
        return true;
    }

    if (use_guest_stack && !attempt->guest_stack_switch_supported)
    {
        attempt->valid = true;
        attempt->message =
            "guest stack execution requires 32-bit MSVC Win32 support";
        return true;
    }

#if !defined(_WIN32)
    attempt->valid = true;
    attempt->message =
        "minimal original entry execution requires Win32 host APIs";
    return true;
#else
    if (!placement.valid || !placement.placed)
    {
        attempt->message = "relocated image is not placed";
        return false;
    }

    ThreadContext context;
    if (SingleStepHotspotProfileEnabled())
    {
        context.single_step_hotspot_profile =
            std::make_unique<Win32SingleStepHotspotProfile>();
    }
    if (ExecutionTimeProfileEnabled())
    {
        context.execution_time_profile =
            std::make_unique<Win32ExecutionTimeProfile>();
        context.aot_worker_timing =
            std::make_unique<Win32AotWorkerTimingProfile>();
    }
    SharedTelemetryMapping shared_telemetry =
        OpenSharedTelemetryMapping();
    context.shared_live_telemetry = shared_telemetry.telemetry;
    if (context.shared_live_telemetry != nullptr)
    {
        InterlockedExchange(&context.shared_live_telemetry->host_phase, 1);
    }
    context.entry_address = aot_placement != nullptr
        ? aot_placement->entry_address : entry_address;
    context.runtime_base = placement.placed_base;
    context.runtime_size = placement.placed_size;
    context.guest_initial_esp = guest_initial_esp;
    context.use_guest_stack = use_guest_stack;
    context.enable_privileged_trap_hle = enable_privileged_trap_hle;
    context.enable_traced_dos_hle = enable_traced_dos_hle;
    context.enable_segment_load_hle = enable_segment_load_hle;
    context.enable_dos_hle = enable_dos_hle;
    context.enable_single_step_trace = enable_single_step_trace;
    context.aot_placement = aot_placement;
    context.execution_backend = execution_backend;
    char call_return_trace_text[8] = {};
    const DWORD call_return_trace_length = GetEnvironmentVariableA(
        "REPIU_AOT_DBT_CALL_TRACE", call_return_trace_text,
        static_cast<DWORD>(sizeof(call_return_trace_text)));
    context.aot_dbt_call_return_trace_configured =
        call_return_trace_length == 1U &&
        call_return_trace_text[0] == '1';
    char call_step_probe_text[128] = {};
    GetEnvironmentVariableA(
        "REPIU_AOT_DBT_CALL_STEP", call_step_probe_text,
        static_cast<DWORD>(sizeof(call_step_probe_text)));
    ConfigureAotDbtCallStepProbe(&context, call_step_probe_text);
    context.glide_backend.SetExecutionBackend(execution_backend);
    char probe_offset_text[32] = {};
    const DWORD probe_offset_length = GetEnvironmentVariableA(
        "REPIU_EXECUTION_PROBE_OFFSET", probe_offset_text,
        static_cast<DWORD>(sizeof(probe_offset_text)));
    if (probe_offset_length > 0U &&
        probe_offset_length < sizeof(probe_offset_text))
    {
        char* end = nullptr;
        const unsigned long value = std::strtoul(
            probe_offset_text, &end, 0);
        if (end != probe_offset_text && *end == '\0' && value <= UINT32_MAX)
        {
            context.execution_probe_configured = true;
            context.execution_probe_offset =
                static_cast<std::uint32_t>(value);
        }
    }
    if (context.execution_probe_configured && aot_placement != nullptr &&
        !InstallWin32AotProbeSentinel(
            aot_placement,
            context.runtime_base + context.execution_probe_offset))
    {
        context.execution_probe_configured = false;
    }
    const auto read_hex_env = [](const char* name, std::uint32_t* out) {
        char text[32] = {};
        const DWORD length = GetEnvironmentVariableA(
            name, text, static_cast<DWORD>(sizeof(text)));
        if (length == 0U || length >= sizeof(text))
        {
            return false;
        }
        char* end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 0);
        if (end == text || *end != '\0' || value > UINT32_MAX)
        {
            return false;
        }
        *out = static_cast<std::uint32_t>(value);
        return true;
    };
    if (read_hex_env("REPIU_EXECUTION_TRACE_START",
                     &context.execution_trace_start_offset) &&
        read_hex_env("REPIU_EXECUTION_TRACE_END",
                     &context.execution_trace_end_offset) &&
        read_hex_env("REPIU_EXECUTION_TRACE_ESP_OFFSET",
                     &context.execution_trace_esp_offset) &&
        context.execution_trace_start_offset <=
            context.execution_trace_end_offset)
    {
        context.execution_trace_configured = true;
    }
    if (context.execution_trace_configured && aot_placement != nullptr &&
        !InstallWin32AotProbeSentinel(
            aot_placement,
            context.runtime_base + context.execution_trace_start_offset))
    {
        context.execution_trace_configured = false;
    }
    // A single sentinel only forces single-stepping until the AOT dispatcher
    // (HandleAotReentry) finds a resolvable cached target for the NEXT guest
    // address and jumps straight back into fast cached execution — typically
    // after just one instruction, since the rest of the traced range usually
    // already has a normal cache entry. A second, independent sentinel forces
    // a fresh breakpoint (and a fresh capture) at that later point too.
    if (context.execution_trace_configured && aot_placement != nullptr &&
        read_hex_env("REPIU_EXECUTION_TRACE_SENTINEL2",
                     &context.execution_trace_sentinel2_offset))
    {
        context.execution_trace_sentinel2_configured =
            InstallWin32AotProbeSentinel(
                aot_placement,
                context.runtime_base + context.execution_trace_sentinel2_offset);
    }
    if (aot_placement != nullptr)
    {
        context.aot_cache_entry_count.store(1, std::memory_order_relaxed);
    }
    context.glide_state.texture_memory_bytes =
        repiu::hle::kPiuBansheeVirtualTextureMemoryBytes;
    if (glide_exports != nullptr)
    {
        context.glide_exports = *glide_exports;
    }
    if (cd_chd_path != nullptr && context.cd_image.Open(*cd_chd_path))
    {
        context.mscdex_available = true;
        context.cd_audio_available = context.cd_audio.Open(*cd_chd_path);
    }
    // Sound is optional: a missing or unreadable ROM set leaves the PIU10 sound
    // window inert instead of failing the run, exactly like a cabinet with a dead
    // sound board still booting.
    if (sound_rom_zip_path != nullptr)
    {
        context.ymz_audio_available =
            context.ymz_audio.Open(*sound_rom_zip_path);
    }
    context.dos_environment_block = BuildDosEnvironmentBlock();
    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    repiu::runtime::InitializeSelectorAllocator(
        &context.dpmi_selector_allocator, 0x00A4U);
    repiu::runtime::InitializeDosLowMemory(&context.dos_low_memory);
    for (const repiu::runtime::RelocatedSelectorBinding& binding :
         placement.selector_bindings)
    {
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            repiu::runtime::GuestDescriptor{
                binding.selector,
                binding.relocated_base_address,
                binding.limit,
                0,
                true,
        });
    }
    const auto find_linexe_segment =
        [linexe_module](std::uint16_t selector)
            -> const exe::Dos16mBoundSegment* {
        if (linexe_module == nullptr)
        {
            return nullptr;
        }
        for (const exe::Dos16mBoundSegment& segment :
             linexe_module->segments)
        {
            if (segment.selector == selector)
            {
                return &segment;
            }
        }
        return nullptr;
    };
    const exe::Dos16mBoundSegment* extracted_code =
        find_linexe_segment(kDos4gwLinexeCodeSelector);
    const exe::Dos16mBoundSegment* extracted_bss =
        find_linexe_segment(0x0088U);
    const exe::Dos16mBoundSegment* extracted_data =
        find_linexe_segment(kDos4gwLinexeDataSelector);
    const bool extracted_linexe_valid = linexe_module != nullptr &&
        extracted_code != nullptr && extracted_bss != nullptr &&
        extracted_data != nullptr;
    if (repiu::hle::BuildLinexeCallGatePlan(&context.linexe_gate_plan) &&
        repiu::hle::BuildLinexeArenaLayout(
            placement.hle_reserve_base,
            placement.arena_end_address,
            extracted_linexe_valid
                ? static_cast<std::uint32_t>(extracted_code->image.size())
                : static_cast<std::uint32_t>(
                      context.linexe_gate_plan.gate_image.size()),
            extracted_linexe_valid
                ? static_cast<std::uint32_t>(extracted_bss->image.size())
                : 0U,
            extracted_linexe_valid
                ? static_cast<std::uint32_t>(extracted_data->image.size())
                : static_cast<std::uint32_t>(
                      context.linexe_gate_plan.private_data_image.size()),
            &context.linexe_arena_layout))
    {
        const auto address = [](std::uint32_t value) {
            return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
        };
        const bool glide_gate_fits =
            context.linexe_arena_layout.gate_code_size >
                kGlideFirstGateOffset &&
            repiu::hle::BuildGlideGatePlan(
                context.glide_exports,
                kGlideFirstGateOffset,
                kGlideGateStride,
                context.linexe_arena_layout.gate_code_size -
                    kGlideFirstGateOffset,
                &context.glide_gate_plan);
        const bool images_written =
            WriteGuestBytes(&context,
                            address(context.linexe_arena_layout.client_data_base),
                            context.linexe_gate_plan.client_data_image.data(),
                            context.linexe_gate_plan.client_data_image.size()) &&
            WriteGuestBytes(&context,
                            address(context.linexe_arena_layout.gate_code_base),
                            extracted_linexe_valid
                                ? extracted_code->image.data()
                                : context.linexe_gate_plan.gate_image.data(),
                            extracted_linexe_valid
                                ? extracted_code->image.size()
                                : context.linexe_gate_plan.gate_image.size()) &&
            glide_gate_fits &&
            WriteGuestBytes(
                &context,
                address(context.linexe_arena_layout.gate_code_base +
                        kGlideFirstGateOffset),
                context.glide_gate_plan.image.data(),
                context.glide_gate_plan.image.size()) &&
            (!extracted_linexe_valid || WriteGuestBytes(
                &context,
                address(context.linexe_arena_layout.bss_base),
                extracted_bss->image.data(),
                extracted_bss->image.size())) &&
            WriteGuestBytes(&context,
                            address(context.linexe_arena_layout.private_data_base),
                            extracted_linexe_valid
                                ? extracted_data->image.data()
                                : context.linexe_gate_plan.private_data_image.data(),
                            extracted_linexe_valid
                                ? extracted_data->image.size()
                                : context.linexe_gate_plan.private_data_image.size());
        const bool descriptors_registered = images_written &&
            repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {kDos4gwClientDataSelector,
                 context.linexe_arena_layout.client_data_base,
                 0x0FFFU, 0, true}) &&
            repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {kDos4gwLinexeDataSelector,
                 context.linexe_arena_layout.private_data_base,
                 extracted_linexe_valid ? extracted_data->limit : 0x0FFFU,
                 0, true}) &&
            repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {kDos4gwLinexeCodeSelector,
                 context.linexe_arena_layout.gate_code_base,
                 context.linexe_arena_layout.gate_code_size - 1U,
                 0, true}) &&
            (!extracted_linexe_valid || repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {0x0088U,
                 context.linexe_arena_layout.bss_base,
                 extracted_bss->limit,
                 0, true}));
        if (descriptors_registered)
        {
            DWORD ignored = 0;
            const bool client_protected = VirtualProtect(
                address(context.linexe_arena_layout.client_data_base),
                0x1000U, PAGE_READONLY, &ignored) != 0;
            const bool private_protected = VirtualProtect(
                address(context.linexe_arena_layout.private_data_base),
                context.linexe_arena_layout.private_data_size,
                extracted_linexe_valid ? PAGE_READWRITE : PAGE_READONLY,
                &ignored) != 0;
            const bool gates_protected = VirtualProtect(
                address(context.linexe_arena_layout.gate_code_base),
                context.linexe_arena_layout.gate_code_size,
                extracted_linexe_valid ? PAGE_READWRITE : PAGE_EXECUTE_READ,
                &ignored) != 0;
            const bool bss_protected = !extracted_linexe_valid ||
                VirtualProtect(address(context.linexe_arena_layout.bss_base),
                               context.linexe_arena_layout.bss_size,
                               PAGE_READWRITE, &ignored) != 0;
            context.linexe_environment_active =
                client_protected && private_protected && gates_protected &&
                bss_protected;
        }
    }
    if (context.linexe_environment_active)
    {
        context.aot_excluded_guest_ranges.push_back({
            context.linexe_arena_layout.gate_code_base,
            context.linexe_arena_layout.gate_code_size});
    }
    if (dos_file_system != nullptr)
    {
        context.dos_file_system = *dos_file_system;
    }

    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.create_thread == nullptr ||
        api.close_handle == nullptr ||
        api.get_last_error == nullptr)
    {
        attempt->message = "failed to resolve required Win32 thread APIs";
        return false;
    }

    const auto stop_translation_worker = [&context]() {
        if (context.aot_translation_thread != nullptr)
        {
            context.aot_translation_shutdown.store(
                true, std::memory_order_release);
            if (context.aot_translation_request_event == nullptr ||
                SetEvent(context.aot_translation_request_event) == 0 ||
                WaitForSingleObject(context.aot_translation_thread,
                                    INFINITE) != WAIT_OBJECT_0)
            {
                // Context ownership cannot be released while the worker could
                // still reference it. Treat an impossible join failure as a
                // process-local terminal failure rather than creating UAF.
                std::abort();
            }
            CloseHandle(context.aot_translation_thread);
            context.aot_translation_thread = nullptr;
        }
        if (context.aot_translation_request_event != nullptr)
        {
            CloseHandle(context.aot_translation_request_event);
            context.aot_translation_request_event = nullptr;
        }
        if (context.aot_translation_complete_event != nullptr)
        {
            CloseHandle(context.aot_translation_complete_event);
            context.aot_translation_complete_event = nullptr;
        }
    };
    ReResolveAotSegmentOverrides(&context);
    if (context.aot_placement != nullptr)
    {
        context.aot_translation_request_event =
            CreateEventA(nullptr, FALSE, FALSE, nullptr);
        context.aot_translation_complete_event =
            CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (context.aot_translation_request_event == nullptr ||
            context.aot_translation_complete_event == nullptr)
        {
            stop_translation_worker();
            attempt->message = "failed to create AOT translation events";
            return false;
        }
        context.aot_translation_thread = api.create_thread(
            nullptr, 0, AotTranslationWorkerProc, &context, 0, nullptr);
        if (context.aot_translation_thread == nullptr)
        {
            stop_translation_worker();
            attempt->message = "failed to create AOT translation worker";
            return false;
        }
        if (!InstallWin32AotGuestPageWriteWatches(
                *context.aot_placement, nullptr,
                &context.aot_page_write_watch))
        {
            RestoreWin32AotGuestPageWriteWatches(
                &context.aot_page_write_watch);
            stop_translation_worker();
            attempt->message =
                "failed to install AOT guest code write watches";
            return false;
        }
    }

    context.glide_backend.BindHostThread();
    DWORD guest_thread_id = 0;
    HANDLE thread = api.create_thread(nullptr,
                                      0,
                                      GuestEntryThreadProc,
                                      &context,
                                      0,
                                      &guest_thread_id);
    if (thread == nullptr)
    {
        const DWORD error = api.get_last_error();
        std::ostringstream stream;
        stream << "CreateThread failed with error " << error;
        attempt->message = stream.str();
        RestoreWin32AotGuestPageWriteWatches(&context.aot_page_write_watch);
        stop_translation_worker();
        return false;
    }

    attempt->attempted = true;
    if (context.shared_live_telemetry != nullptr)
    {
        InterlockedExchange(&context.shared_live_telemetry->host_phase, 2);
        InterlockedExchange(
            &context.shared_live_telemetry->guest_thread_id,
            static_cast<long>(guest_thread_id));
        InterlockedExchange(
            &context.shared_live_telemetry->host_main_thread_id,
            static_cast<long>(GetCurrentThreadId()));
        if (context.aot_placement != nullptr &&
            context.aot_placement->placed)
        {
            InterlockedExchange(
                &context.shared_live_telemetry->aot_cache_base,
                static_cast<long>(context.aot_placement->base_address));
            InterlockedExchange(
                &context.shared_live_telemetry->aot_cache_size,
                static_cast<long>(context.aot_placement->capacity));
        }
    }
    attempt->guest_stack_switch_attempted = use_guest_stack;
    DWORD exit_code = 0;
    const DWORD wait_result = PollThreadUntilExit(
        thread,
        timeout_milliseconds,
        (enable_single_step_trace || aot_placement != nullptr)
            ? &context : nullptr,
        &context,
        &exit_code);

    const auto remove_vectored_handler = [&context]() {
        if (context.vectored_handler != nullptr)
        {
            RemoveVectoredExceptionHandler(context.vectored_handler);
            context.vectored_handler = nullptr;
        }
    };

    const bool host_exit_requested =
        wait_result == kWin32HostExitRequested;
    if (wait_result == WAIT_TIMEOUT || host_exit_requested)
    {
        attempt->timed_out = !host_exit_requested;
        attempt->quit_requested = host_exit_requested;
        const DWORD interruption_exit_code =
            host_exit_requested ? 0U : 3U;
        attempt->thread_exit_code = interruption_exit_code;

        bool gracefully_interrupted = false;
        if (SuspendThread(thread) != static_cast<DWORD>(-1))
        {
            CONTEXT win32_context = {};
            win32_context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
            if (GetThreadContext(thread, &win32_context))
            {
                if (!host_exit_requested)
                {
                    CopySnapshotFromContextRecord(
                        win32_context, &attempt->timeout_snapshot);
                }

                const std::uint32_t suspended_eip = static_cast<std::uint32_t>(win32_context.Eip);
                if (IsGuestInstructionPointer(&context, suspended_eip) ||
                    IsAotCacheAddress(&context, suspended_eip))
                {
                    context.hle_message = host_exit_requested
                        ? "SDL exit requested; hijacked guest thread for clean teardown"
                        : "timeout reached during guest execution; hijacked thread for clean teardown";
                    RecoverToHost(&win32_context, &context);
                    SetThreadContext(thread, &win32_context);
                    gracefully_interrupted = true;
                }
            }
            ResumeThread(thread);

            if (gracefully_interrupted)
            {
                DWORD thread_exit_code = 0;
                if (WaitForSingleObject(thread, 3000U) == WAIT_OBJECT_0 &&
                    GetExitCodeThread(thread, &thread_exit_code) &&
                    thread_exit_code != STILL_ACTIVE)
                {
                    // Cleanly exited
                }
                else
                {
                    gracefully_interrupted = false;
                }
            }
        }

        if (!gracefully_interrupted && api.terminate_thread != nullptr)
        {
            api.terminate_thread(thread, interruption_exit_code);
            WaitForSingleObject(thread, 5000U);
        }

        context.glide_backend.Close();
        remove_vectored_handler();
        stop_translation_worker();
        RestoreWin32AotGuestPageWriteWatches(&context.aot_page_write_watch);
        CopyThreadObservationToAttempt(context, attempt);
        attempt->valid = true;
        attempt->message = context.hle_message.empty()
            ? (host_exit_requested
                ? "minimal execution stopped by SDL exit request"
                : "minimal execution attempt timed out")
            : context.hle_message;
        api.close_handle(thread);
        return true;
    }

    // Teardown milestones are published through host_phase so an external
    // supervisor can locate the exact step if this path blocks.
    context.glide_backend.Close();
    const auto set_teardown_phase = [&context](long phase) {
        if (context.shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context.shared_live_telemetry->host_phase, phase);
        }
    };
    set_teardown_phase(10);
    remove_vectored_handler();
    set_teardown_phase(11);
    stop_translation_worker();
    set_teardown_phase(12);
    RestoreWin32AotGuestPageWriteWatches(&context.aot_page_write_watch);
    set_teardown_phase(13);
    api.close_handle(thread);
    set_teardown_phase(14);

    attempt->returned = context.returned;
    attempt->exception_caught = context.exception_caught;
    attempt->guest_stack_return_esp = context.guest_return_esp;
    attempt->seh_exception_code = context.exception_code;
    attempt->seh_exception_address = context.exception_address;
    attempt->exception_eax = context.exception_eax;
    attempt->exception_ebx = context.exception_ebx;
    attempt->exception_ecx = context.exception_ecx;
    attempt->exception_edx = context.exception_edx;
    attempt->exception_esi = context.exception_esi;
    attempt->exception_edi = context.exception_edi;
    attempt->exception_snapshot = context.exception_snapshot;
    attempt->exception_access_kind = context.exception_access_kind;
    attempt->exception_fault_va = context.exception_fault_va;
    attempt->exception_fault_region_base =
        context.exception_fault_region_base;
    attempt->exception_fault_alloc_base = context.exception_fault_alloc_base;
    attempt->exception_fault_state = context.exception_fault_state;
    attempt->exception_fault_protect = context.exception_fault_protect;
    attempt->exception_fault_region_size =
        context.exception_fault_region_size;
    std::memcpy(attempt->exception_esi_dwords,
                context.exception_esi_dwords,
                sizeof(attempt->exception_esi_dwords));
    attempt->exception_esi_dword_valid_mask =
        context.exception_esi_dword_valid_mask;
    std::memcpy(attempt->exception_register_strings,
                context.exception_register_strings,
                sizeof(attempt->exception_register_strings));
    attempt->exception_register_string_valid_mask =
        context.exception_register_string_valid_mask;
    attempt->exception_stack_base = context.exception_stack_base;
    std::memcpy(attempt->exception_stack_dwords,
                context.exception_stack_dwords,
                sizeof(attempt->exception_stack_dwords));
    attempt->exception_stack_dword_count =
        context.exception_stack_dword_count;
    attempt->unhandled_breakpoint_evidence =
        context.unhandled_breakpoint_evidence;
    attempt->aot_probe_guest_address = context.aot_probe_guest_address;
    attempt->aot_probe_cache_address = context.aot_probe_cache_address;
    attempt->aot_probe_cache_valid = context.aot_probe_cache_valid;
    std::memcpy(attempt->aot_probe_cache_bytes,
                context.aot_probe_cache_bytes,
                sizeof(attempt->aot_probe_cache_bytes));
    CopyThreadObservationToAttempt(context, attempt);
    attempt->thread_exit_code = exit_code;
    attempt->hle_stdout_output.assign(
        context.hle_stdout_output,
        context.hle_stdout_output + context.hle_stdout_output_size);
    attempt->hle_stderr_output.assign(
        context.hle_stderr_output,
        context.hle_stderr_output + context.hle_stderr_output_size);
    attempt->valid = true;

    if (attempt->returned)
    {
        attempt->message = context.hle_message.empty()
                               ? "original entry returned to host trampoline"
                               : context.hle_message;
    }
    else if (attempt->exception_caught)
    {
        attempt->message = context.hle_message.empty()
                               ? "original entry raised a caught exception"
                               : context.hle_message;
    }
    else
    {
        attempt->message =
            "minimal execution attempt ended without return or exception";
    }

    return true;
#endif
}

bool AttemptWin32MinimalExecution(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    return RunWin32ExecutionThread(
        placement,
        entry_address,
        0,
        false,
        false,
        false,
        false,
        false,
        false,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        runtime::ExecutionBackend::kLegacy,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    if (!stack_plan.valid)
    {
        *attempt = Win32MinimalExecutionAttempt{};
        attempt->entry_address = stack_plan.entry_eip;
        attempt->guest_stack_initial_esp = stack_plan.initial_esp;
        attempt->message = "guest stack switch plan is not valid";
        return false;
    }

    return RunWin32ExecutionThread(
        placement,
        stack_plan.entry_eip,
        stack_plan.initial_esp,
        true,
        false,
        false,
        false,
        false,
        false,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        runtime::ExecutionBackend::kLegacy,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackTrapExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    const std::filesystem::path* sound_rom_zip_path,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    if (!stack_plan.valid)
    {
        *attempt = Win32MinimalExecutionAttempt{};
        attempt->entry_address = stack_plan.entry_eip;
        attempt->guest_stack_initial_esp = stack_plan.initial_esp;
        attempt->message = "guest stack switch plan is not valid";
        return false;
    }

    return RunWin32ExecutionThread(
        placement,
        stack_plan.entry_eip,
        stack_plan.initial_esp,
        true,
        true,
        true,
        true,
        false,
        true,
        &dos_file_system,
        linexe_module,
        glide_exports,
        cd_chd_path,
        sound_rom_zip_path,
        nullptr,
        runtime::ExecutionBackend::kLegacy,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackHleExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    if (!stack_plan.valid)
    {
        *attempt = Win32MinimalExecutionAttempt{};
        attempt->entry_address = stack_plan.entry_eip;
        attempt->guest_stack_initial_esp = stack_plan.initial_esp;
        attempt->message = "guest stack switch plan is not valid";
        return false;
    }

    return RunWin32ExecutionThread(
        placement,
        stack_plan.entry_eip,
        stack_plan.initial_esp,
        true,
        true,
        true,
        true,
        true,
        false,
        &dos_file_system,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        runtime::ExecutionBackend::kLegacy,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackAotExecution(
    const Win32RelocatedImagePlacement& placement,
    Win32AotCodeCachePlacement& aot_placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    const std::filesystem::path* sound_rom_zip_path,
    runtime::ExecutionBackend execution_backend,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr || !stack_plan.valid || !aot_placement.placed)
    {
        if (attempt != nullptr)
        {
            *attempt = Win32MinimalExecutionAttempt{};
            attempt->message = !stack_plan.valid
                ? "guest stack switch plan is not valid"
                : "AOT code cache placement is not valid";
        }
        return false;
    }
    return RunWin32ExecutionThread(
        placement, stack_plan.entry_eip, stack_plan.initial_esp,
        true, true, true, true, false, false, &dos_file_system,
        linexe_module, glide_exports, cd_chd_path, sound_rom_zip_path,
        &aot_placement,
        execution_backend,
        timeout_milliseconds, attempt);
}

}  // namespace repiu::platform::win32
