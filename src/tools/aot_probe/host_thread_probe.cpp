#include "host_thread_probe.h"

#include "repiu/platform/host_thread.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace repiu::tools
{
namespace
{

using repiu::platform::CloseHostThread;
using repiu::platform::CreateHostThread;
using repiu::platform::HostThread;
using repiu::platform::HostThreadStatus;
using repiu::platform::JoinHostThread;
using repiu::platform::QueryHostThread;

constexpr std::uint32_t kExitMarker = 0x0000ABCDU;

// Task 503d-18. The value Windows reports for a thread that is still running,
// which is also a legal exit code. Both of the engine's call sites decided with
// `!= STILL_ACTIVE`, so a thread that ended with this would have looked like one
// that never stops. Nothing in the engine returns it today; this probe is what
// keeps the layer from letting it back in.
constexpr std::uint32_t kStillActive = 259U;

struct ThreadWitness
{
    std::atomic<bool> ran{false};
    std::atomic<bool> release{false};
    std::atomic<std::uint32_t> observed_id{0};
    void* observed_parameter = nullptr;
    std::uint32_t return_value = kExitMarker;
};

std::uint32_t WitnessEntry(void* parameter)
{
    auto* witness = static_cast<ThreadWitness*>(parameter);
    witness->observed_parameter = parameter;
    witness->observed_id.store(repiu::platform::CurrentThreadId(),
                               std::memory_order_relaxed);
    witness->ran.store(true, std::memory_order_release);
    // Spins rather than sleeps so the caller decides how long this thread
    // lives; a probe that depends on a timing window is a probe that fails on
    // someone else's machine.
    while (!witness->release.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    return witness->return_value;
}

bool WaitUntilRan(const ThreadWitness& witness)
{
    // Bounded so a broken layer fails rather than hangs. A stuck probe is worse
    // than a failed one -- 3c's work order says so, and it is still true.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!witness.ran.load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

// What the two engine call sites do: start a thread, ask whether it is still
// running without waiting, and read what it returned.
bool ProbeRunAndCollect()
{
    ThreadWitness witness;
    HostThread thread;
    std::uint32_t host_error = 0;
    bool ok = CreateHostThread(&WitnessEntry, &witness, &thread, &host_error);
    ok = ok && thread.valid && host_error == 0;
    if (!ok)
    {
        return false;
    }

    ok = ok && WaitUntilRan(witness);
    // The parameter arrived unchanged, and the thread is a different one.
    ok = ok && witness.observed_parameter == static_cast<void*>(&witness);
    const std::uint32_t observed_id =
        witness.observed_id.load(std::memory_order_relaxed);
    ok = ok && observed_id != 0U &&
        observed_id != repiu::platform::CurrentThreadId();

    // Still running, asked without waiting.
    const HostThreadStatus while_running = QueryHostThread(thread);
    ok = ok && while_running.running;

    // A bounded wait on a thread that is not going to finish must report that
    // rather than block or lie.
    std::uint32_t ignored = 0;
    ok = ok && !JoinHostThread(thread, 50U, &ignored);

    witness.release.store(true, std::memory_order_release);

    std::uint32_t exit_code = 0;
    ok = ok && JoinHostThread(thread, 5000U, &exit_code);
    ok = ok && exit_code == kExitMarker;

    const HostThreadStatus after = QueryHostThread(thread);
    ok = ok && !after.running && after.exit_code == kExitMarker;

    CloseHostThread(&thread);
    ok = ok && !thread.valid;
    return ok;
}

// The ambiguity this layer exists to remove: a thread whose exit code is the
// number Windows also uses to mean "still running".
bool ProbeStillActiveExitCode()
{
    ThreadWitness witness;
    witness.return_value = kStillActive;
    HostThread thread;
    bool ok = CreateHostThread(&WitnessEntry, &witness, &thread, nullptr);
    if (!ok)
    {
        return false;
    }
    ok = ok && WaitUntilRan(witness);
    witness.release.store(true, std::memory_order_release);

    std::uint32_t exit_code = 0;
    ok = ok && JoinHostThread(thread, 5000U, &exit_code);
    ok = ok && exit_code == kStillActive;

    // The whole point: exited, and saying so, with that code.
    const HostThreadStatus after = QueryHostThread(thread);
    ok = ok && !after.running && after.exit_code == kStillActive;

    CloseHostThread(&thread);
    return ok;
}

// Refusals. A layer that returns true for an impossible request hides the
// failure until something further along cannot explain itself.
bool ProbeRefusals()
{
    HostThread thread;
    std::uint32_t host_error = 0xFFFFFFFFU;
    bool ok = !CreateHostThread(nullptr, nullptr, &thread, &host_error);
    ok = ok && !thread.valid && host_error == 0U;
    ok = ok && !CreateHostThread(&WitnessEntry, nullptr, nullptr, nullptr);

    // Querying something never created answers "not running" rather than
    // reading through a null handle.
    HostThread empty;
    const HostThreadStatus status = QueryHostThread(empty);
    ok = ok && !status.running && status.exit_code == 0U;
    ok = ok && !JoinHostThread(empty, 0U, nullptr);

    // And releasing it twice is not a crash.
    CloseHostThread(&empty);
    CloseHostThread(&empty);
    return ok;
}

}  // namespace

bool RunHostThreadProbe()
{
    const bool run_ok = ProbeRunAndCollect();
    const bool still_active_ok = ProbeStillActiveExitCode();
    const bool refusals_ok = ProbeRefusals();
    const bool all = run_ok && still_active_ok && refusals_ok;
    std::cout << "host_thread_run=" << (run_ok ? "true" : "false")
              << "\nhost_thread_still_active_exit_code="
              << (still_active_ok ? "true" : "false")
              << "\nhost_thread_refusals=" << (refusals_ok ? "true" : "false")
              << "\nhost_thread_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
