#include "repiu/platform/win32/win32_thread_api.h"
#include "repiu/platform/host_thread.h"

#include "../host_thread_platform.h"

// Task 758. The Win32 half of the host thread API: CreateThread, and the
// interrupt as SuspendThread/GetThreadContext.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <new>

namespace repiu::platform
{
namespace
{

// Task 503d-18. The entry the engine writes is `std::uint32_t(void*)`, and
// neither host starts a thread with that signature, so each backend carries a
// record with the real entry and its argument and a trampoline of its own
// shape. The record is heap-allocated because it has to outlive the call that
// created the thread.
struct HostThreadRecord
{
    HostThreadEntry entry = nullptr;
    void* parameter = nullptr;
};

DWORD WINAPI HostThreadTrampoline(void* parameter)
{
    auto* record = static_cast<HostThreadRecord*>(parameter);
    const std::uint32_t result = record->entry(record->parameter);
    // Freed here rather than in CloseHostThread, because on this host nothing
    // reads the record after the entry returns: the exit code lives in the
    // thread object the handle names. The POSIX record cannot do this -- a
    // caller still reads the completion flag out of it.
    delete record;
    return static_cast<DWORD>(result);
}

}  // namespace

std::uint32_t CurrentThreadId()
{
    return static_cast<std::uint32_t>(GetCurrentThreadId());
}

namespace host_thread_platform
{

bool StartThread(HostThreadEntry entry,
                 void* parameter,
                 HostThread* thread,
                 std::uint32_t* host_error)
{
    auto* record = new (std::nothrow) HostThreadRecord{};
    if (record == nullptr)
    {
        return false;
    }
    record->entry = entry;
    record->parameter = parameter;

    DWORD thread_id = 0;
    HANDLE handle = CreateThread(nullptr, 0, HostThreadTrampoline, record, 0,
                                 &thread_id);
    if (handle == nullptr)
    {
        if (host_error != nullptr)
        {
            *host_error = static_cast<std::uint32_t>(GetLastError());
        }
        delete record;
        return false;
    }
    // What identifies the thread from here on is the handle; the record is the
    // trampoline's own and it frees it.
    thread->handle = handle;
    thread->id = static_cast<std::uint32_t>(thread_id);
    thread->valid = true;
    return true;
}

HostThreadStatus QueryThread(const HostThread& thread)
{
    HostThreadStatus status;
    // Not GetExitCodeThread alone. It reports 259 for a running thread, and 259
    // is a legal exit code, so the wait is what separates the two questions.
    auto handle = static_cast<HANDLE>(thread.handle);
    status.running = WaitForSingleObject(handle, 0) != WAIT_OBJECT_0;
    DWORD exit_code = 0;
    if (!status.running && GetExitCodeThread(handle, &exit_code))
    {
        status.exit_code = static_cast<std::uint32_t>(exit_code);
    }
    return status;
}

bool JoinThread(const HostThread& thread,
                const std::uint32_t timeout_milliseconds,
                std::uint32_t* exit_code)
{
    auto handle = static_cast<HANDLE>(thread.handle);
    if (WaitForSingleObject(handle, static_cast<DWORD>(
                                        timeout_milliseconds)) != WAIT_OBJECT_0)
    {
        return false;
    }
    DWORD code = 0;
    if (exit_code != nullptr && GetExitCodeThread(handle, &code))
    {
        *exit_code = static_cast<std::uint32_t>(code);
    }
    return true;
}

ThreadInterruptFailure InterruptThread(const HostThread& thread,
                                       ThreadInterruptCallback callback,
                                       ThreadInterruptContextCallback context_callback,
                                       void* user_data,
                                       const std::uint32_t timeout_milliseconds)
{
    auto handle = static_cast<HANDLE>(thread.handle);
    if (SuspendThread(handle) == static_cast<DWORD>(-1))
    {
        return ThreadInterruptFailure::kNotDelivered;
    }
    // The timeout has nothing to wait for on this host: SuspendThread has
    // already stopped the target by the time it returns, so the sample is
    // bounded by the callback itself.
    (void)timeout_milliseconds;

    GuestCpuContext registers = {};
    registers.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
    bool sampled = false;
    if (GetThreadContext(handle, &registers))
    {
        bool write_back = false;
        if (context_callback != nullptr)
        {
            write_back = context_callback(&registers, user_data, &registers);
        }
        else
        {
            write_back = callback(&registers, user_data);
        }
        sampled = !write_back || SetThreadContext(handle, &registers) != 0;
    }
    ResumeThread(handle);
    if (!sampled)
    {
        return ThreadInterruptFailure::kNotDelivered;
    }
    return ThreadInterruptFailure::kNone;
}

void CloseThread(HostThread* thread)
{
    CloseHandle(static_cast<HANDLE>(thread->handle));
}

void DetachThread(HostThread* thread)
{
    // The record is the trampoline's own here, and it frees it when the entry
    // returns -- which for a thread that never returns means it is not freed at
    // all. That is the same leak this function accepts on the other host, for
    // the same reason.
    CloseHandle(static_cast<HANDLE>(thread->handle));
}

}  // namespace host_thread_platform

bool TerminateHostThread(const HostThread& thread, std::uint32_t exit_code)
{
    const repiu::platform::win32::Win32ThreadApi& api =
        repiu::platform::win32::GetWin32ThreadApi();
    if (api.terminate_thread == nullptr)
    {
        return false;
    }
    auto* thread_handle = static_cast<HANDLE>(thread.handle);
    api.terminate_thread(thread_handle, static_cast<DWORD>(exit_code));
    return true;
}

}  // namespace repiu::platform
