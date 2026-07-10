#include "repiu/platform/win32/execution_trampoline.h"

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::platform::win32
{
namespace
{

struct StackSwitchCallState
{
    std::uint32_t entry_address = 0;
    std::uint32_t initial_esp = 0;
    std::uint32_t host_esp = 0;
    std::uint32_t guest_return_esp = 0;
    std::uint32_t result_code = 0;
    std::uint32_t enable_single_step_trace = 0;
};

struct DosInterruptVectorShadow
{
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    bool valid = false;
};

struct ThreadContext
{
    std::uint32_t entry_address = 0;
    std::uint32_t runtime_base = 0;
    std::uint32_t runtime_size = 0;
    std::uint32_t guest_initial_esp = 0;
    std::uint32_t host_esp = 0;
    std::uint32_t guest_return_esp = 0;
    StackSwitchCallState* active_call_state = nullptr;
    bool use_guest_stack = false;
    bool enable_privileged_trap_hle = false;
    bool enable_traced_dos_hle = false;
    bool enable_segment_load_hle = false;
    bool enable_dos_hle = false;
    bool enable_single_step_trace = false;
    bool returned = false;
    bool process_exit = false;
    bool exception_caught = false;
    std::uint32_t exception_code = 0;
    std::uint32_t exception_address = 0;
    std::uint32_t exception_eax = 0;
    std::uint32_t exception_ebx = 0;
    std::uint32_t exception_ecx = 0;
    std::uint32_t exception_edx = 0;
    std::uint32_t exception_esi = 0;
    std::uint32_t exception_edi = 0;
    std::uint32_t handled_hle_trap_count = 0;
    std::uint32_t last_hle_trap_address = 0;
    std::uint32_t last_hle_trap_opcode = 0;
    Win32PortIoObservation port_io;
    std::uint32_t handled_dos_interrupt_count = 0;
    std::uint32_t last_dos_interrupt_vector = 0;
    std::uint32_t last_dos_interrupt_ah = 0;
    std::uint32_t last_dos_interrupt_ax = 0;
    repiu::hle::DosVirtualFileSystemState dos_file_system;
    std::uint32_t handled_dos_chdir_count = 0;
    std::string last_dos_chdir_guest_path;
    std::string last_dos_chdir_host_path;
    std::string last_dos_chdir_virtual_path;
    bool last_dos_chdir_success = false;
    std::uint16_t last_dos_chdir_error = 0;
    std::uint32_t handled_dos_getcwd_count = 0;
    std::uint8_t last_dos_getcwd_drive = 0;
    std::string last_dos_getcwd_path;
    bool last_dos_getcwd_success = false;
    std::uint16_t last_dos_getcwd_error = 0;
    std::uint32_t handled_dos_open_count = 0;
    std::string last_dos_open_guest_path;
    std::string last_dos_open_host_path;
    std::string last_dos_open_virtual_path;
    bool last_dos_open_success = false;
    std::uint16_t last_dos_open_error = 0;
    std::uint16_t last_dos_open_handle = 0;
    std::uint8_t last_dos_open_access_mode = 0;
    std::uint32_t handled_dos_ioctl_count = 0;
    std::uint8_t last_dos_ioctl_subfunction = 0;
    std::uint16_t last_dos_ioctl_handle = 0;
    bool last_dos_ioctl_success = false;
    std::uint16_t last_dos_ioctl_error = 0;
    std::uint16_t last_dos_ioctl_device_info = 0;
    std::uint32_t handled_dos_resize_count = 0;
    std::uint16_t last_dos_resize_selector = 0;
    std::uint16_t last_dos_resize_paragraphs = 0;
    bool last_dos_resize_success = false;
    std::uint16_t last_dos_resize_error = 0;
    std::uint32_t handled_segment_load_count = 0;
    std::uint32_t last_segment_load_address = 0;
    std::uint32_t last_segment_load_opcode = 0;
    std::uint32_t last_segment_load_register = 0;
    std::uint32_t last_segment_load_selector = 0;
    std::uint32_t last_segment_load_source = 0;
    std::uint32_t handled_segment_store_count = 0;
    std::uint32_t last_segment_store_address = 0;
    std::uint32_t last_segment_store_opcode = 0;
    std::uint32_t last_segment_store_register = 0;
    std::uint32_t last_segment_store_selector = 0;
    std::uint32_t last_segment_store_destination = 0;
    std::uint32_t handled_segment_memory_load_count = 0;
    std::uint32_t last_segment_memory_load_address = 0;
    std::uint32_t last_segment_memory_load_opcode = 0;
    std::uint32_t last_segment_memory_load_register = 0;
    std::uint32_t last_segment_memory_load_selector = 0;
    std::uint32_t last_segment_memory_load_offset = 0;
    std::uint32_t last_segment_memory_load_width = 0;
    std::uint32_t last_segment_memory_load_value = 0;
    std::uint32_t handled_low_memory_access_count = 0;
    std::uint32_t last_low_memory_access_address = 0;
    std::uint32_t last_low_memory_access_opcode = 0;
    std::uint32_t last_low_memory_access_esi = 0;
    std::uint32_t last_low_memory_access_edi = 0;
    std::uint32_t last_low_memory_access_destination = 0;
    std::uint32_t last_low_memory_access_value = 0;
    std::vector<std::uint8_t> dos_environment_block;
    bool last_dos_environment_access_valid = false;
    std::uint32_t last_dos_environment_access_offset = 0;
    std::uint32_t last_dos_environment_entry_offset = 0;
    std::uint32_t last_dos_environment_value_length = 0;
    std::string last_dos_environment_entry_name;
    std::uint32_t handled_memory_store_count = 0;
    std::uint32_t last_memory_store_address = 0;
    std::uint32_t last_memory_store_opcode = 0;
    std::uint32_t last_memory_store_destination = 0;
    std::uint32_t last_memory_store_value = 0;
    std::uint32_t last_memory_store_width = 0;
    std::string last_memory_store_source_kind;
    bool last_memory_store_applied = false;
    std::uint32_t shadow_memory_write_count = 0;
    std::uint32_t shadow_memory_read_hit_count = 0;
    bool shadow_memory_range_valid = false;
    std::uint32_t shadow_memory_min_address = 0;
    std::uint32_t shadow_memory_max_address = 0;
    std::uint32_t last_traced_fpu_m32_value = 0;
    bool has_last_traced_fpu_m32_value = false;
    std::atomic<std::uint32_t> single_step_trace_count{0};
    std::atomic<std::uint32_t> single_step_eip{0};
    std::atomic<std::uint32_t> single_step_eax{0};
    std::atomic<std::uint32_t> single_step_ebx{0};
    std::atomic<std::uint32_t> single_step_ecx{0};
    std::atomic<std::uint32_t> single_step_edx{0};
    std::atomic<std::uint32_t> single_step_esi{0};
    std::atomic<std::uint32_t> single_step_edi{0};
    std::atomic<std::uint32_t> single_step_esp{0};
    std::atomic<std::uint32_t> single_step_ebp{0};
    std::atomic<std::uint32_t> single_step_eflags{0};
    std::atomic<std::uint32_t> single_step_cs{0};
    std::atomic<std::uint32_t> single_step_ds{0};
    std::atomic<std::uint32_t> single_step_es{0};
    std::atomic<std::uint32_t> single_step_ss{0};
    std::atomic<std::uint32_t> single_step_fs{0};
    std::atomic<std::uint32_t> single_step_gs{0};
    std::atomic<std::uint32_t> diagnostic_progress_count{0};
    std::uint32_t diagnostic_poll_iteration_count = 0;
    std::uint32_t diagnostic_quiet_iteration_count = 0;
    std::unordered_map<std::uint32_t, std::uint8_t> shadow_memory;
    std::uint16_t guest_es = 0;
    std::uint16_t guest_ss = 0;
    std::uint16_t guest_ds = 0;
    std::uint16_t guest_fs = 0;
    std::uint16_t guest_gs = 0;
    std::array<DosInterruptVectorShadow, 256> dos_interrupt_vectors = {};
    char hle_console_output[4096] = {};
    std::uint32_t hle_console_output_size = 0;
    std::string hle_message;
};

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
thread_local ThreadContext* g_active_thread_context = nullptr;

using CreateThreadFn = HANDLE(WINAPI*)(
    LPSECURITY_ATTRIBUTES,
    SIZE_T,
    LPTHREAD_START_ROUTINE,
    LPVOID,
    DWORD,
    LPDWORD);
using CloseHandleFn = BOOL(WINAPI*)(HANDLE);
using GetExitCodeThreadFn = BOOL(WINAPI*)(HANDLE, LPDWORD);
using GetLastErrorFn = DWORD(WINAPI*)();
using GetThreadContextFn = BOOL(WINAPI*)(HANDLE, LPCONTEXT);
using ResumeThreadFn = DWORD(WINAPI*)(HANDLE);
using SuspendThreadFn = DWORD(WINAPI*)(HANDLE);
using TerminateThreadFn = BOOL(WINAPI*)(HANDLE, DWORD);

struct Win32ThreadApi
{
    CreateThreadFn create_thread = nullptr;
    CloseHandleFn close_handle = nullptr;
    GetExitCodeThreadFn get_exit_code_thread = nullptr;
    GetLastErrorFn get_last_error = nullptr;
    GetThreadContextFn get_thread_context = nullptr;
    ResumeThreadFn resume_thread = nullptr;
    SuspendThreadFn suspend_thread = nullptr;
    TerminateThreadFn terminate_thread = nullptr;
};

template <typename FunctionType>
FunctionType ResolveKernel32Function(HMODULE kernel32, const char* name)
{
    return reinterpret_cast<FunctionType>(GetProcAddress(kernel32, name));
}

const Win32ThreadApi& GetWin32ThreadApi()
{
    static const Win32ThreadApi api = []
    {
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        Win32ThreadApi resolved;
        if (kernel32 == nullptr)
        {
            return resolved;
        }

        resolved.create_thread =
            ResolveKernel32Function<CreateThreadFn>(kernel32, "CreateThread");
        resolved.close_handle =
            ResolveKernel32Function<CloseHandleFn>(kernel32, "CloseHandle");
        resolved.get_exit_code_thread =
            ResolveKernel32Function<GetExitCodeThreadFn>(
                kernel32,
                "GetExitCodeThread");
        resolved.get_last_error =
            ResolveKernel32Function<GetLastErrorFn>(kernel32, "GetLastError");
        resolved.get_thread_context =
            ResolveKernel32Function<GetThreadContextFn>(
                kernel32,
                "GetThreadContext");
        resolved.resume_thread =
            ResolveKernel32Function<ResumeThreadFn>(kernel32, "ResumeThread");
        resolved.suspend_thread =
            ResolveKernel32Function<SuspendThreadFn>(
                kernel32,
                "SuspendThread");
        resolved.terminate_thread =
            ResolveKernel32Function<TerminateThreadFn>(
                kernel32,
                "TerminateThread");
        return resolved;
    }();

    return api;
}

DWORD PollThreadUntilExit(HANDLE thread,
                          DWORD timeout_milliseconds,
                          ThreadContext* progress_context,
                          DWORD* exit_code)
{
    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.get_exit_code_thread == nullptr)
    {
        return WAIT_FAILED;
    }

    const DWORD quiet_iteration_limit = 100000U;
    const DWORD start_tick = GetTickCount();
    std::uint32_t last_progress_count = 0;
    std::uint32_t last_single_step_count = 0;
    if (progress_context != nullptr)
    {
        last_progress_count =
            progress_context->diagnostic_progress_count.load(
                std::memory_order_relaxed);
        last_single_step_count =
            progress_context->single_step_trace_count.load(
                std::memory_order_relaxed);
    }

    DWORD quiet_iterations = 0;
    for (DWORD iteration = 0;; ++iteration)
    {
        if (progress_context != nullptr)
        {
            progress_context->diagnostic_poll_iteration_count =
                iteration + 1;
        }
        DWORD current_exit_code = 0;
        if (!api.get_exit_code_thread(thread, &current_exit_code))
        {
            return WAIT_FAILED;
        }

        if (current_exit_code != STILL_ACTIVE)
        {
            if (exit_code != nullptr)
            {
                *exit_code = current_exit_code;
            }
            return WAIT_OBJECT_0;
        }

        bool progressed = false;
        if (progress_context != nullptr)
        {
            const std::uint32_t progress_count =
                progress_context->diagnostic_progress_count.load(
                    std::memory_order_relaxed);
            const std::uint32_t single_step_count =
                progress_context->single_step_trace_count.load(
                    std::memory_order_relaxed);
            progressed =
                progress_count != last_progress_count ||
                single_step_count != last_single_step_count;
            last_progress_count = progress_count;
            last_single_step_count = single_step_count;
        }

        if (progressed)
        {
            quiet_iterations = 0;
        }
        else
        {
            ++quiet_iterations;
        }
        if (progress_context != nullptr)
        {
            progress_context->diagnostic_quiet_iteration_count =
                quiet_iterations;
        }

        if (quiet_iterations >= quiet_iteration_limit)
        {
            return WAIT_TIMEOUT;
        }

        if (timeout_milliseconds != INFINITE &&
            GetTickCount() - start_tick >= timeout_milliseconds)
        {
            return WAIT_TIMEOUT;
        }
    }
}

void CopySnapshotFromContextRecord(const CONTEXT& source,
                                   X86ExecutionSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return;
    }

#if defined(_M_IX86)
    snapshot->captured = true;
    snapshot->eip = source.Eip;
    snapshot->eax = source.Eax;
    snapshot->ebx = source.Ebx;
    snapshot->ecx = source.Ecx;
    snapshot->edx = source.Edx;
    snapshot->esi = source.Esi;
    snapshot->edi = source.Edi;
    snapshot->esp = source.Esp;
    snapshot->ebp = source.Ebp;
    snapshot->eflags = source.EFlags;
    snapshot->cs = static_cast<std::uint16_t>(source.SegCs);
    snapshot->ds = static_cast<std::uint16_t>(source.SegDs);
    snapshot->es = static_cast<std::uint16_t>(source.SegEs);
    snapshot->ss = static_cast<std::uint16_t>(source.SegSs);
    snapshot->fs = static_cast<std::uint16_t>(source.SegFs);
    snapshot->gs = static_cast<std::uint16_t>(source.SegGs);
#else
    (void)source;
    snapshot->captured = false;
#endif
}

void CaptureSuspendedThreadSnapshot(HANDLE thread,
                                    X86ExecutionSnapshot* snapshot)
{
    if (thread == nullptr || snapshot == nullptr)
    {
        return;
    }

#if defined(_M_IX86)
    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.suspend_thread == nullptr ||
        api.get_thread_context == nullptr ||
        api.resume_thread == nullptr)
    {
        return;
    }

    if (api.suspend_thread(thread) == static_cast<DWORD>(-1))
    {
        return;
    }

    CONTEXT thread_context = {};
    thread_context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
    if (api.get_thread_context(thread, &thread_context))
    {
        CopySnapshotFromContextRecord(thread_context, snapshot);
    }
    api.resume_thread(thread);
#else
    (void)thread;
    snapshot->captured = false;
#endif
}

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

bool BuildSingleStepSnapshot(const ThreadContext& context,
                             X86ExecutionSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return false;
    }

    const std::uint32_t count =
        context.single_step_trace_count.load(std::memory_order_relaxed);
    if (count == 0)
    {
        *snapshot = X86ExecutionSnapshot{};
        return false;
    }

    snapshot->captured = true;
    snapshot->eip =
        context.single_step_eip.load(std::memory_order_relaxed);
    snapshot->eax =
        context.single_step_eax.load(std::memory_order_relaxed);
    snapshot->ebx =
        context.single_step_ebx.load(std::memory_order_relaxed);
    snapshot->ecx =
        context.single_step_ecx.load(std::memory_order_relaxed);
    snapshot->edx =
        context.single_step_edx.load(std::memory_order_relaxed);
    snapshot->esi =
        context.single_step_esi.load(std::memory_order_relaxed);
    snapshot->edi =
        context.single_step_edi.load(std::memory_order_relaxed);
    snapshot->esp =
        context.single_step_esp.load(std::memory_order_relaxed);
    snapshot->ebp =
        context.single_step_ebp.load(std::memory_order_relaxed);
    snapshot->eflags =
        context.single_step_eflags.load(std::memory_order_relaxed);
    snapshot->cs = static_cast<std::uint16_t>(
        context.single_step_cs.load(std::memory_order_relaxed));
    snapshot->ds = static_cast<std::uint16_t>(
        context.single_step_ds.load(std::memory_order_relaxed));
    snapshot->es = static_cast<std::uint16_t>(
        context.single_step_es.load(std::memory_order_relaxed));
    snapshot->ss = static_cast<std::uint16_t>(
        context.single_step_ss.load(std::memory_order_relaxed));
    snapshot->fs = static_cast<std::uint16_t>(
        context.single_step_fs.load(std::memory_order_relaxed));
    snapshot->gs = static_cast<std::uint16_t>(
        context.single_step_gs.load(std::memory_order_relaxed));
    return true;
}

void CopyThreadObservationToAttempt(const ThreadContext& context,
                                    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return;
    }

    attempt->single_step_trace_count =
        context.single_step_trace_count.load(std::memory_order_relaxed);
    attempt->diagnostic_poll_iteration_count =
        context.diagnostic_poll_iteration_count;
    attempt->diagnostic_progress_count =
        context.diagnostic_progress_count.load(std::memory_order_relaxed);
    attempt->diagnostic_quiet_iteration_count =
        context.diagnostic_quiet_iteration_count;
    BuildSingleStepSnapshot(context, &attempt->last_single_step_snapshot);
    attempt->dos_environment_block_size =
        static_cast<std::uint32_t>(context.dos_environment_block.size());
    attempt->last_dos_environment_access_valid =
        context.last_dos_environment_access_valid;
    attempt->last_dos_environment_access_offset =
        context.last_dos_environment_access_offset;
    attempt->last_dos_environment_entry_offset =
        context.last_dos_environment_entry_offset;
    attempt->last_dos_environment_value_length =
        context.last_dos_environment_value_length;
    attempt->last_dos_environment_entry_name =
        context.last_dos_environment_entry_name;
    attempt->handled_hle_trap_count = context.handled_hle_trap_count;
    attempt->last_hle_trap_address = context.last_hle_trap_address;
    attempt->last_hle_trap_opcode = context.last_hle_trap_opcode;
    attempt->port_io = context.port_io;
    attempt->handled_dos_interrupt_count =
        context.handled_dos_interrupt_count;
    attempt->last_dos_interrupt_vector = context.last_dos_interrupt_vector;
    attempt->last_dos_interrupt_ah = context.last_dos_interrupt_ah;
    attempt->last_dos_interrupt_ax = context.last_dos_interrupt_ax;
    attempt->handled_dos_chdir_count = context.handled_dos_chdir_count;
    attempt->last_dos_chdir_guest_path =
        context.last_dos_chdir_guest_path;
    attempt->last_dos_chdir_host_path = context.last_dos_chdir_host_path;
    attempt->last_dos_chdir_virtual_path =
        context.last_dos_chdir_virtual_path;
    attempt->last_dos_chdir_success = context.last_dos_chdir_success;
    attempt->last_dos_chdir_error = context.last_dos_chdir_error;
    attempt->handled_dos_getcwd_count = context.handled_dos_getcwd_count;
    attempt->last_dos_getcwd_drive = context.last_dos_getcwd_drive;
    attempt->last_dos_getcwd_path = context.last_dos_getcwd_path;
    attempt->last_dos_getcwd_success = context.last_dos_getcwd_success;
    attempt->last_dos_getcwd_error = context.last_dos_getcwd_error;
    attempt->handled_dos_open_count = context.handled_dos_open_count;
    attempt->last_dos_open_guest_path = context.last_dos_open_guest_path;
    attempt->last_dos_open_host_path = context.last_dos_open_host_path;
    attempt->last_dos_open_virtual_path =
        context.last_dos_open_virtual_path;
    attempt->last_dos_open_success = context.last_dos_open_success;
    attempt->last_dos_open_error = context.last_dos_open_error;
    attempt->last_dos_open_handle = context.last_dos_open_handle;
    attempt->last_dos_open_access_mode = context.last_dos_open_access_mode;
    attempt->handled_dos_ioctl_count = context.handled_dos_ioctl_count;
    attempt->last_dos_ioctl_subfunction =
        context.last_dos_ioctl_subfunction;
    attempt->last_dos_ioctl_handle = context.last_dos_ioctl_handle;
    attempt->last_dos_ioctl_success = context.last_dos_ioctl_success;
    attempt->last_dos_ioctl_error = context.last_dos_ioctl_error;
    attempt->last_dos_ioctl_device_info =
        context.last_dos_ioctl_device_info;
    attempt->handled_dos_resize_count = context.handled_dos_resize_count;
    attempt->last_dos_resize_selector = context.last_dos_resize_selector;
    attempt->last_dos_resize_paragraphs =
        context.last_dos_resize_paragraphs;
    attempt->last_dos_resize_success = context.last_dos_resize_success;
    attempt->last_dos_resize_error = context.last_dos_resize_error;
    attempt->handled_segment_load_count =
        context.handled_segment_load_count;
    attempt->last_segment_load_address = context.last_segment_load_address;
    attempt->last_segment_load_opcode = context.last_segment_load_opcode;
    attempt->last_segment_load_register =
        context.last_segment_load_register;
    attempt->last_segment_load_selector =
        context.last_segment_load_selector;
    attempt->last_segment_load_source = context.last_segment_load_source;
    attempt->handled_segment_store_count =
        context.handled_segment_store_count;
    attempt->last_segment_store_address = context.last_segment_store_address;
    attempt->last_segment_store_opcode = context.last_segment_store_opcode;
    attempt->last_segment_store_register =
        context.last_segment_store_register;
    attempt->last_segment_store_selector =
        context.last_segment_store_selector;
    attempt->last_segment_store_destination =
        context.last_segment_store_destination;
    attempt->handled_segment_memory_load_count =
        context.handled_segment_memory_load_count;
    attempt->last_segment_memory_load_address =
        context.last_segment_memory_load_address;
    attempt->last_segment_memory_load_opcode =
        context.last_segment_memory_load_opcode;
    attempt->last_segment_memory_load_register =
        context.last_segment_memory_load_register;
    attempt->last_segment_memory_load_selector =
        context.last_segment_memory_load_selector;
    attempt->last_segment_memory_load_offset =
        context.last_segment_memory_load_offset;
    attempt->last_segment_memory_load_width =
        context.last_segment_memory_load_width;
    attempt->last_segment_memory_load_value =
        context.last_segment_memory_load_value;
    attempt->handled_low_memory_access_count =
        context.handled_low_memory_access_count;
    attempt->last_low_memory_access_address =
        context.last_low_memory_access_address;
    attempt->last_low_memory_access_opcode =
        context.last_low_memory_access_opcode;
    attempt->last_low_memory_access_esi =
        context.last_low_memory_access_esi;
    attempt->last_low_memory_access_edi =
        context.last_low_memory_access_edi;
    attempt->last_low_memory_access_destination =
        context.last_low_memory_access_destination;
    attempt->last_low_memory_access_value =
        context.last_low_memory_access_value;
    attempt->handled_memory_store_count = context.handled_memory_store_count;
    attempt->last_memory_store_address = context.last_memory_store_address;
    attempt->last_memory_store_opcode = context.last_memory_store_opcode;
    attempt->last_memory_store_destination =
        context.last_memory_store_destination;
    attempt->last_memory_store_value = context.last_memory_store_value;
    attempt->last_memory_store_width = context.last_memory_store_width;
    attempt->last_memory_store_source_kind =
        context.last_memory_store_source_kind;
    attempt->last_memory_store_applied = context.last_memory_store_applied;
    attempt->shadow_memory_write_count = context.shadow_memory_write_count;
    attempt->shadow_memory_read_hit_count =
        context.shadow_memory_read_hit_count;
    attempt->shadow_memory_byte_count =
        static_cast<std::uint32_t>(context.shadow_memory.size());
    attempt->shadow_memory_range_valid = context.shadow_memory_range_valid;
    attempt->shadow_memory_min_address = context.shadow_memory_min_address;
    attempt->shadow_memory_max_address = context.shadow_memory_max_address;
}

int CaptureException(EXCEPTION_POINTERS* exception_info,
                     ThreadContext* context)
{
    if (exception_info != nullptr && context != nullptr)
    {
        context->exception_caught = true;
        context->exception_code =
            exception_info->ExceptionRecord->ExceptionCode;
        context->exception_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(
                exception_info->ExceptionRecord->ExceptionAddress));
#if defined(_M_IX86)
        context->exception_eax = exception_info->ContextRecord->Eax;
        context->exception_ebx = exception_info->ContextRecord->Ebx;
        context->exception_ecx = exception_info->ContextRecord->Ecx;
        context->exception_edx = exception_info->ContextRecord->Edx;
        context->exception_esi = exception_info->ContextRecord->Esi;
        context->exception_edi = exception_info->ContextRecord->Edi;
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
        mov [ecx + 8], esp

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
    std::uint32_t host_esp = thread_context->host_esp;
    if (host_esp == 0 && thread_context->active_call_state != nullptr)
    {
        host_esp = thread_context->active_call_state->host_esp;
    }
    thread_context->host_esp = host_esp;
    context->Esp = host_esp;
}

bool IsGuestRangeReadable(ThreadContext* context,
                          const void* source,
                          std::uint32_t byte_count)
{
    if (context == nullptr || source == nullptr || byte_count == 0)
    {
        return false;
    }

    const std::uintptr_t base =
        static_cast<std::uintptr_t>(context->runtime_base);
    const std::uintptr_t size =
        static_cast<std::uintptr_t>(context->runtime_size);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(source);
    const std::uintptr_t end = address + byte_count;
    return address >= base && end >= address && end <= base + size;
}

bool IsGuestRangeWritable(ThreadContext* context,
                          void* destination,
                          std::uint32_t byte_count)
{
    return IsGuestRangeReadable(context, destination, byte_count);
}

bool HandlePrivilegedTrapInstruction(CONTEXT* win32_context,
                                     ThreadContext* context);
bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context);
bool HandleTracedDosInterrupt21(CONTEXT* win32_context,
                                ThreadContext* context);
bool HandleTracedDosInterrupt2F(CONTEXT* win32_context,
                                ThreadContext* context);
bool HandleTracedDpmiInterrupt31(CONTEXT* win32_context,
                                 ThreadContext* context);
bool HandleTracedMouseInterrupt33(CONTEXT* win32_context,
                                  ThreadContext* context);
bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context);
bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context);
bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context);
bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context);
bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);
bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);
bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);
bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);
bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context);
bool HandleDosMemoryAccess(CONTEXT* win32_context,
                           ThreadContext* context);

bool IsGuestInstructionPointer(ThreadContext* context, std::uint32_t eip)
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

bool HandleSingleStepTrace(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->enable_single_step_trace)
    {
        return false;
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
    }

    if (context->enable_privileged_trap_hle &&
        HandlePrivilegedTrapInstruction(win32_context, context))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }
    if (context->enable_privileged_trap_hle &&
        HandlePortIoInstruction(win32_context, context))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }
    if (context->enable_traced_dos_hle &&
        (HandleTracedDosInterrupt21(win32_context, context) ||
         HandleTracedDosInterrupt2F(win32_context, context) ||
         HandleTracedDpmiInterrupt31(win32_context, context) ||
         HandleTracedMouseInterrupt33(win32_context, context)))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }
    if (context->enable_segment_load_hle &&
        (HandleSegmentLoadInstruction(win32_context, context) ||
         HandleSegmentStoreInstruction(win32_context, context) ||
         HandleSegmentOverrideByteLoadInstruction(win32_context, context) ||
         HandleSegmentMemoryCompareInstruction(win32_context, context) ||
         HandleSegmentMemoryLoadInstruction(win32_context, context) ||
         HandleTracedMemoryLoadInstruction(win32_context, context) ||
         HandleTracedMemoryStoreInstruction(win32_context, context) ||
         HandleTracedMemoryTestInstruction(win32_context, context) ||
         HandleTracedFpuMemoryInstruction(win32_context, context) ||
         HandleDosMemoryAccess(win32_context, context)))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }

    win32_context->EFlags |= 0x00000100U;
    return true;
}

bool WriteGuestUInt16(ThreadContext* context,
                      void* destination,
                      std::uint16_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest segment store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    DWORD ignored_protect = 0;
    VirtualProtect(destination,
                   sizeof(value),
                   previous_protect,
                   &ignored_protect);
    return true;
}

bool WriteGuestUInt8(ThreadContext* context,
                     void* destination,
                     std::uint8_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest byte store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    DWORD ignored_protect = 0;
    VirtualProtect(destination,
                   sizeof(value),
                   previous_protect,
                   &ignored_protect);
    return true;
}

bool WriteGuestUInt32(ThreadContext* context,
                      void* destination,
                      std::uint32_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest dword store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    DWORD ignored_protect = 0;
    VirtualProtect(destination,
                   sizeof(value),
                   previous_protect,
                   &ignored_protect);
    return true;
}

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
    VirtualProtect(destination,
                   byte_count,
                   previous_protect,
                   &ignored_protect);
    return true;
}

bool ReadGuestUInt32(ThreadContext* context,
                     const void* source,
                     std::uint32_t* value)
{
    if (value == nullptr ||
        !IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }

    std::memcpy(value, source, sizeof(*value));
    return true;
}

void WriteShadowMemory(ThreadContext* context,
                       std::uint32_t destination,
                       std::uint32_t value,
                       std::uint32_t byte_count)
{
    if (context == nullptr || byte_count == 0)
    {
        return;
    }

    ++context->shadow_memory_write_count;
    for (std::uint32_t index = 0; index < byte_count; ++index)
    {
        const std::uint32_t address = destination + index;
        context->shadow_memory[address] =
            static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU);
        if (!context->shadow_memory_range_valid)
        {
            context->shadow_memory_range_valid = true;
            context->shadow_memory_min_address = address;
            context->shadow_memory_max_address = address;
        }
        else
        {
            context->shadow_memory_min_address =
                std::min(context->shadow_memory_min_address, address);
            context->shadow_memory_max_address =
                std::max(context->shadow_memory_max_address, address);
        }
    }
}

bool ReadShadowUInt32(ThreadContext* context,
                      std::uint32_t source,
                      std::uint32_t* value)
{
    if (value == nullptr)
    {
        return false;
    }

    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const auto found = context->shadow_memory.find(source + index);
        if (found == context->shadow_memory.end())
        {
            return false;
        }
        result |= static_cast<std::uint32_t>(found->second) << (index * 8);
    }

    *value = result;
    ++context->shadow_memory_read_hit_count;
    return true;
}

bool AppendConsoleOutput(ThreadContext* context,
                         const void* source,
                         std::uint32_t byte_count)
{
    if (context == nullptr || source == nullptr || byte_count == 0)
    {
        return false;
    }

    if (!IsGuestRangeReadable(context, source, byte_count))
    {
        context->hle_message = "DOS console output buffer is outside runtime memory";
        return false;
    }

    const std::uint32_t available =
        sizeof(context->hle_console_output) -
        context->hle_console_output_size;
    const std::uint32_t copied = std::min(byte_count, available);
    if (copied == 0)
    {
        return false;
    }

    std::memcpy(
        context->hle_console_output + context->hle_console_output_size,
        source,
        copied);
    context->hle_console_output_size += copied;
    return true;
}

bool ReadGuestAsciz(ThreadContext* context,
                    std::uint32_t address,
                    std::uint32_t max_length,
                    std::string* value)
{
    if (context == nullptr || value == nullptr || max_length == 0)
    {
        return false;
    }

    value->clear();
    const char* text = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(address));
    for (std::uint32_t index = 0; index < max_length; ++index)
    {
        if (!IsGuestRangeReadable(context, text + index, 1))
        {
            return false;
        }

        const char ch = text[index];
        if (ch == '\0')
        {
            return true;
        }
        value->push_back(ch);
    }

    return false;
}

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
                               std::uint16_t ax);

void RecordDosChangeDirectory(ThreadContext* context,
                              const std::string& guest_path,
                              const repiu::hle::DosResolvedPath& resolved)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_chdir_count;
    context->last_dos_chdir_guest_path = guest_path;
    context->last_dos_chdir_host_path = resolved.host_path.string();
    context->last_dos_chdir_virtual_path = resolved.dos_path;
    context->last_dos_chdir_success =
        resolved.result == repiu::hle::DosPathResult::kOk;
    context->last_dos_chdir_error =
        repiu::hle::DosPathResultToErrorCode(resolved.result);
}

bool HandleDosChangeDirectory(CONTEXT* win32_context, ThreadContext* context)
{
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    if (!ReadGuestAsciz(context,
                        static_cast<std::uint32_t>(win32_context->Edx),
                        260,
                        &guest_path))
    {
        resolved.result = repiu::hle::DosPathResult::kPathNotFound;
        resolved.guest_path = "";
        resolved.message = "DOS chdir path is outside runtime memory";
        RecordDosChangeDirectory(context, guest_path, resolved);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (!repiu::hle::ChangeDosCurrentDirectory(
            &context->dos_file_system,
            guest_path,
            &resolved))
    {
        return false;
    }

    RecordDosChangeDirectory(context, guest_path, resolved);
    if (resolved.result == repiu::hle::DosPathResult::kOk)
    {
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

bool HandleDosGetCurrentDirectory(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::string current_directory =
        repiu::hle::GetDosCurrentDirectory(context->dos_file_system);
    const std::uint8_t drive = static_cast<std::uint8_t>(
        win32_context->Edx & 0xFFU);
    ++context->handled_dos_getcwd_count;
    context->last_dos_getcwd_drive = drive;
    context->last_dos_getcwd_path = current_directory;
    if (current_directory.size() >= 64)
    {
        context->last_dos_getcwd_success = false;
        context->last_dos_getcwd_error = 0x0005U;
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0005U;
        win32_context->EFlags |= 1U;
        return true;
    }

    std::string asciz = current_directory;
    asciz.push_back('\0');
    void* destination = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(win32_context->Esi));
    if (!WriteGuestBytes(context,
                         destination,
                         asciz.data(),
                         asciz.size()))
    {
        context->last_dos_getcwd_success = false;
        context->last_dos_getcwd_error = 0x0003U;
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    context->last_dos_getcwd_success = true;
    context->last_dos_getcwd_error = 0;
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosOpen(ThreadContext* context,
                   const std::string& guest_path,
                   const repiu::hle::DosResolvedPath& resolved,
                   std::uint16_t handle,
                   std::uint8_t access_mode)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_open_count;
    context->last_dos_open_guest_path = guest_path;
    context->last_dos_open_host_path = resolved.host_path.string();
    context->last_dos_open_virtual_path = resolved.dos_path;
    context->last_dos_open_success =
        resolved.result == repiu::hle::DosPathResult::kOk;
    context->last_dos_open_error =
        repiu::hle::DosPathResultToErrorCode(resolved.result);
    context->last_dos_open_handle = handle;
    context->last_dos_open_access_mode = access_mode;
}

bool HandleDosOpenFile(CONTEXT* win32_context, ThreadContext* context)
{
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    const std::uint8_t access_mode = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    if (!ReadGuestAsciz(context,
                        static_cast<std::uint32_t>(win32_context->Edx),
                        260,
                        &guest_path))
    {
        resolved.result = repiu::hle::DosPathResult::kFileNotFound;
        resolved.guest_path = "";
        resolved.message = "DOS open path is outside runtime memory";
        RecordDosOpen(context, guest_path, resolved, 0, access_mode);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0002U;
        win32_context->EFlags |= 1U;
        return true;
    }

    std::uint16_t handle = 0;
    if (!repiu::hle::OpenDosFile(&context->dos_file_system,
                                 guest_path,
                                 access_mode,
                                 &resolved,
                                 &handle))
    {
        return false;
    }

    RecordDosOpen(context, guest_path, resolved, handle, access_mode);
    if (resolved.result == repiu::hle::DosPathResult::kOk)
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | handle;
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

void RecordDosIoctl(ThreadContext* context,
                    std::uint8_t subfunction,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error,
                    std::uint16_t device_info)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_ioctl_count;
    context->last_dos_ioctl_subfunction = subfunction;
    context->last_dos_ioctl_handle = handle;
    context->last_dos_ioctl_success = success;
    context->last_dos_ioctl_error = error;
    context->last_dos_ioctl_device_info = device_info;
}

bool HandleDosIoctl(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t subfunction = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    if (subfunction != 0x00)
    {
        std::ostringstream stream;
        stream << "unsupported DOS IOCTL subfunction AL=0x"
               << std::hex << static_cast<unsigned>(subfunction);
        context->hle_message = stream.str();
        return false;
    }

    std::uint16_t device_info = 0;
    if (handle < 5)
    {
        device_info = 0x0080;
        RecordDosIoctl(context, subfunction, handle, true, 0, device_info);
        win32_context->Edx =
            (win32_context->Edx & 0xFFFF0000U) | device_info;
        win32_context->EFlags &= ~1U;
        return true;
    }

    if (repiu::hle::IsDosFileHandleOpen(context->dos_file_system, handle))
    {
        RecordDosIoctl(context, subfunction, handle, true, 0, device_info);
        win32_context->Edx =
            (win32_context->Edx & 0xFFFF0000U) | device_info;
        win32_context->EFlags &= ~1U;
        return true;
    }

    constexpr std::uint16_t kInvalidHandle = 0x0006;
    RecordDosIoctl(context,
                   subfunction,
                   handle,
                   false,
                   kInvalidHandle,
                   0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFF0000U) | kInvalidHandle;
    win32_context->EFlags |= 1U;
    return true;
}

void RecordDosResize(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint16_t paragraphs,
                     bool success,
                     std::uint16_t error)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_resize_count;
    context->last_dos_resize_selector = selector;
    context->last_dos_resize_paragraphs = paragraphs;
    context->last_dos_resize_success = success;
    context->last_dos_resize_error = error;
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

bool HandleDosResizeMemoryBlock(CONTEXT* win32_context,
                                ThreadContext* context)
{
    const std::uint16_t paragraphs = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    constexpr std::uint16_t kObservedPiuResizeSelector = 0x0024;
    constexpr std::uint16_t kObservedPiuStageCfgFailureLimitParagraphs =
        0x4AE0;
    constexpr std::uint16_t kObservedPiuResizeLimitParagraphs = 0xE700;
    constexpr std::uint16_t kInsufficientMemory = 0x0008;
    if (context->guest_es == kObservedPiuResizeSelector &&
        paragraphs > kObservedPiuResizeLimitParagraphs)
    {
        RecordDosResize(context,
                        context->guest_es,
                        paragraphs,
                        false,
                        kInsufficientMemory);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | kInsufficientMemory;
        win32_context->Ebx =
            (win32_context->Ebx & 0xFFFF0000U) |
            kObservedPiuResizeLimitParagraphs;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (context->guest_es == kObservedPiuResizeSelector &&
        !context->last_dos_open_success &&
        context->last_dos_open_guest_path == "stage.cfg" &&
        paragraphs > kObservedPiuStageCfgFailureLimitParagraphs)
    {
        RecordDosResize(context,
                        context->guest_es,
                        paragraphs,
                        false,
                        kInsufficientMemory);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | kInsufficientMemory;
        win32_context->Ebx =
            (win32_context->Ebx & 0xFFFF0000U) |
            kObservedPiuStageCfgFailureLimitParagraphs;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosResize(context, context->guest_es, paragraphs, true, 0);
    win32_context->EFlags &= ~1U;
    return true;
}

void HandleDosGetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t vector = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const DosInterruptVectorShadow& entry =
        context->dos_interrupt_vectors[vector];
    const std::uint16_t segment = entry.valid ? entry.segment : 0;
    const std::uint16_t offset = entry.valid ? entry.offset : 0;

    context->guest_es = segment;
    win32_context->SegEs = segment;
    win32_context->Ebx =
        (win32_context->Ebx & 0xFFFF0000U) | offset;
    win32_context->EFlags &= ~1U;
}

void HandleDosSetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t vector = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    DosInterruptVectorShadow& entry =
        context->dos_interrupt_vectors[vector];

    entry.segment = context->guest_ds != 0
        ? context->guest_ds
        : static_cast<std::uint16_t>(win32_context->SegDs);
    entry.offset = static_cast<std::uint16_t>(
        win32_context->Edx & 0xFFFFU);
    entry.valid = true;
    win32_context->EFlags &= ~1U;
}

bool HandleDosInterrupt21(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFF);

    switch (ah)
    {
        case 0x09:
        {
            const char* text = reinterpret_cast<const char*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            if (text == nullptr ||
                !IsGuestRangeReadable(context, text, 1))
            {
                win32_context->Eax = 0;
                break;
            }

            std::uint32_t length = 0;
            while (length < 4096 &&
                   IsGuestRangeReadable(context, text, length + 1) &&
                   text[length] != '$')
            {
                ++length;
            }
            AppendConsoleOutput(context, text, length);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFFFF00U) | static_cast<DWORD>('$');
            break;
        }
        case 0x30:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x0007U;
            win32_context->Ebx = 0;
            win32_context->Ecx = 0;
            break;
        case 0x25:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetInterruptVector(win32_context, context);
            break;
        case 0x35:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetInterruptVector(win32_context, context);
            break;
        case 0x3B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosChangeDirectory(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3D:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosOpenFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x40:
        {
            const void* text = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            const std::uint32_t byte_count = win32_context->Ecx;
            AppendConsoleOutput(context, text, byte_count);
            win32_context->Eax = byte_count;
            win32_context->EFlags &= ~1U;
            break;
        }
        case 0x44:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosIoctl(win32_context, context))
            {
                return false;
            }
            break;
        case 0x47:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosGetCurrentDirectory(win32_context, context))
            {
                return false;
            }
            break;
        case 0x4C:
            RecoverFromHleExit(win32_context, context);
            return true;
        case 0x4A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosResizeMemoryBlock(win32_context, context))
            {
                return false;
            }
            break;
        case 0xFF:
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            break;
        case 0xED:
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            break;
        default:
        {
            std::ostringstream stream;
            stream << "unsupported DOS INT 21h AH=0x"
                   << std::hex << static_cast<unsigned>(ah);
            context->hle_message = stream.str();
            return false;
        }
    }

    win32_context->Eip += 2;
    return true;
}

bool HandleDosInterrupt2F(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x1686)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported DOS interrupt 0x2f AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

bool HandleDpmiInterrupt31(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x0400)
    {
        RecordHandledDosInterrupt(context, 0x31, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported DPMI INT 31h AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

bool HandleMouseInterrupt33(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x0000)
    {
        RecordHandledDosInterrupt(context, 0x33, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->Ebx &= 0xFFFF0000U;
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0002)
    {
        RecordHandledDosInterrupt(context, 0x33, ax);
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported mouse INT 33h AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
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

std::uint16_t ReadRegister16(const CONTEXT& win32_context,
                             std::uint8_t register_id)
{
    switch (register_id & 0x07)
    {
        case 0:
            return static_cast<std::uint16_t>(win32_context.Eax & 0xFFFFU);
        case 1:
            return static_cast<std::uint16_t>(win32_context.Ecx & 0xFFFFU);
        case 2:
            return static_cast<std::uint16_t>(win32_context.Edx & 0xFFFFU);
        case 3:
            return static_cast<std::uint16_t>(win32_context.Ebx & 0xFFFFU);
        case 4:
            return static_cast<std::uint16_t>(win32_context.Esp & 0xFFFFU);
        case 5:
            return static_cast<std::uint16_t>(win32_context.Ebp & 0xFFFFU);
        case 6:
            return static_cast<std::uint16_t>(win32_context.Esi & 0xFFFFU);
        case 7:
            return static_cast<std::uint16_t>(win32_context.Edi & 0xFFFFU);
        default:
            return 0;
    }
}

void RecordGuestSegmentLoad(CONTEXT* win32_context,
                            ThreadContext* context,
                            std::uint8_t segment_register,
                            std::uint16_t selector,
                            std::uint32_t source)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_load_count;
    context->last_segment_load_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_load_opcode = 0x8E;
    context->last_segment_load_register = segment_register;
    context->last_segment_load_selector = selector;
    context->last_segment_load_source = source;

    switch (segment_register)
    {
        case 0:
            context->guest_es = selector;
            break;
        case 2:
            context->guest_ss = selector;
            break;
        case 3:
            context->guest_ds = selector;
            break;
        case 4:
            context->guest_fs = selector;
            break;
        case 5:
            context->guest_gs = selector;
            break;
        default:
            break;
    }
}

std::uint16_t ReadGuestSegmentSelector(const ThreadContext& context,
                                       std::uint8_t segment_register)
{
    switch (segment_register)
    {
        case 0:
            return context.guest_es;
        case 2:
            return context.guest_ss;
        case 3:
            return context.guest_ds;
        case 4:
            return context.guest_fs;
        case 5:
            return context.guest_gs;
        default:
            return 0;
    }
}

void RecordGuestSegmentStore(CONTEXT* win32_context,
                             ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t destination)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_store_count;
    context->last_segment_store_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_store_opcode = 0x8C;
    context->last_segment_store_register = segment_register;
    context->last_segment_store_selector = selector;
    context->last_segment_store_destination = destination;
}

void RecordGuestSegmentMemoryLoad(CONTEXT* win32_context,
                                  ThreadContext* context,
                                  std::uint8_t opcode,
                                  std::uint8_t segment_register,
                                  std::uint16_t selector,
                                  std::uint32_t offset,
                                  std::uint32_t byte_width,
                                  std::uint32_t value)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_memory_load_count;
    context->last_segment_memory_load_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_memory_load_opcode = opcode;
    context->last_segment_memory_load_register = segment_register;
    context->last_segment_memory_load_selector = selector;
    context->last_segment_memory_load_offset = offset;
    context->last_segment_memory_load_width = byte_width;
    context->last_segment_memory_load_value = value;
}

void RecordGuestMemoryStore(CONTEXT* win32_context,
                            ThreadContext* context,
                            std::uint32_t opcode,
                            std::uint32_t destination,
                            std::uint32_t value,
                            std::uint32_t byte_width,
                            const char* source_kind,
                            bool applied)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_memory_store_count;
    context->last_memory_store_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_memory_store_opcode = opcode;
    context->last_memory_store_destination = destination;
    context->last_memory_store_value = value;
    context->last_memory_store_width = byte_width;
    context->last_memory_store_source_kind =
        source_kind != nullptr ? source_kind : "unknown";
    context->last_memory_store_applied = applied;
}

std::uint32_t ReadGeneralRegister32(const CONTEXT* win32_context,
                                    std::uint8_t register_index)
{
    switch (register_index & 0x07U)
    {
        case 0:
            return win32_context->Eax;
        case 1:
            return win32_context->Ecx;
        case 2:
            return win32_context->Edx;
        case 3:
            return win32_context->Ebx;
        case 4:
            return win32_context->Esp;
        case 5:
            return win32_context->Ebp;
        case 6:
            return win32_context->Esi;
        case 7:
            return win32_context->Edi;
        default:
            return 0;
    }
}

void WriteGeneralRegister32(CONTEXT* win32_context,
                            std::uint8_t register_index,
                            std::uint32_t value)
{
    switch (register_index & 0x07U)
    {
        case 0:
            win32_context->Eax = value;
            break;
        case 1:
            win32_context->Ecx = value;
            break;
        case 2:
            win32_context->Edx = value;
            break;
        case 3:
            win32_context->Ebx = value;
            break;
        case 4:
            win32_context->Esp = value;
            break;
        case 5:
            win32_context->Ebp = value;
            break;
        case 6:
            win32_context->Esi = value;
            break;
        case 7:
            win32_context->Edi = value;
            break;
        default:
            break;
    }
}

bool DecodeModRmMemoryDestinationNoSib(
    const CONTEXT* win32_context,
    const std::uint8_t* instruction,
    std::uint32_t* destination,
    std::uint32_t* instruction_size)
{
    const std::uint8_t modrm = instruction[1];
    const std::uint8_t mod = (modrm >> 6) & 0x03U;
    const std::uint8_t rm = modrm & 0x07U;
    if (mod == 0x03 || rm == 0x04)
    {
        return false;
    }

    std::uint32_t base = 0;
    std::uint32_t displacement = 0;
    std::uint32_t size = 2;
    if (mod == 0x00 && rm == 0x05)
    {
        return false;
    }

    base = ReadGeneralRegister32(win32_context, rm);
    if (mod == 0x01)
    {
        displacement = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(
                static_cast<std::int8_t>(instruction[2])));
        size = 3;
    }
    else if (mod == 0x02)
    {
        displacement =
            static_cast<std::uint32_t>(instruction[2]) |
            (static_cast<std::uint32_t>(instruction[3]) << 8) |
            (static_cast<std::uint32_t>(instruction[4]) << 16) |
            (static_cast<std::uint32_t>(instruction[5]) << 24);
        size = 6;
    }

    *destination = base + displacement;
    *instruction_size = size;
    return true;
}

bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context);

bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context);

bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context);

bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);

bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context);

bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);

bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context);

bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint32_t prefix_length = 0;
    while (instruction[prefix_length] == 0x26 ||
           instruction[prefix_length] == 0x2E ||
           instruction[prefix_length] == 0x36 ||
           instruction[prefix_length] == 0x3E ||
           instruction[prefix_length] == 0x64 ||
           instruction[prefix_length] == 0x65 ||
           instruction[prefix_length] == 0x66 ||
           instruction[prefix_length] == 0x67)
    {
        ++prefix_length;
    }

    if (instruction[prefix_length] != 0x8E)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[prefix_length + 1];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t segment_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t source_register =
        static_cast<std::uint8_t>(modrm & 0x07);

    if (segment_register == 1 || segment_register > 5)
    {
        return false;
    }

    std::uint16_t selector = 0;
    std::uint32_t source = 0;
    std::uint32_t instruction_length = prefix_length + 2;
    if (mod == 0x03)
    {
        selector = ReadRegister16(*win32_context, source_register);
    }
    else if (mod == 0x00 && source_register == 0x05)
    {
        source =
            static_cast<std::uint32_t>(instruction[prefix_length + 2]) |
            (static_cast<std::uint32_t>(instruction[prefix_length + 3])
             << 8) |
            (static_cast<std::uint32_t>(instruction[prefix_length + 4])
             << 16) |
            (static_cast<std::uint32_t>(instruction[prefix_length + 5])
             << 24);
        const void* source_pointer = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(source));
        if (!IsGuestRangeReadable(context, source_pointer, 2))
        {
            return false;
        }

        std::memcpy(&selector, source_pointer, sizeof(selector));
        instruction_length += 4;
    }
    else
    {
        return false;
    }

    RecordGuestSegmentLoad(win32_context,
                           context,
                           segment_register,
                           selector,
                           source);
    win32_context->Eip += instruction_length;
    HandleSegmentStoreInstruction(win32_context, context);
    return true;
}

bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x66 || instruction[1] != 0x26 ||
        instruction[2] != 0x8C)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[3];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t segment_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t rm = static_cast<std::uint8_t>(modrm & 0x07);
    if (mod != 0x00 || rm != 0x05)
    {
        return false;
    }

    if (segment_register == 1 || segment_register > 5)
    {
        return false;
    }

    const std::uint32_t destination =
        static_cast<std::uint32_t>(instruction[4]) |
        (static_cast<std::uint32_t>(instruction[5]) << 8) |
        (static_cast<std::uint32_t>(instruction[6]) << 16) |
        (static_cast<std::uint32_t>(instruction[7]) << 24);
    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    if (!IsGuestRangeWritable(context, destination_pointer, 2))
    {
        return false;
    }

    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);
    if (!WriteGuestUInt16(context, destination_pointer, selector))
    {
        return false;
    }

    RecordGuestSegmentStore(win32_context,
                            context,
                            segment_register,
                            selector,
                            destination);
    win32_context->Eip += 8;
    return true;
}

bool ReadSegmentOverrideByte(ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t offset,
                             std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (segment_register == 0 && selector == context->guest_es &&
        selector != 0 && offset == 0x80)
    {
        *value = 0;
        return true;
    }

    return false;
}

bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x26 || instruction[1] != 0x8A ||
        instruction[2] != 0x4F)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[2];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t destination_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t base_register =
        static_cast<std::uint8_t>(modrm & 0x07);
    if (mod != 0x01 || destination_register != 0x01 ||
        base_register != 0x07)
    {
        return false;
    }

    const std::int8_t displacement =
        static_cast<std::int8_t>(instruction[3]);
    const std::uint32_t offset =
        static_cast<std::uint32_t>(win32_context->Edi + displacement);
    const std::uint8_t segment_register = 0;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);

    std::uint8_t value = 0;
    if (!ReadSegmentOverrideByte(
            context, segment_register, selector, offset, &value))
    {
        return false;
    }

    win32_context->Ecx =
        (win32_context->Ecx & 0xFFFFFF00U) | value;
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x8A,
                                 segment_register,
                                 selector,
                                 offset,
                                 1,
                                 value);
    win32_context->Eip += 4;
    return true;
}

void RecordDosEnvironmentAccess(ThreadContext* context, std::uint32_t offset)
{
    if (context == nullptr ||
        offset >= context->dos_environment_block.size())
    {
        return;
    }

    const auto& block = context->dos_environment_block;
    std::size_t cursor = 0;
    while (cursor < block.size() && block[cursor] != 0)
    {
        const std::size_t entry_begin = cursor;
        while (cursor < block.size() && block[cursor] != 0)
        {
            ++cursor;
        }
        const std::size_t entry_end = cursor;

        if (offset >= entry_begin && offset <= entry_end)
        {
            std::size_t equals = entry_begin;
            while (equals < entry_end && block[equals] != '=')
            {
                ++equals;
            }

            const std::size_t name_end =
                equals < entry_end ? equals : entry_end;
            const std::size_t value_begin =
                equals < entry_end ? equals + 1 : entry_end;
            context->last_dos_environment_access_valid = true;
            context->last_dos_environment_access_offset = offset;
            context->last_dos_environment_entry_offset =
                static_cast<std::uint32_t>(entry_begin);
            context->last_dos_environment_value_length =
                static_cast<std::uint32_t>(entry_end - value_begin);
            if (name_end > entry_begin)
            {
                context->last_dos_environment_entry_name.assign(
                    reinterpret_cast<const char*>(&block[entry_begin]),
                    name_end - entry_begin);
            }
            else
            {
                context->last_dos_environment_entry_name = "<unnamed>";
            }
            context->diagnostic_progress_count.fetch_add(
                1,
                std::memory_order_relaxed);
            return;
        }

        if (cursor < block.size())
        {
            ++cursor;
        }
    }
}

bool ReadSegmentDword(ThreadContext* context,
                      std::uint8_t segment_register,
                      std::uint16_t selector,
                      std::uint32_t offset,
                      std::uint32_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (segment_register == 3 && selector == context->guest_ds &&
        selector != 0 && offset < 0x10000)
    {
        RecordDosEnvironmentAccess(context, offset);
        std::uint32_t result = 0;
        for (std::uint32_t index = 0; index < 4; ++index)
        {
            std::uint8_t byte = 0;
            const std::uint32_t byte_offset = offset + index;
            if (byte_offset >= offset &&
                byte_offset < context->dos_environment_block.size())
            {
                byte = context->dos_environment_block[byte_offset];
            }
            result |= static_cast<std::uint32_t>(byte) << (index * 8);
        }
        *value = result;
        return true;
    }

    return false;
}

bool ReadSegmentByte(ThreadContext* context,
                     std::uint8_t segment_register,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (segment_register == 3 && selector == context->guest_ds &&
        selector != 0 && offset < 0x10000)
    {
        RecordDosEnvironmentAccess(context, offset);
        if (offset < context->dos_environment_block.size())
        {
            *value = context->dos_environment_block[offset];
        }
        else
        {
            *value = 0;
        }
        return true;
    }

    return false;
}

bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    const std::uint8_t segment_register = 3;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);

    if (instruction[0] == 0x8B && instruction[1] == 0x06)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint32_t value = 0;
        if (!ReadSegmentDword(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        win32_context->Eax = value;
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0x8B,
                                     segment_register,
                                     selector,
                                     offset,
                                     4,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              offset,
                              value);
        win32_context->Eip += 2;
        return true;
    }

    if (instruction[0] == 0xAC)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint8_t value = 0;
        if (!ReadSegmentByte(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        win32_context->Eax =
            (win32_context->Eax & 0xFFFFFF00U) | value;
        if ((win32_context->EFlags & 0x400U) != 0)
        {
            --win32_context->Esi;
        }
        else
        {
            ++win32_context->Esi;
        }
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0xAC,
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              offset,
                              value);
        ++win32_context->Eip;
        return true;
    }

    if (instruction[0] == 0xA4)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint8_t value = 0;
        if (!ReadSegmentByte(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        const std::uint32_t destination_address = win32_context->Edi;
        void* destination = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination_address));
        if (!WriteGuestUInt8(context, destination, value))
        {
            return false;
        }

        if ((win32_context->EFlags & 0x400U) != 0)
        {
            --win32_context->Esi;
            --win32_context->Edi;
        }
        else
        {
            ++win32_context->Esi;
            ++win32_context->Edi;
        }
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0xA4,
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              destination_address,
                              value);
        ++win32_context->Eip;
        return true;
    }

    return false;
}

bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x80 || instruction[1] != 0x3E)
    {
        return false;
    }

    const std::uint8_t segment_register = 3;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);
    const std::uint32_t offset = win32_context->Esi;

    std::uint8_t value = 0;
    if (!ReadSegmentByte(
            context, segment_register, selector, offset, &value))
    {
        return false;
    }

    const std::uint8_t immediate = instruction[2];
    if (value == immediate)
    {
        win32_context->EFlags |= 0x40U;
    }
    else
    {
        win32_context->EFlags &= ~0x40U;
    }
    win32_context->EFlags &= ~1U;
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x80,
                                 segment_register,
                                 selector,
                                 offset,
                                 1,
                                 value);
    RecordLowMemoryAccess(win32_context,
                          context,
                          instruction[0],
                          offset,
                          value);
    win32_context->Eip += 3;
    return true;
}

bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint32_t destination = 0;
    std::uint32_t value = 0;
    std::uint32_t instruction_size = 0;
    std::uint32_t value_width = 4;
    std::uint32_t store_opcode = instruction[0];
    const char* source_kind = "unknown";

    if (instruction[0] == 0xC7)
    {
        source_kind = "mov-imm32";
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        if (operation != 0 ||
            !DecodeModRmMemoryDestinationNoSib(win32_context,
                                               instruction,
                                               &destination,
                                               &instruction_size))
        {
            return false;
        }
        const std::uint32_t immediate_offset = instruction_size;
        value =
            static_cast<std::uint32_t>(instruction[immediate_offset]) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 1]) << 8) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 2]) << 16) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 3]) << 24);
        instruction_size += 4;
    }
    else if (instruction[0] == 0x66 && instruction[1] == 0xC7)
    {
        store_opcode = 0x66C7;
        source_kind = "mov-imm16";
        const std::uint8_t operation = (instruction[2] >> 3) & 0x07U;
        std::uint32_t unprefixed_instruction_size = 0;
        if (operation != 0 ||
            !DecodeModRmMemoryDestinationNoSib(
                win32_context,
                instruction + 1,
                &destination,
                &unprefixed_instruction_size))
        {
            return false;
        }
        const std::uint32_t immediate_offset =
            1 + unprefixed_instruction_size;
        value =
            static_cast<std::uint32_t>(instruction[immediate_offset]) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 1]) << 8);
        instruction_size = immediate_offset + 2;
        value_width = 2;
    }
    else if (instruction[0] == 0x89)
    {
        source_kind = "mov-reg32";
        if (!DecodeModRmMemoryDestinationNoSib(win32_context,
                                               instruction,
                                               &destination,
                                               &instruction_size))
        {
            return false;
        }
        const std::uint8_t source_register = (instruction[1] >> 3) & 0x07U;
        value = ReadGeneralRegister32(win32_context, source_register);
    }
    else
    {
        return false;
    }

    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));

    if (IsGuestRangeWritable(context, destination_pointer, value_width))
    {
        const bool written = value_width == 2
            ? WriteGuestUInt16(context,
                               destination_pointer,
                               static_cast<std::uint16_t>(value))
            : WriteGuestUInt32(context, destination_pointer, value);
        if (!written)
        {
            return false;
        }
        RecordGuestMemoryStore(win32_context,
                               context,
                               store_opcode,
                               destination,
                               value,
                               value_width,
                               source_kind,
                               true);
        win32_context->Eip += instruction_size;
        return true;
    }

    if (context->last_dos_open_success ||
        context->last_dos_open_guest_path.empty())
    {
        return false;
    }

    RecordGuestMemoryStore(win32_context,
                           context,
                           store_opcode,
                           destination,
                           value,
                           value_width,
                           source_kind,
                           false);
    WriteShadowMemory(context, destination, value, value_width);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x8B)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryDestinationNoSib(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    std::uint32_t value = 0;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    if (!ReadGuestUInt32(context, source_pointer, &value) &&
        !ReadShadowUInt32(context, source, &value))
    {
        return false;
    }

    const std::uint8_t destination_register = (instruction[1] >> 3) & 0x07U;
    WriteGeneralRegister32(win32_context, destination_register, value);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xF7 || instruction[1] != 0x07)
    {
        return false;
    }

    const std::uint32_t source = win32_context->Edi;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    std::uint32_t source_value = 0;
    if (!ReadGuestUInt32(context, source_pointer, &source_value) &&
        !ReadShadowUInt32(context, source, &source_value))
    {
        return false;
    }

    const std::uint32_t mask =
        static_cast<std::uint32_t>(instruction[2]) |
        (static_cast<std::uint32_t>(instruction[3]) << 8) |
        (static_cast<std::uint32_t>(instruction[4]) << 16) |
        (static_cast<std::uint32_t>(instruction[5]) << 24);
    const std::uint32_t result = source_value & mask;

    win32_context->EFlags &= ~1U;
    win32_context->EFlags &= ~0x800U;
    if (result == 0)
    {
        win32_context->EFlags |= 0x40U;
    }
    else
    {
        win32_context->EFlags &= ~0x40U;
    }

    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= 0x80U;
    }
    else
    {
        win32_context->EFlags &= ~0x80U;
    }

    win32_context->Eip += 6;
    return true;
}

bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xD9)
    {
        return false;
    }

    std::uint32_t address = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryDestinationNoSib(win32_context,
                                           instruction,
                                           &address,
                                           &instruction_size))
    {
        return false;
    }

    const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(address));
    if (operation == 0)
    {
        std::uint32_t value = 0;
        if (ReadGuestUInt32(context, source_pointer, &value))
        {
            context->last_traced_fpu_m32_value = value;
            context->has_last_traced_fpu_m32_value = true;
            win32_context->Eip += instruction_size;
            return true;
        }
        if (ReadShadowUInt32(context, address, &value))
        {
            context->last_traced_fpu_m32_value = value;
            context->has_last_traced_fpu_m32_value = true;
            win32_context->Eip += instruction_size;
            return true;
        }
        return false;
    }

    if (operation != 2 && operation != 3)
    {
        return false;
    }
    if (!context->has_last_traced_fpu_m32_value)
    {
        return false;
    }

    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(address));
    const std::uint32_t value = context->last_traced_fpu_m32_value;
    if (IsGuestRangeWritable(context, destination_pointer, sizeof(value)))
    {
        if (!WriteGuestUInt32(context, destination_pointer, value))
        {
            return false;
        }
        RecordGuestMemoryStore(win32_context,
                               context,
                               0xD9,
                               address,
                               value,
                               4,
                               "fpu-m32",
                               true);
        win32_context->Eip += instruction_size;
        return true;
    }

    if (context->last_dos_open_success ||
        context->last_dos_open_guest_path.empty())
    {
        return false;
    }

    RecordGuestMemoryStore(win32_context,
                           context,
                           0xD9,
                           address,
                           value,
                           4,
                           "fpu-m32",
                           false);
    WriteShadowMemory(context, address, value, 4);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedDosInterrupt21(CONTEXT* win32_context,
                                ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x21)
    {
        return false;
    }

    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFF);
    switch (ah)
    {
        case 0x30:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x0007U;
            win32_context->Ebx = 0;
            win32_context->Ecx = 0;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0x25:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetInterruptVector(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x35:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetInterruptVector(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x3B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosChangeDirectory(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3D:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosOpenFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x40:
        {
            RecordHandledDosInterrupt(context, 0x21, ax);
            const void* text = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            const std::uint32_t byte_count = win32_context->Ecx;
            AppendConsoleOutput(context, text, byte_count);
            win32_context->Eax = byte_count;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        }
        case 0x44:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosIoctl(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x47:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosGetCurrentDirectory(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x4C:
            RecordHandledDosInterrupt(context, 0x21, ax);
            RecoverFromHleExit(win32_context, context);
            return true;
        case 0xFF:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0xED:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0x4A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosResizeMemoryBlock(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        default:
            return false;
    }
}

bool HandleTracedDosInterrupt2F(CONTEXT* win32_context,
                                ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x2F)
    {
        return false;
    }

    return HandleDosInterrupt2F(win32_context, context);
}

bool HandleTracedDpmiInterrupt31(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x31)
    {
        return false;
    }

    return HandleDpmiInterrupt31(win32_context, context);
}

bool HandleTracedMouseInterrupt33(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x33)
    {
        return false;
    }

    return HandleMouseInterrupt33(win32_context, context);
}

bool HandlePrivilegedTrapInstruction(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (*instruction == 0xFA)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        win32_context->EFlags &= ~0x00000200U;
        ++win32_context->Eip;
        return true;
    }
    if (*instruction == 0xFB)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        win32_context->EFlags |= 0x00000200U;
        ++win32_context->Eip;
        return true;
    }

    return false;
}

void RecordPortIo(ThreadContext* context,
                  std::uint32_t address,
                  std::uint32_t opcode,
                  std::uint16_t port,
                  std::uint32_t width,
                  std::uint32_t value,
                  bool is_input,
                  bool handled,
                  const std::string& result)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->port_io.observed_count;
    context->port_io.last_address = address;
    context->port_io.last_opcode = opcode;
    context->port_io.last_port = port;
    context->port_io.last_width = width;
    context->port_io.last_value = value;
    context->port_io.last_is_input = is_input;
    context->port_io.last_handled = handled;
    context->port_io.last_result = result;
    if (context->port_io.trace_stored_count < kWin32PortIoTraceCapacity)
    {
        Win32PortIoTraceEntry& entry =
            context->port_io.trace[context->port_io.trace_stored_count];
        entry.valid = true;
        entry.sequence = context->port_io.observed_count;
        entry.address = address;
        entry.opcode = opcode;
        entry.port = port;
        entry.width = width;
        entry.value = value;
        entry.is_input = is_input;
        entry.handled = handled;
        ++context->port_io.trace_stored_count;
    }
}

bool IsObservedPortInitializationWrite(std::uint16_t port,
                                       std::uint32_t width,
                                       std::uint32_t value)
{
    if (width != 4)
    {
        return false;
    }

    return (port == 0x02AC && value == 0x00000010U) ||
           (port == 0x02A0 && value == 0x00000001U) ||
           (port == 0x02A2 && value == 0x00000000U);
}

bool IsPortIoTraceCandidate(std::uint16_t port,
                            std::uint32_t width,
                            bool is_input)
{
    return !is_input && width == 4 && port >= 0x02A0 && port <= 0x02AF;
}

bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x66 ||
        (instruction[1] != 0xEF && instruction[1] != 0xED))
    {
        return false;
    }

    const std::uint16_t port = static_cast<std::uint16_t>(
        win32_context->Edx & 0xFFFFU);
    const bool is_input = instruction[1] == 0xED;
    const std::uint32_t value = is_input ? 0 : win32_context->Eax;
    const std::uint32_t width = 4;
    const std::uint32_t opcode = is_input ? 0x66ED : 0x66EF;
    if (is_input)
    {
        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     true,
                     false,
                     "unsupported-in");
        std::ostringstream stream;
        stream << "unsupported port I/O IN EAX,DX port=0x"
               << std::hex << static_cast<unsigned>(port);
        context->hle_message = stream.str();
        return false;
    }

    if (IsObservedPortInitializationWrite(port, width, value))
    {
        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "ignored");
        win32_context->Eip += 2;
        return true;
    }

    if (IsPortIoTraceCandidate(port, width, false))
    {
        if (context->port_io.observed_count >= kWin32DeferredPortIoLimit)
        {
            context->port_io.trace_limit_reached = true;
            RecordPortIo(context,
                         static_cast<std::uint32_t>(win32_context->Eip),
                         opcode,
                         port,
                         width,
                         value,
                         false,
                         false,
                         "deferred-limit");
            std::ostringstream stream;
            stream << "deferred port I/O limit reached for OUT DX,EAX port=0x"
                   << std::hex << static_cast<unsigned>(port)
                   << " value=0x" << value;
            context->hle_message = stream.str();
            return false;
        }

        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "deferred-ignored");
        win32_context->Eip += 2;
        return true;
    }

    RecordPortIo(context,
                 static_cast<std::uint32_t>(win32_context->Eip),
                 opcode,
                 port,
                 width,
                 value,
                 false,
                 false,
                 "unsupported");
    std::ostringstream stream;
    stream << "unsupported port I/O OUT DX,EAX port=0x";
    stream << std::hex << static_cast<unsigned>(port)
           << " value=0x" << value;
    context->hle_message = stream.str();
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
            value = 0;
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

LONG WINAPI GuestStackVectoredExceptionHandler(
    EXCEPTION_POINTERS* exception_info)
{
    ThreadContext* context = g_active_thread_context;
    if (context == nullptr ||
        exception_info == nullptr || exception_info->ContextRecord == nullptr)
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
    if (exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_SINGLE_STEP &&
        HandleSingleStepTrace(win32_context, context))
    {
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
        HandleSegmentStoreInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentOverrideByteLoadInstruction(win32_context, context))
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
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_dos_hle &&
        exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_ACCESS_VIOLATION &&
        HandleDosMemoryAccess(win32_context, context))
    {
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
#endif

DWORD WINAPI GuestEntryThreadProc(void* parameter)
{
    ThreadContext* context = static_cast<ThreadContext*>(parameter);
    if (context == nullptr)
    {
        return 1;
    }

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
            context->active_call_state = &state;
            g_active_thread_context = context;
            void* vectored_handler = AddVectoredExceptionHandler(
                1, GuestStackVectoredExceptionHandler);
            if (vectored_handler == nullptr)
            {
                g_active_thread_context = nullptr;
                context->active_call_state = nullptr;
                return 5;
            }

            CallGuestEntryWithStack(&state);

            RemoveVectoredExceptionHandler(vectored_handler);
            g_active_thread_context = nullptr;
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
            g_active_thread_context = context;
            void* vectored_handler = AddVectoredExceptionHandler(
                1, GuestStackVectoredExceptionHandler);
            if (vectored_handler == nullptr)
            {
                g_active_thread_context = nullptr;
                return 5;
            }
#endif
            using EntryFunction = void (*)();
            EntryFunction entry = reinterpret_cast<EntryFunction>(
                static_cast<std::uintptr_t>(context->entry_address));
            entry();
#if defined(_MSC_VER) && defined(_M_IX86)
            RemoveVectoredExceptionHandler(vectored_handler);
            g_active_thread_context = nullptr;
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
    context.entry_address = entry_address;
    context.runtime_base = placement.placed_base;
    context.runtime_size = placement.placed_size;
    context.guest_initial_esp = guest_initial_esp;
    context.use_guest_stack = use_guest_stack;
    context.enable_privileged_trap_hle = enable_privileged_trap_hle;
    context.enable_traced_dos_hle = enable_traced_dos_hle;
    context.enable_segment_load_hle = enable_segment_load_hle;
    context.enable_dos_hle = enable_dos_hle;
    context.enable_single_step_trace = enable_single_step_trace;
    context.dos_environment_block = BuildDosEnvironmentBlock();
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

    HANDLE thread = api.create_thread(nullptr,
                                      0,
                                      GuestEntryThreadProc,
                                      &context,
                                      0,
                                      nullptr);
    if (thread == nullptr)
    {
        const DWORD error = api.get_last_error();
        std::ostringstream stream;
        stream << "CreateThread failed with error " << error;
        attempt->message = stream.str();
        return false;
    }

    attempt->attempted = true;
    attempt->guest_stack_switch_attempted = use_guest_stack;
    DWORD exit_code = 0;
    const DWORD wait_result = PollThreadUntilExit(
        thread,
        timeout_milliseconds,
        enable_single_step_trace ? &context : nullptr,
        &exit_code);

    if (wait_result == WAIT_TIMEOUT)
    {
        attempt->timed_out = true;
        attempt->thread_exit_code = 3;
        CopyThreadObservationToAttempt(context, attempt);
        attempt->valid = true;
        attempt->message = "minimal execution attempt timed out";
        if (api.terminate_thread != nullptr)
        {
            api.terminate_thread(thread, 3);
        }
        api.close_handle(thread);
        return true;
    }

    api.close_handle(thread);

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
    CopyThreadObservationToAttempt(context, attempt);
    attempt->thread_exit_code = exit_code;
    attempt->hle_console_output.assign(
        context.hle_console_output,
        context.hle_console_output +
            context.hle_console_output_size);
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
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackTrapExecution(
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
        false,
        true,
        &dos_file_system,
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
        timeout_milliseconds,
        attempt);
}

}  // namespace repiu::platform::win32
