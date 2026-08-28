#pragma once

// Win32 kernel32 thread-API function-pointer table, resolved once at runtime.
// Extracted from execution_trampoline.cpp (Phase 1 increment 4) so the live
// telemetry TU can share it. GetWin32ThreadApi is inline (single shared cache).
//
// Task 503d-14. Fenced whole, because the file is a kernel32 table by
// construction -- names resolved out of a DLL that exists on one host. Three of
// its members are what the engine needs to run: create the translation worker,
// create the guest thread, and close the handles afterwards. Those want a
// neutral counterpart, and its shape has to come from the four call sites in
// execution_trampoline.cpp rather than from this table.
//
// TerminateThread is the one with no POSIX counterpart. It is the execution
// watchdog's last resort, used when the guest thread will not stop gracefully,
// and pthread_cancel is not the same thing: it acts at cancellation points,
// where TerminateThread does not ask. What Linux does there is a decision the
// next sub-stage has to make, not one this fence settles.
#if defined(_WIN32)

#include <windows.h>

namespace repiu::platform::win32
{

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

inline const Win32ThreadApi& GetWin32ThreadApi()
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

} // namespace repiu::platform::win32

#endif  // defined(_WIN32)
