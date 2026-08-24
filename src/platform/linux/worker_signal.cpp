#include "repiu/platform/worker_signal.h"

#if !defined(_WIN32)

#include <condition_variable>
#include <new>
#include <mutex>

namespace repiu::platform
{
namespace
{

// A condition variable with a flag, which is what an auto-reset event is once
// the name is taken away. `notify_one` wakes a single waiter and the waiter
// clears the flag, so a signal is consumed exactly once -- the property the
// handshake depends on.
//
// Not std::binary_semaphore: this has to answer Reset, and a semaphore offers
// no way to discard a permit without a waiter to consume it.
struct WorkerSignal
{
    std::mutex mutex;
    std::condition_variable condition;
    bool signalled = false;
};

}  // namespace

void* CreateWorkerSignal()
{
    return new (std::nothrow) WorkerSignal();
}

void DestroyWorkerSignal(void* signal)
{
    delete static_cast<WorkerSignal*>(signal);
}

bool SignalWorker(void* signal)
{
    if (signal == nullptr)
    {
        return false;
    }
    auto* state = static_cast<WorkerSignal*>(signal);
    {
        const std::lock_guard<std::mutex> guard(state->mutex);
        state->signalled = true;
    }
    state->condition.notify_one();
    return true;
}

bool WaitForWorkerSignal(void* signal)
{
    if (signal == nullptr)
    {
        return false;
    }
    auto* state = static_cast<WorkerSignal*>(signal);
    std::unique_lock<std::mutex> lock(state->mutex);
    // The predicate covers both a signal that arrived before the wait and a
    // spurious wake, either of which would otherwise desynchronise the
    // handshake.
    state->condition.wait(lock, [state] { return state->signalled; });
    state->signalled = false;
    return true;
}

void ResetWorkerSignal(void* signal)
{
    if (signal == nullptr)
    {
        return;
    }
    auto* state = static_cast<WorkerSignal*>(signal);
    const std::lock_guard<std::mutex> guard(state->mutex);
    state->signalled = false;
}

}  // namespace repiu::platform

#endif
