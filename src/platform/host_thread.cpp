#include "repiu/platform/host_thread.h"

#include "host_thread_platform.h"

// Task 758. The host thread API's argument checks, shared by every host. The
// thread itself is started, sampled and released by host_thread_platform.h,
// defined in win32/ and linux/.

namespace repiu::platform
{

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

    return host_thread_platform::StartThread(entry, parameter, thread,
                                             host_error);
}

HostThreadStatus QueryHostThread(const HostThread& thread)
{
    HostThreadStatus status;
    if (!thread.valid || thread.handle == nullptr)
    {
        status.running = false;
        return status;
    }
    return host_thread_platform::QueryThread(thread);
}

bool JoinHostThread(const HostThread& thread,
                    const std::uint32_t timeout_milliseconds,
                    std::uint32_t* exit_code)
{
    if (!thread.valid || thread.handle == nullptr)
    {
        return false;
    }
    return host_thread_platform::JoinThread(thread, timeout_milliseconds,
                                            exit_code);
}

bool InterruptHostThreadImpl(const HostThread& thread,
                             ThreadInterruptCallback callback,
                             ThreadInterruptContextCallback context_callback,
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
    if (!thread.valid || thread.handle == nullptr ||
        (callback == nullptr && context_callback == nullptr))
    {
        return fail(ThreadInterruptFailure::kRefused);
    }
    const ThreadInterruptFailure reason =
        host_thread_platform::InterruptThread(thread, callback, context_callback,
                                              user_data, timeout_milliseconds);
    if (reason != ThreadInterruptFailure::kNone)
    {
        return fail(reason);
    }
    return true;
}

bool InterruptHostThread(const HostThread& thread,
                         ThreadInterruptCallback callback,
                         void* user_data,
                         const std::uint32_t timeout_milliseconds,
                         ThreadInterruptFailure* failure)
{
    return InterruptHostThreadImpl(thread, callback, nullptr, user_data,
                                   timeout_milliseconds, failure);
}

bool InterruptHostThreadWithContext(
    const HostThread& thread,
    ThreadInterruptContextCallback callback,
    void* user_data,
    const std::uint32_t timeout_milliseconds,
    ThreadInterruptFailure* failure)
{
    return InterruptHostThreadImpl(thread, nullptr, callback, user_data,
                                   timeout_milliseconds, failure);
}

void CloseHostThread(HostThread* thread)
{
    if (thread == nullptr || !thread->valid || thread->handle == nullptr)
    {
        return;
    }
    host_thread_platform::CloseThread(thread);
    *thread = HostThread{};
}

void DetachHostThread(HostThread* thread)
{
    if (thread == nullptr || !thread->valid || thread->handle == nullptr)
    {
        return;
    }
    host_thread_platform::DetachThread(thread);
    *thread = HostThread{};
}

}  // namespace repiu::platform
