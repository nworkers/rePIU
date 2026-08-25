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
#include <csignal>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
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

    // Task 503d-20. The interrupt's handshake, as one state rather than a pair
    // of flags.
    //
    // Task 503d-22: two flags were wrong, and wrong in a way that corrupted
    // memory. A signal already on its way cannot be cancelled, so a requester
    // that timed out and returned would leave the handler to run afterwards --
    // against a `user_data` that had been a local of the caller's frame.
    // Checking a flag and then clearing it cannot fix that: the handler can
    // pass the check an instant before the clear.
    //
    // The states below are claimed with a compare-exchange, which makes the
    // question "did the handler start" answerable exactly once. A requester
    // that loses the race has to wait for `kDone`, because by then the handler
    // is already touching its memory.
    enum class InterruptState : std::uint8_t
    {
        kIdle,
        kRequested,
        kRunning,
        kDone,
        kAbandoned,
    };
    std::atomic<InterruptState> interrupt_state{InterruptState::kIdle};
    ThreadInterruptCallback interrupt_callback = nullptr;
    void* interrupt_user_data = nullptr;
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

#if !defined(_WIN32)

// Task 503d-20. A real-time signal, because 3c already owns SIGSEGV, SIGBUS,
// SIGTRAP, SIGILL and SIGFPE, and a fault handler that fired for this would
// classify it as a guest fault.
//
// SIGRTMIN rather than SIGUSR1: the two SIGUSR signals belong to whoever
// embeds this, and a real-time signal is the one range a library can take
// without arguing about it.
int InterruptSignal()
{
    return SIGRTMIN;
}

// The record whose sample is being taken. One at a time: the callers are a
// watchdog and a diagnostic, neither of which runs concurrently with itself,
// and a per-thread registry would be state to keep correct for no gain.
std::atomic<HostThreadRecord*> g_interrupt_target{nullptr};

void InterruptSignalHandler(int, siginfo_t*, void* host_context)
{
    HostThreadRecord* record =
        g_interrupt_target.load(std::memory_order_acquire);
    if (record == nullptr)
    {
        return;
    }
    // Claiming the request is what makes the callback's `user_data` safe to
    // touch: whoever loses this exchange does not run the callback, and the
    // requester that loses it waits instead of returning.
    auto expected = HostThreadRecord::InterruptState::kRequested;
    if (!record->interrupt_state.compare_exchange_strong(
            expected, HostThreadRecord::InterruptState::kRunning,
            std::memory_order_acq_rel))
    {
        // A stray or late delivery. Ignoring it is right: the request it
        // belonged to has been answered or abandoned.
        return;
    }

    GuestCpuContext registers;
    if (LoadGuestCpuContext(host_context, &registers) &&
        record->interrupt_callback != nullptr)
    {
        record->interrupt_callback(&registers, record->interrupt_user_data);
        // Written back unconditionally. The callback may have changed nothing,
        // and storing an unchanged context costs less than asking it.
        StoreGuestCpuContext(registers, host_context);
    }
    record->interrupt_state.store(HostThreadRecord::InterruptState::kDone,
                                  std::memory_order_release);
}

// Installed once, on first use rather than at startup: a process that never
// interrupts a thread should not have a handler for this signal at all, and
// nothing here can install it before the callers exist.
//
// SA_ONSTACK matters for the guest thread, which runs on the guest's stack with
// a sigaltstack that 3c installed. Without it the handler frame lands on the
// guest's own stack, which is the stack this is usually called to inspect. A
// thread with no alternate stack simply ignores the flag.
bool EnsureInterruptHandler()
{
    static const bool installed = []() {
        struct sigaction action = {};
        action.sa_sigaction = &InterruptSignalHandler;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
        sigemptyset(&action.sa_mask);
        return sigaction(InterruptSignal(), &action, nullptr) == 0;
    }();
    return installed;
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

bool InterruptHostThread(const HostThread& thread,
                         ThreadInterruptCallback callback,
                         void* user_data,
                         const std::uint32_t timeout_milliseconds,
                         ThreadInterruptFailure* failure)
{
    const auto fail = [failure](const ThreadInterruptFailure reason) {
        if (failure != nullptr)
        {
            *failure = reason;
        }
        return false;
    };
    if (failure != nullptr)
    {
        *failure = ThreadInterruptFailure::kNone;
    }
    if (!thread.valid || thread.handle == nullptr || callback == nullptr)
    {
        return fail(ThreadInterruptFailure::kRefused);
    }
#if defined(_WIN32)
    auto handle = static_cast<HANDLE>(thread.handle);
    if (SuspendThread(handle) == static_cast<DWORD>(-1))
    {
        return fail(ThreadInterruptFailure::kNotDelivered);
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
        callback(&registers, user_data);
        sampled = SetThreadContext(handle, &registers) != 0;
    }
    ResumeThread(handle);
    if (!sampled)
    {
        return fail(ThreadInterruptFailure::kNotDelivered);
    }
    return true;
#else
    auto* record = static_cast<HostThreadRecord*>(thread.handle);
    if (record->finished.load(std::memory_order_acquire))
    {
        // Signalling a thread that has exited is how a caller learns it has,
        // not something to attempt: pthread_kill on a stale pthread_t is
        // undefined rather than an error.
        return fail(ThreadInterruptFailure::kNotDelivered);
    }

    if (!EnsureInterruptHandler())
    {
        return fail(ThreadInterruptFailure::kRefused);
    }

    HostThreadRecord* expected = nullptr;
    if (!g_interrupt_target.compare_exchange_strong(expected, record,
                                                    std::memory_order_acq_rel))
    {
        // One at a time, as the handler's comment says.
        return fail(ThreadInterruptFailure::kRefused);
    }

    record->interrupt_callback = callback;
    record->interrupt_user_data = user_data;
    record->interrupt_state.store(HostThreadRecord::InterruptState::kRequested,
                                  std::memory_order_release);

    const auto past = [](const timespec& deadline) {
        timespec now{};
        clock_gettime(CLOCK_REALTIME, &now);
        return now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec);
    };

    bool answered = false;
    bool delivered = false;
    if (pthread_kill(record->thread, InterruptSignal()) == 0)
    {
        delivered = true;
        const timespec deadline = DeadlineFromNow(timeout_milliseconds);
        while (record->interrupt_state.load(std::memory_order_acquire) !=
                   HostThreadRecord::InterruptState::kDone &&
               !past(deadline))
        {
            // Yielding rather than sleeping: the target is usually running and
            // the answer arrives in microseconds, and a caller investigating a
            // stall should not add a millisecond of its own to every sample.
            sched_yield();
        }

        // Task 503d-22. Giving up is a claim, not a decision. If the handler
        // has not started, abandoning the request stops it from ever running
        // against a `user_data` this call is about to return past. If it has
        // started, there is nothing to abandon and the only safe move is to
        // wait for it: the callback is holding the caller's memory.
        auto expected = HostThreadRecord::InterruptState::kRequested;
        if (record->interrupt_state.compare_exchange_strong(
                expected, HostThreadRecord::InterruptState::kAbandoned,
                std::memory_order_acq_rel))
        {
            answered = false;
        }
        else
        {
            while (record->interrupt_state.load(std::memory_order_acquire) !=
                   HostThreadRecord::InterruptState::kDone)
            {
                sched_yield();
            }
            answered = true;
        }
    }

    record->interrupt_state.store(HostThreadRecord::InterruptState::kIdle,
                                  std::memory_order_release);
    g_interrupt_target.store(nullptr, std::memory_order_release);
    if (!answered)
    {
        return fail(delivered ? ThreadInterruptFailure::kTimedOut
                              : ThreadInterruptFailure::kNotDelivered);
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
