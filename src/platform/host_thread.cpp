#include "repiu/platform/host_thread.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#endif

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
#if !defined(_WIN32)
    pthread_t thread{};
    // Written by the thread, read by a caller that must not block. Release on
    // the store and acquire on the load, so the exit code beside it is visible
    // to whoever sees the flag set.
    std::atomic<bool> finished{false};
    std::atomic<std::uint32_t> exit_code{0};
    std::atomic<bool> joined{false};
#endif
};

#if defined(_WIN32)

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

#else

void* HostThreadTrampoline(void* parameter)
{
    auto* record = static_cast<HostThreadRecord*>(parameter);
    const std::uint32_t result = record->entry(record->parameter);
    record->exit_code.store(result, std::memory_order_relaxed);
    record->finished.store(true, std::memory_order_release);
    return nullptr;
}

// `pthread_timedjoin_np` wants an absolute CLOCK_REALTIME deadline rather than
// a duration, which is the one thing about it that is easy to get wrong.
timespec DeadlineFromNow(const std::uint32_t milliseconds)
{
    timespec deadline{};
    clock_gettime(CLOCK_REALTIME, &deadline);
    constexpr long kNanosecondsPerSecond = 1000000000L;
    deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
    deadline.tv_nsec +=
        static_cast<long>(milliseconds % 1000U) * 1000000L;
    if (deadline.tv_nsec >= kNanosecondsPerSecond)
    {
        deadline.tv_nsec -= kNanosecondsPerSecond;
        ++deadline.tv_sec;
    }
    return deadline;
}

#endif

}  // namespace

std::uint32_t CurrentThreadId()
{
#if defined(_WIN32)
    return static_cast<std::uint32_t>(GetCurrentThreadId());
#elif defined(SYS_gettid)
    // Not pthread_self: that is a pointer-like handle private to the C library,
    // while this is the number the kernel, /proc, and a debugger all use.
    return static_cast<std::uint32_t>(::syscall(SYS_gettid));
#else
    return 0U;
#endif
}

bool CreateHostThread(HostThreadEntry entry,
                      void* parameter,
                      HostThread* thread,
                      std::uint32_t* host_error)
{
    if (host_error != nullptr)
    {
        *host_error = 0;
    }
    if (entry == nullptr || thread == nullptr)
    {
        return false;
    }
    *thread = HostThread{};

    auto* record = new (std::nothrow) HostThreadRecord{};
    if (record == nullptr)
    {
        return false;
    }
    record->entry = entry;
    record->parameter = parameter;

#if defined(_WIN32)
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
#else
    const int created = pthread_create(&record->thread, nullptr,
                                       HostThreadTrampoline, record);
    if (created != 0)
    {
        if (host_error != nullptr)
        {
            *host_error = static_cast<std::uint32_t>(created);
        }
        delete record;
        return false;
    }
    thread->handle = record;
    // POSIX names a thread only from inside it, so the identifier stays zero
    // here. Every caller that needs the number reads it from the thread's own
    // `CurrentThreadId()`, which is what the engine's thread procedure already
    // stores into its context.
    thread->id = 0;
    thread->valid = true;
#endif
    return true;
}

HostThreadStatus QueryHostThread(const HostThread& thread)
{
    HostThreadStatus status;
    if (!thread.valid || thread.handle == nullptr)
    {
        status.running = false;
        return status;
    }
#if defined(_WIN32)
    // Not GetExitCodeThread alone. It reports 259 for a running thread, and 259
    // is a legal exit code, so the wait is what separates the two questions.
    auto handle = static_cast<HANDLE>(thread.handle);
    status.running = WaitForSingleObject(handle, 0) != WAIT_OBJECT_0;
    DWORD exit_code = 0;
    if (!status.running && GetExitCodeThread(handle, &exit_code))
    {
        status.exit_code = static_cast<std::uint32_t>(exit_code);
    }
#else
    const auto* record = static_cast<const HostThreadRecord*>(thread.handle);
    status.running = !record->finished.load(std::memory_order_acquire);
    if (!status.running)
    {
        status.exit_code = record->exit_code.load(std::memory_order_relaxed);
    }
#endif
    return status;
}

bool JoinHostThread(const HostThread& thread,
                    const std::uint32_t timeout_milliseconds,
                    std::uint32_t* exit_code)
{
    if (!thread.valid || thread.handle == nullptr)
    {
        return false;
    }
#if defined(_WIN32)
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
#else
    auto* record = static_cast<HostThreadRecord*>(thread.handle);
    if (!record->joined.load(std::memory_order_acquire))
    {
        const timespec deadline = DeadlineFromNow(timeout_milliseconds);
        const int joined = pthread_timedjoin_np(record->thread, nullptr,
                                                &deadline);
        if (joined != 0)
        {
            return false;
        }
        record->joined.store(true, std::memory_order_release);
    }
    if (exit_code != nullptr)
    {
        *exit_code = record->exit_code.load(std::memory_order_acquire);
    }
    return true;
#endif
}

void CloseHostThread(HostThread* thread)
{
    if (thread == nullptr || !thread->valid || thread->handle == nullptr)
    {
        return;
    }
#if defined(_WIN32)
    CloseHandle(static_cast<HANDLE>(thread->handle));
#else
    auto* record = static_cast<HostThreadRecord*>(thread->handle);
    // A thread that was never joined still owns its stack, so it is joined here
    // even though the caller has established that it exited. That join returns
    // immediately.
    if (!record->joined.load(std::memory_order_acquire))
    {
        pthread_join(record->thread, nullptr);
        record->joined.store(true, std::memory_order_release);
    }
    delete record;
#endif
    *thread = HostThread{};
}

}  // namespace repiu::platform
