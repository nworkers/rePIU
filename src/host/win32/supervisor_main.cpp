#include "repiu/platform/win32/live_telemetry.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

long ReadInterlocked(volatile long* value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

void PrintPage(const char* label, HANDLE process, std::uint32_t address)
{
    MEMORY_BASIC_INFORMATION memory = {};
    const SIZE_T queried = VirtualQueryEx(
        process,
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
        &memory,
        sizeof(memory));
    std::cout << "[repiu-debug] " << label << "=0x" << std::hex
              << address << " page_base=0x"
              << reinterpret_cast<std::uintptr_t>(memory.BaseAddress)
              << " page_size=0x" << memory.RegionSize
              << " state=0x" << memory.State
              << " protect=0x" << memory.Protect
              << " type=0x" << memory.Type
              << " queried=" << std::dec << queried << "\n";
}

void PrintGuardPageEvent(HANDLE process,
                         const DEBUG_EVENT& event,
                         const EXCEPTION_DEBUG_INFO& exception)
{
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                               FALSE,
                               event.dwThreadId);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    const BOOL context_read =
        thread != nullptr && GetThreadContext(thread, &context);
    const EXCEPTION_RECORD& record = exception.ExceptionRecord;
    const std::uint32_t access_type = record.NumberParameters >= 1
        ? static_cast<std::uint32_t>(record.ExceptionInformation[0])
        : UINT32_MAX;
    const std::uint32_t fault_address = record.NumberParameters >= 2
        ? static_cast<std::uint32_t>(record.ExceptionInformation[1])
        : 0U;
    const std::uint32_t eip = context_read
        ? static_cast<std::uint32_t>(context.Eip)
        : static_cast<std::uint32_t>(
              reinterpret_cast<std::uintptr_t>(record.ExceptionAddress));
    std::uint8_t bytes[16] = {};
    SIZE_T bytes_read = 0;
    ReadProcessMemory(process,
                      reinterpret_cast<const void*>(
                          static_cast<std::uintptr_t>(eip)),
                      bytes,
                      sizeof(bytes),
                      &bytes_read);
    std::cout << "[repiu-debug] guard first_chance="
              << exception.dwFirstChance
              << " pid=" << event.dwProcessId
              << " tid=" << event.dwThreadId
              << " access_type=0x" << std::hex << access_type
              << " fault=0x" << fault_address
              << " context=" << (context_read ? "true" : "false")
              << " eip=0x" << eip
              << " eax/ebx/ecx/edx=0x" << context.Eax << "/0x"
              << context.Ebx << "/0x" << context.Ecx << "/0x"
              << context.Edx
              << " esi/edi/esp/ebp=0x" << context.Esi << "/0x"
              << context.Edi << "/0x" << context.Esp << "/0x"
              << context.Ebp
              << " eflags=0x" << context.EFlags
              << " dr0/dr6/dr7=0x" << context.Dr0 << "/0x"
              << context.Dr6 << "/0x" << context.Dr7
              << " bytes=";
    for (SIZE_T index = 0; index < bytes_read; ++index)
    {
        std::cout << (index == 0 ? "" : " ")
                  << std::hex << static_cast<unsigned>(bytes[index]);
    }
    std::cout << std::dec << "\n";
    PrintPage("eip", process, eip);
    PrintPage("fault", process, fault_address);
    if (context_read)
    {
        PrintPage("esp", process, static_cast<std::uint32_t>(context.Esp));
    }
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
    std::cout.flush();
}

void PrintSnapshot(
    repiu::platform::win32::Win32SharedLiveTelemetry& telemetry,
    std::uint32_t elapsed_milliseconds,
    HANDLE child_process)
{
    const std::uint32_t guest_eip = static_cast<std::uint32_t>(
        ReadInterlocked(&telemetry.last_guest_eip));
    std::cout << "[repiu-supervisor] elapsed_ms=" << elapsed_milliseconds
              << " phase="
              << ReadInterlocked(&telemetry.host_phase)
              << " heartbeat="
              << ReadInterlocked(&telemetry.heartbeat)
              << " dispatch_entry="
              << ReadInterlocked(&telemetry.dispatch_entry_count)
              << " dispatch_exit="
              << ReadInterlocked(&telemetry.dispatch_exit_count)
              << " last_eip=0x" << std::hex
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_eip))
              << " last_guest_eip=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_eip))
              << " exception=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_exception_code))
              << " guest_eax=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_eax))
              << " guest_ebx/ecx/edx=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_ebx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_ecx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_edx))
              << " guest_esi/edi=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_esi)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_edi))
              << " guest_esp=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_esp))
              << " recovery_fs/ds/es/gs=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_fs)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_ds)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_es)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_gs))
              << " dpmi_frame_eax/ebx/ecx=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.dpmi_frame_eax)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.dpmi_frame_ebx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.dpmi_frame_ecx))
              << " handler_phase="
              << ReadInterlocked(&telemetry.guest_handler_phase)
              << " glide_ordinal="
              << ReadInterlocked(&telemetry.glide_gate_ordinal)
              << " glide_esp=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_esp))
              << " glide_ebx/ecx/edx=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_ebx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_ecx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_edx))
              << " glide_stack="
              << ReadInterlocked(&telemetry.glide_gate_stack[0]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[1]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[2]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[3]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[4]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[5]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[6]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[7])
              << " mscdex_probe/request/cmd/status="
              << ReadInterlocked(&telemetry.mscdex_probe_count) << "/"
              << ReadInterlocked(&telemetry.mscdex_request_count) << "/"
              << std::hex
              << ReadInterlocked(&telemetry.mscdex_last_command) << "/"
              << ReadInterlocked(&telemetry.mscdex_last_status)
              << std::dec
              << " aot_boundary/reentry="
              << ReadInterlocked(&telemetry.aot_boundary_count) << "/"
              << ReadInterlocked(&telemetry.aot_reentry_count)
              << " sample_count/unmapped="
              << ReadInterlocked(&telemetry.native_sample_count) << "/"
              << ReadInterlocked(&telemetry.native_sample_unmapped_count)
              << " sample_stage="
              << ReadInterlocked(&telemetry.native_sample_stage)
              << " sample_eip=0x" << std::hex
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_eip))
              << " sample_guest=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_guest_eip))
              << " sample_eax/ecx/esi/edi=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_eax)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_ecx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_esi)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_edi))
              << " sample_esp=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.native_sample_esp))
              << " sample_indirect=0x"
              << static_cast<std::uint32_t>(ReadInterlocked(
                     &telemetry.native_sample_indirect_source))
              << "->0x"
              << static_cast<std::uint32_t>(ReadInterlocked(
                     &telemetry.native_sample_indirect_target))
              << " sample_ring=";
    for (std::uint32_t index = 0;
         index < repiu::platform::win32::kWin32NativeSampleRingCapacity;
         ++index)
    {
        std::cout << (index == 0 ? "0x" : ",0x") << std::hex
                  << static_cast<std::uint32_t>(ReadInterlocked(
                         &telemetry.native_sample_ring[index]));
    }
    std::cout << " sample_ring_mapped=0x" << std::hex
              << static_cast<std::uint32_t>(ReadInterlocked(
                     &telemetry.native_sample_ring_mapped_bits))
              << std::dec << " sample_ring_cursor="
              << ReadInterlocked(&telemetry.native_sample_ring_cursor)
              << "\n";
    constexpr std::uint32_t kLongRuntimeBoundary = 0x030873F4U;
    if (child_process != nullptr && guest_eip == kLongRuntimeBoundary)
    {
        const std::uint32_t esi = static_cast<std::uint32_t>(
            ReadInterlocked(&telemetry.last_guest_esi));
        const std::uint32_t source = esi + 0x34U;
        MEMORY_BASIC_INFORMATION memory = {};
        const SIZE_T queried = VirtualQueryEx(
            child_process,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(source)),
            &memory,
            sizeof(memory));
        std::uint32_t value = 0;
        SIZE_T bytes_read = 0;
        const BOOL read = ReadProcessMemory(
            child_process,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(source)),
            &value,
            sizeof(value),
            &bytes_read);
        std::cout << "[repiu-supervisor] boundary_source=0x"
                  << std::hex << source
                  << " read=" << (read ? "true" : "false")
                  << " bytes=" << std::dec << bytes_read
                  << " value=0x" << std::hex << value
                  << " page_base=0x"
                  << reinterpret_cast<std::uintptr_t>(memory.BaseAddress)
                  << " page_size=0x" << memory.RegionSize
                  << " state=0x" << memory.State
                  << " protect=0x" << memory.Protect
                  << " type=0x" << memory.Type
                  << " queried=" << std::dec << queried << "\n";
    }
    std::cout.flush();
}

// Samples a child-process thread externally. This works even when the
// loader's in-process poll loop is stalled, because the supervisor shares
// no locks with the child. Output happens only after the thread resumes.
void SampleChildThreadContext(DWORD thread_id,
                              const char* label,
                              std::uint32_t elapsed_milliseconds,
                              std::uint32_t cache_base,
                              std::uint32_t cache_capacity)
{
    if (thread_id == 0)
    {
        return;
    }
    HANDLE thread = OpenThread(
        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
        FALSE,
        thread_id);
    if (thread == nullptr)
    {
        std::cout << "[repiu-supervisor-sample] label=" << label
                  << " tid=" << thread_id
                  << " open_failed=" << GetLastError() << "\n";
        return;
    }
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    bool context_read = false;
    DWORD context_error = 0;
    if (SuspendThread(thread) != static_cast<DWORD>(-1))
    {
        if (GetThreadContext(thread, &context))
        {
            context_read = true;
        }
        else
        {
            context_error = GetLastError();
        }
        ResumeThread(thread);
    }
    else
    {
        context_error = GetLastError();
    }
    CloseHandle(thread);
    if (!context_read)
    {
        std::cout << "[repiu-supervisor-sample] label=" << label
                  << " tid=" << thread_id
                  << " context_failed=" << context_error << "\n";
        return;
    }
    const std::uint32_t eip = static_cast<std::uint32_t>(context.Eip);
    const bool in_cache = cache_capacity != 0 && eip >= cache_base &&
        eip < cache_base + cache_capacity;
    std::cout << "[repiu-supervisor-sample] label=" << label
              << " tid=" << thread_id
              << " elapsed_ms=" << elapsed_milliseconds
              << std::hex
              << " eip=0x" << eip
              << " in_cache=" << (in_cache ? "true" : "false")
              << " eax/ebx/ecx/edx=0x" << context.Eax << "/0x"
              << context.Ebx << "/0x" << context.Ecx << "/0x" << context.Edx
              << " esi/edi/esp/ebp=0x" << context.Esi << "/0x"
              << context.Edi << "/0x" << context.Esp << "/0x" << context.Ebp
              << std::dec << "\n";
    std::cout.flush();
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string target = argc >= 2 ? argv[1] : "piu_1st";
    const std::uint32_t timeout_milliseconds =
        argc >= 3 ? static_cast<std::uint32_t>(std::stoul(argv[2]))
                  : 10000U;
    const bool debug_exceptions =
        argc >= 4 && std::strcmp(argv[3], "debug-exceptions") == 0;

    char module_path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, module_path, MAX_PATH) == 0)
    {
        return 1;
    }
    const std::filesystem::path loader_path =
        std::filesystem::path(module_path).parent_path() /
        "repiu_loader_win32.exe";

    const std::string mapping_name =
        "Local\\repiu-live-" + std::to_string(GetCurrentProcessId()) +
        "-" + std::to_string(GetTickCount());
    HANDLE mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(repiu::platform::win32::Win32SharedLiveTelemetry),
        mapping_name.c_str());
    if (mapping == nullptr)
    {
        std::cerr << "[repiu-supervisor] CreateFileMapping failed error="
                  << GetLastError() << "\n";
        return 2;
    }
    repiu::platform::win32::Win32SharedLiveTelemetry fallback_telemetry;
    auto* telemetry = static_cast<
        repiu::platform::win32::Win32SharedLiveTelemetry*>(
            MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (telemetry == nullptr)
    {
        std::cerr << "[repiu-supervisor] MapViewOfFile failed error="
                  << GetLastError() << "\n";
        if (!debug_exceptions)
        {
            CloseHandle(mapping);
            return 3;
        }
        CloseHandle(mapping);
        mapping = nullptr;
        telemetry = &fallback_telemetry;
    }
    *telemetry = repiu::platform::win32::Win32SharedLiveTelemetry{};

    if (mapping != nullptr)
    {
        SetEnvironmentVariableA(
            repiu::platform::win32::kWin32LiveTelemetryEnvironment,
            mapping_name.c_str());
    }
    const std::string child_timeout = "0";
    SetEnvironmentVariableA(
        repiu::platform::win32::kWin32ExecutionTimeoutEnvironment,
        child_timeout.c_str());
    std::string command =
        "\"" + loader_path.string() + "\" " + target;
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL created = CreateProcessA(
        nullptr,
        command.data(),
        nullptr,
        nullptr,
        FALSE,
        debug_exceptions ? DEBUG_ONLY_THIS_PROCESS : 0,
        nullptr,
        nullptr,
        &startup,
        &process);
    if (mapping != nullptr)
    {
        SetEnvironmentVariableA(
            repiu::platform::win32::kWin32LiveTelemetryEnvironment,
            nullptr);
    }
    SetEnvironmentVariableA(
        repiu::platform::win32::kWin32ExecutionTimeoutEnvironment,
        nullptr);
    if (!created)
    {
        if (mapping != nullptr)
        {
            UnmapViewOfFile(telemetry);
            CloseHandle(mapping);
        }
        return 4;
    }

    const DWORD start = GetTickCount();
    DWORD last_snapshot = start - 1000U;
    long stall_last_dispatch = -1;
    long stall_last_samples = -1;
    DWORD stall_start_tick = start;
    DWORD last_external_sample_tick = 0;
    bool terminated = false;
    bool debug_process_exited = false;
    bool debug_wait_error_reported = false;
    for (;;)
    {
        DWORD wait = WAIT_TIMEOUT;
        if (debug_exceptions)
        {
            DEBUG_EVENT event = {};
            if (WaitForDebugEvent(&event, 50U))
            {
                DWORD continue_status = DBG_CONTINUE;
                if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
                {
                    const EXCEPTION_DEBUG_INFO& exception =
                        event.u.Exception;
                    const DWORD code =
                        exception.ExceptionRecord.ExceptionCode;
                    if (code != EXCEPTION_SINGLE_STEP &&
                        code != EXCEPTION_ACCESS_VIOLATION &&
                        code != EXCEPTION_PRIV_INSTRUCTION)
                    {
                        std::cout << "[repiu-debug] exception code=0x"
                                  << std::hex << code
                                  << " first_chance=" << std::dec
                                  << exception.dwFirstChance
                                  << " tid=" << event.dwThreadId
                                  << " address=0x" << std::hex
                                  << reinterpret_cast<std::uintptr_t>(
                                         exception.ExceptionRecord.
                                             ExceptionAddress)
                                  << std::dec << "\n";
                    }
                    if (code == EXCEPTION_GUARD_PAGE)
                    {
                        PrintGuardPageEvent(process.hProcess,
                                            event,
                                            exception);
                    }
                    if (code == EXCEPTION_ACCESS_VIOLATION)
                    {
                        PrintGuardPageEvent(process.hProcess,
                                            event,
                                            exception);
                    }
                    if (code != EXCEPTION_BREAKPOINT)
                    {
                        continue_status = DBG_EXCEPTION_NOT_HANDLED;
                    }
                }
                else if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT &&
                         event.u.CreateProcessInfo.hFile != nullptr)
                {
                    CloseHandle(event.u.CreateProcessInfo.hFile);
                }
                else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT &&
                         event.u.LoadDll.hFile != nullptr)
                {
                    CloseHandle(event.u.LoadDll.hFile);
                }
                else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
                {
                    std::cout << "[repiu-debug] exit_process code=0x"
                              << std::hex << event.u.ExitProcess.dwExitCode
                              << std::dec << "\n";
                    debug_process_exited = true;
                }
                ContinueDebugEvent(event.dwProcessId,
                                   event.dwThreadId,
                                   continue_status);
            }
            else if (!debug_wait_error_reported &&
                     GetLastError() != ERROR_SEM_TIMEOUT)
            {
                std::cerr << "[repiu-debug] WaitForDebugEvent failed error="
                          << GetLastError() << "\n";
                debug_wait_error_reported = true;
            }
            wait = debug_process_exited
                ? WAIT_OBJECT_0
                : WaitForSingleObject(process.hProcess, 0U);
        }
        else
        {
            wait = WaitForSingleObject(process.hProcess, 50U);
        }
        const DWORD elapsed = GetTickCount() - start;
        if (GetTickCount() - last_snapshot >= 1000U)
        {
            PrintSnapshot(*telemetry, elapsed, process.hProcess);
            last_snapshot = GetTickCount();
        }
        // When both the dispatch stream and the in-process sampler stall
        // while the child is alive, capture child thread contexts
        // externally: this remains possible even if the loader's own poll
        // loop is frozen.
        const long stall_dispatch_now =
            ReadInterlocked(&telemetry->dispatch_entry_count);
        const long stall_samples_now =
            ReadInterlocked(&telemetry->native_sample_count);
        if (stall_dispatch_now != stall_last_dispatch ||
            stall_samples_now != stall_last_samples)
        {
            stall_last_dispatch = stall_dispatch_now;
            stall_last_samples = stall_samples_now;
            stall_start_tick = GetTickCount();
        }
        else if (wait == WAIT_TIMEOUT && stall_dispatch_now > 0 &&
                 GetTickCount() - stall_start_tick >= 3000U &&
                 GetTickCount() - last_external_sample_tick >= 2000U)
        {
            const std::uint32_t cache_base = static_cast<std::uint32_t>(
                ReadInterlocked(&telemetry->aot_cache_base));
            const std::uint32_t cache_capacity = static_cast<std::uint32_t>(
                ReadInterlocked(&telemetry->aot_cache_size));
            SampleChildThreadContext(
                static_cast<DWORD>(
                    ReadInterlocked(&telemetry->guest_thread_id)),
                "guest", elapsed, cache_base, cache_capacity);
            SampleChildThreadContext(
                static_cast<DWORD>(
                    ReadInterlocked(&telemetry->host_main_thread_id)),
                "host-main", elapsed, cache_base, cache_capacity);
            last_external_sample_tick = GetTickCount();
        }
        if (wait == WAIT_OBJECT_0)
        {
            break;
        }
        if (elapsed >= timeout_milliseconds)
        {
            TerminateProcess(process.hProcess, 124U);
            WaitForSingleObject(process.hProcess, 5000U);
            terminated = true;
            break;
        }
    }
    PrintSnapshot(*telemetry, GetTickCount() - start, process.hProcess);

    DWORD exit_code = 0;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (mapping != nullptr)
    {
        UnmapViewOfFile(telemetry);
        CloseHandle(mapping);
    }
    std::cout << "[repiu-supervisor] child_exit=" << exit_code
              << " terminated=" << (terminated ? "true" : "false")
              << "\n";
    return 0;
}
