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

// Task 503d-20. What the interrupt has to answer, on both hosts.
//
// Three scenarios, and they are the ones the callers depend on: that a sample
// lands where the thread actually is, that an edit reaches it, and that a
// target which cannot answer is reported rather than waited on forever.
struct InterruptWitness
{
    std::atomic<bool> spinning{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint32_t> sampled_eip{0};
    std::atomic<std::uint32_t> callback_count{0};
    std::atomic<bool> edit_observed{false};
};

// The loop a sample has to land inside.
std::uint32_t InterruptSpinEntry(void* parameter)
{
    auto* witness = static_cast<InterruptWitness*>(parameter);
    witness->spinning.store(true, std::memory_order_release);
    while (!witness->stop.load(std::memory_order_acquire))
    {
        // Nothing that enters the kernel: a sample taken while the thread sat
        // in a syscall would report the syscall's address, not this loop's.
        // The counter is read through a volatile pointer rather than declared
        // volatile, which C++20 deprecates for increments.
        int burn = 0;
        volatile int* const observed = &burn;
        while (*observed < 1000)
        {
            burn = *observed + 1;
        }
    }
    return kExitMarker;
}

void SampleEip(repiu::platform::GuestCpuContext* registers, void* user_data)
{
    auto* witness = static_cast<InterruptWitness*>(user_data);
    witness->callback_count.fetch_add(1, std::memory_order_relaxed);
    witness->sampled_eip.store(static_cast<std::uint32_t>(registers->Eip),
                               std::memory_order_release);
}

// The edit, which is also how the loop is stopped. Windows runs this on the
// caller's thread with the target frozen and Linux on the target itself, so the
// store below is what both have to make visible.
void RequestStopThroughRegisters(repiu::platform::GuestCpuContext* registers,
                                 void* user_data)
{
    auto* witness = static_cast<InterruptWitness*>(user_data);
    witness->sampled_eip.store(static_cast<std::uint32_t>(registers->Eip),
                               std::memory_order_release);
    witness->edit_observed.store(true, std::memory_order_release);
    witness->stop.store(true, std::memory_order_release);
}

bool WaitUntilSpinning(const InterruptWitness& witness)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!witness.spinning.load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

bool ProbeInterruptSamplesAndEdits()
{
    InterruptWitness witness;
    HostThread thread;
    if (!CreateHostThread(&InterruptSpinEntry, &witness, &thread, nullptr))
    {
        return false;
    }
    bool ok = WaitUntilSpinning(witness);

    ok = ok && repiu::platform::InterruptHostThread(thread, &SampleEip,
                                                    &witness, 2000U);
    const std::uint32_t sampled =
        witness.sampled_eip.load(std::memory_order_acquire);
    ok = ok && witness.callback_count.load(std::memory_order_relaxed) == 1U;
    // A zero or unreadable address is what a sample that came from nowhere
    // looks like, and it is the cheapest claim that still means "where the
    // thread is".
    ok = ok && sampled != 0U;

    // Sampling repeatedly must not report one constant. A fixed value would
    // mean the number came from somewhere other than the running thread.
    bool moved = false;
    for (int attempt = 0; attempt < 32 && !moved && ok; ++attempt)
    {
        if (!repiu::platform::InterruptHostThread(thread, &SampleEip, &witness,
                                                  2000U))
        {
            ok = false;
            break;
        }
        moved = witness.sampled_eip.load(std::memory_order_acquire) != sampled;
    }
    ok = ok && moved;

    ok = ok && repiu::platform::InterruptHostThread(
                   thread, &RequestStopThroughRegisters, &witness, 2000U);
    ok = ok && witness.edit_observed.load(std::memory_order_acquire);

    std::uint32_t exit_code = 0;
    witness.stop.store(true, std::memory_order_release);
    ok = ok && JoinHostThread(thread, 5000U, &exit_code);
    ok = ok && exit_code == kExitMarker;
    CloseHostThread(&thread);
    return ok;
}

// A target that cannot answer has to be reported inside the deadline. This is
// what keeps an investigation into a stall from stalling with it.
bool ProbeInterruptRefusals()
{
    HostThread empty;
    bool ok = !repiu::platform::InterruptHostThread(empty, &SampleEip, nullptr,
                                                    100U);
    ok = ok && !repiu::platform::InterruptHostThread(empty, nullptr, nullptr,
                                                     100U);

    // A thread that has exited. The record is still there, and interrupting it
    // must answer false rather than signal a thread that no longer exists.
    ThreadWitness finished;
    HostThread thread;
    if (!CreateHostThread(&WitnessEntry, &finished, &thread, nullptr))
    {
        return false;
    }
    ok = ok && WaitUntilRan(finished);
    finished.release.store(true, std::memory_order_release);
    ok = ok && JoinHostThread(thread, 5000U, nullptr);
    ok = ok && !repiu::platform::InterruptHostThread(thread, &SampleEip,
                                                     nullptr, 100U);
    CloseHostThread(&thread);
    return ok;
}

}  // namespace

bool RunHostThreadProbe()
{
    const bool run_ok = ProbeRunAndCollect();
    const bool still_active_ok = ProbeStillActiveExitCode();
    const bool refusals_ok = ProbeRefusals();
    const bool interrupt_ok = ProbeInterruptSamplesAndEdits();
    const bool interrupt_refusal_ok = ProbeInterruptRefusals();
    const bool all = run_ok && still_active_ok && refusals_ok &&
        interrupt_ok && interrupt_refusal_ok;
    std::cout << "host_thread_run=" << (run_ok ? "true" : "false")
              << "\nhost_thread_still_active_exit_code="
              << (still_active_ok ? "true" : "false")
              << "\nhost_thread_refusals=" << (refusals_ok ? "true" : "false")
              << "\nhost_thread_interrupt="
              << (interrupt_ok ? "true" : "false")
              << "\nhost_thread_interrupt_refusals="
              << (interrupt_refusal_ok ? "true" : "false")
              << "\nhost_thread_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
