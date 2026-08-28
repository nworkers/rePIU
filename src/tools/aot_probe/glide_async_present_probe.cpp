#include "glide_async_present_probe.h"

#include "repiu/engine/glide_async_present.h"
#include "repiu/engine/glide_opengl_backend.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace repiu::tools
{
namespace
{

using engine::GlideOpenGlBackend;

// Records the order commands actually executed in. Ordering is the whole
// correctness argument of the asynchronous path, so it is asserted directly
// rather than inferred from counters.
struct OrderLog
{
    std::mutex mutex;
    std::vector<int> order;

    void Append(int value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        order.push_back(value);
    }

    std::vector<int> Snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return order;
    }
};

}  // namespace

bool RunGlideAsyncPresentProbe()
{
    using engine::kWin32GlideAsyncCommandCapacity;
    using engine::kWin32GlideMaxOutstandingSwaps;
    using engine::ResolveGlideAsyncPresentEnabled;

    // Opt-in until measured: unset and empty are OFF, unrecognised stays
    // fail-closed OFF.
    const bool policy =
        !ResolveGlideAsyncPresentEnabled(nullptr) &&
        !ResolveGlideAsyncPresentEnabled("") &&
        !ResolveGlideAsyncPresentEnabled("0") &&
        !ResolveGlideAsyncPresentEnabled("yes") &&
        ResolveGlideAsyncPresentEnabled("1") &&
        ResolveGlideAsyncPresentEnabled("on") &&
        ResolveGlideAsyncPresentEnabled("true");

    // One outstanding swap is double buffering; the queue bound catches
    // everything else. Both are what stop the guest running away from the host.
    const bool bounds =
        kWin32GlideMaxOutstandingSwaps == 1U &&
        kWin32GlideAsyncCommandCapacity >= 8U;

    auto backend = std::make_unique<GlideOpenGlBackend>();
    OrderLog log;
    std::atomic<bool> host_ready{false};
    std::atomic<bool> stop{false};

    // A real second thread, because the property under test is what happens
    // between two of them.
    std::thread host([&backend, &host_ready, &stop]() {
        backend->BindHostThread();
        host_ready.store(true, std::memory_order_release);
        while (!stop.load(std::memory_order_acquire))
        {
            backend->PumpHostCommands();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        backend->PumpHostCommands();
        // The host closes the channel before it goes away, which is what
        // teardown does. Closing from any other thread would rendezvous with a
        // thread that is no longer pumping.
        backend->Close();
    });
    while (!host_ready.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    for (int index = 1; index <= 3; ++index)
    {
        backend->PostToHostThread([&log, index]() { log.Append(index); },
                                  false);
    }
    // A synchronous command published after three posted ones must run after
    // all three: the pump drains the queue before it touches the sync slot.
    // `BufferSwap` is the shortest public path to that rendezvous; the window is
    // not open, so it fails on the host and returns false, which is irrelevant
    // to the ordering being asserted.
    backend->BufferSwap(0U);
    log.Append(4);

    const std::vector<int> order = log.Snapshot();
    const bool ordering =
        order.size() == 4U && order[0] == 1 && order[1] == 2 &&
        order[2] == 3 && order[3] == 4;

    const auto after_posts = backend->glide_async_present();
    const bool accounting =
        after_posts.enabled && after_posts.posted_count == 3U &&
        after_posts.posted_swap_count == 0U &&
        after_posts.executed_count == 3U &&
        after_posts.refused_count == 0U &&
        after_posts.max_queue_depth >= 1U &&
        backend->glide_pending_swap_count() == 0U;

    stop.store(true, std::memory_order_release);
    host.join();

    // The host has closed the channel and gone. A post now must refuse
    // immediately rather than wait for a pump that will never come -- that
    // refusal is what keeps teardown from deadlocking.
    const bool refused_after_close =
        !backend->PostToHostThread([&log]() { log.Append(99); }, false) &&
        backend->glide_async_present().refused_count == 1U &&
        log.Snapshot().size() == 4U;
    // Hand ownership back to this thread so the destructor's Close runs inline
    // instead of trying to reach the thread that has exited.
    backend->BindHostThread();

    const bool all =
        policy && bounds && ordering && accounting && refused_after_close;

    std::cout << "glide_async_present_policy=" << (policy ? "true" : "false")
              << "\nglide_async_present_bounds=" << (bounds ? "true" : "false")
              << "\nglide_async_present_ordering="
              << (ordering ? "true" : "false")
              << "\nglide_async_present_accounting="
              << (accounting ? "true" : "false")
              << "\nglide_async_present_refused_after_close="
              << (refused_after_close ? "true" : "false")
              << "\nglide_async_present_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
