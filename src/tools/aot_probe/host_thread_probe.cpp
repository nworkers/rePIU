#include "host_thread_probe.h"

#include "repiu/platform/host_thread.h"
#include "repiu/platform/host_time.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#if !defined(_WIN32)
#include <csignal>
#include <pthread.h>
#endif

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
    // Task 503d-22. Which thread the target is, and which one the callback
    // actually ran on. The header documents these as different on the two
    // hosts -- Windows freezes the target and runs the callback on the caller,
    // Linux runs it on the target inside a signal handler -- and that is the
    // documented difference this pins. It is also the only discriminator that
    // separates a sample of the intended thread from a sample of some other
    // thread that answered in its place.
    std::atomic<std::uint32_t> target_thread_id{0};
    std::atomic<std::uint32_t> callback_thread_id{0};
};

// The loop a sample has to land inside.
std::uint32_t InterruptSpinEntry(void* parameter)
{
    auto* witness = static_cast<InterruptWitness*>(parameter);
    witness->target_thread_id.store(repiu::platform::CurrentThreadId(),
                                    std::memory_order_relaxed);
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
    witness->callback_thread_id.store(repiu::platform::CurrentThreadId(),
                                      std::memory_order_relaxed);
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

    // Task 503d-22. And it ran on the thread the host says it should. The two
    // answers are different by design, so this is not one claim written twice:
    // on Linux the callback is a signal handler on the target, and a sample
    // arriving on any other thread would be some other thread's registers
    // reported under this one's name.
    const std::uint32_t ran_on =
        witness.callback_thread_id.load(std::memory_order_relaxed);
    const std::uint32_t target =
        witness.target_thread_id.load(std::memory_order_relaxed);
    ok = ok && ran_on != 0U && target != 0U;
#if defined(_WIN32)
    ok = ok && ran_on == repiu::platform::CurrentThreadId();
#else
    ok = ok && ran_on == target;
#endif

    // Sampling repeatedly must not report one constant. A fixed value would
    // mean the number came from somewhere other than the running thread.
    //
    // The target is given a moment between samples, and that is not padding:
    // the first version of this ran the samples back to back and failed four
    // times in twelve on Windows, because suspending a thread as fast as the
    // loop can ask leaves it almost no time to advance and it is caught at the
    // same instruction. What is under test is that the value tracks the thread,
    // which cannot be observed unless the thread is allowed to run.
    bool moved = false;
    for (int attempt = 0; attempt < 32 && !moved && ok; ++attempt)
    {
        repiu::platform::YieldMilliseconds(1U);
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

// Task 503d-22. What a timed-out interrupt has to leave behind.
//
// POSIX-only, because the thing under test is. On Windows `SuspendThread` has
// already stopped the target by the time it returns, so no request can outlive
// the call that made it and the timeout has nothing to wait for. Linux sends a
// signal, and a signal that has been sent cannot be recalled -- which is the
// whole subject here.
//
// Two flags used to answer this, and they were wrong in a way that corrupted
// memory: a requester that timed out returned while its signal was still in
// flight, and the handler ran afterwards against a `user_data` that had been a
// local of the frame it returned past. Checking a flag and then clearing it
// cannot fix that, because the handler can pass the check an instant before the
// clear.
//
// What is hard about probing this is that the obvious test does not
// discriminate. A late delivery arriving when nothing is outstanding is ignored
// by the old code too -- it finds the flag already cleared. The difference only
// shows when the late delivery lands while a later request is outstanding, and
// that is what part 3 below arranges, on purpose and with a margin rather than
// a race.
#if !defined(_WIN32)

struct BlockedInterruptWitness
{
    std::atomic<bool> ready{false};
    std::atomic<bool> unblock{false};
    std::atomic<bool> unblocked{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint32_t> callback_count{0};
    // How long the thread waits after being told to unblock, so a delivery can
    // be placed inside a window rather than raced against its edge. Read once
    // by the thread itself, before anything else writes it.
    std::uint32_t unblock_delay_milliseconds = 0;
};

void CountInterruptCallback(repiu::platform::GuestCpuContext*, void* user_data)
{
    auto* witness = static_cast<BlockedInterruptWitness*>(user_data);
    witness->callback_count.fetch_add(1, std::memory_order_relaxed);
}

// A thread that cannot answer, made the way the stalled guest was found: with
// the interrupt's signal blocked, so `pthread_kill` succeeds and the delivery
// waits. /proc showed that state as SigBlk and SigPnd carrying the same bit,
// and this reproduces it deliberately.
//
// Naming the signal here couples the probe to the layer's choice of SIGRTMIN.
// That coupling is the point: if the layer moves off it, this stops reproducing
// anything, and it should be moved with it rather than left passing.
std::uint32_t BlockedSignalEntry(void* parameter)
{
    auto* witness = static_cast<BlockedInterruptWitness*>(parameter);
    const std::uint32_t delay = witness->unblock_delay_milliseconds;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    witness->ready.store(true, std::memory_order_release);

    while (!witness->unblock.load(std::memory_order_acquire) &&
           !witness->stop.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    if (delay != 0U)
    {
        repiu::platform::YieldMilliseconds(delay);
    }
    // Unblocking is what delivers whatever is pending, and it happens here
    // rather than at exit so the probe can watch what arrives while the thread
    // is still alive to run it.
    pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
    witness->unblocked.store(true, std::memory_order_release);

    while (!witness->stop.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    return kExitMarker;
}

bool WaitUntilSet(const std::atomic<bool>& flag)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!flag.load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

// Stops a blocked thread and collects it. Written once because the probe below
// has two of them to put down and an early failure must not leak either.
bool ReleaseBlockedThread(BlockedInterruptWitness& witness, HostThread& thread)
{
    if (!thread.valid)
    {
        return true;
    }
    witness.stop.store(true, std::memory_order_release);
    std::uint32_t exit_code = 0;
    const bool joined = JoinHostThread(thread, 5000U, &exit_code);
    CloseHostThread(&thread);
    return joined && exit_code == kExitMarker;
}

bool ProbeInterruptTimeoutAbandon()
{
    // The witnesses outlive every delivery that could reach them, because each
    // thread is joined before this returns. If the abandon were broken the
    // callback would run against this memory, and the probe has to be able to
    // report that rather than demonstrate it by corrupting itself.
    BlockedInterruptWitness stalled;
    // Long enough that the delivery lands well inside part 3's window, short
    // enough that the probe costs a fraction of a second. The margin there is
    // fifty to one: a probe that fails on someone else's machine teaches them
    // to suspect their own change.
    stalled.unblock_delay_milliseconds = 20U;

    HostThread stalled_thread;
    if (!CreateHostThread(&BlockedSignalEntry, &stalled, &stalled_thread,
                          nullptr))
    {
        return false;
    }
    bool ok = WaitUntilSet(stalled.ready);

    // 1. The deadline is reported rather than waited past. An investigation
    //    into a stall that joins the stall is not an investigation.
    const auto started = std::chrono::steady_clock::now();
    auto failure = repiu::platform::ThreadInterruptFailure::kNone;
    ok = ok && !repiu::platform::InterruptHostThread(stalled_thread,
                                                     &CountInterruptCallback,
                                                     &stalled, 100U, &failure);
    const auto waited = std::chrono::steady_clock::now() - started;
    ok = ok && failure == repiu::platform::ThreadInterruptFailure::kTimedOut;
    // Generous, because what is under test is that it returns at all rather
    // than how promptly it does.
    ok = ok && waited < std::chrono::seconds(5);
    ok = ok && stalled.callback_count.load(std::memory_order_relaxed) == 0U;

    // 2. The layer is not left holding the request. A slot that stayed claimed
    //    would turn every later sample into a refusal, which is how a fix for a
    //    rare race becomes a permanent outage.
    InterruptWitness healthy;
    HostThread healthy_thread;
    if (ok &&
        CreateHostThread(&InterruptSpinEntry, &healthy, &healthy_thread,
                         nullptr))
    {
        ok = WaitUntilSpinning(healthy);
        ok = ok && repiu::platform::InterruptHostThread(healthy_thread,
                                                        &SampleEip, &healthy,
                                                        2000U);
        ok = ok && healthy.callback_count.load(std::memory_order_relaxed) == 1U;
        healthy.stop.store(true, std::memory_order_release);
        std::uint32_t healthy_exit = 0;
        ok = JoinHostThread(healthy_thread, 5000U, &healthy_exit) && ok;
        ok = ok && healthy_exit == kExitMarker;
        CloseHostThread(&healthy_thread);
    }
    else
    {
        ok = false;
    }

    // 3. The claim the compare-exchange makes, tested where it is the only
    //    thing that holds: the abandoned signal is let through while a second
    //    request is outstanding on a different thread.
    //
    //    Without it the late delivery claims the request it finds -- the
    //    handler has no way to tell it apart from its own -- and runs the
    //    callback against the second requester's `user_data`, reporting one
    //    thread's registers under the other's name. The second requester would
    //    then be told it succeeded.
    BlockedInterruptWitness second;
    HostThread second_thread;
    if (ok &&
        CreateHostThread(&BlockedSignalEntry, &second, &second_thread, nullptr))
    {
        ok = WaitUntilSet(second.ready);
        // The stalled thread unblocks 20 ms from here. The request below stays
        // outstanding for a second, because its own target has the signal
        // blocked too.
        stalled.unblock.store(true, std::memory_order_release);

        auto stolen = repiu::platform::ThreadInterruptFailure::kNone;
        ok = ok && !repiu::platform::InterruptHostThread(second_thread,
                                                         &CountInterruptCallback,
                                                         &second, 1000U,
                                                         &stolen);
        ok = ok && stolen == repiu::platform::ThreadInterruptFailure::kTimedOut;
        // The two shapes the failure takes: the callback ran at all, or it ran
        // and the request it took was reported as answered.
        ok = ok && second.callback_count.load(std::memory_order_relaxed) == 0U;

        ok = ok && WaitUntilSet(stalled.unblocked);
        ok = ok && stalled.callback_count.load(std::memory_order_relaxed) == 0U;

        // And the second thread's own pending signal, released with nothing
        // outstanding: the case that was already safe, kept honest.
        second.unblock.store(true, std::memory_order_release);
        ok = ok && WaitUntilSet(second.unblocked);
        repiu::platform::YieldMilliseconds(50U);
        ok = ok && second.callback_count.load(std::memory_order_relaxed) == 0U;
    }
    else
    {
        ok = false;
    }

    ok = ReleaseBlockedThread(second, second_thread) && ok;
    ok = ReleaseBlockedThread(stalled, stalled_thread) && ok;
    return ok;
}

#endif  // !defined(_WIN32)

}  // namespace

bool RunHostThreadProbe()
{
    const bool run_ok = ProbeRunAndCollect();
    const bool still_active_ok = ProbeStillActiveExitCode();
    const bool refusals_ok = ProbeRefusals();
    const bool interrupt_ok = ProbeInterruptSamplesAndEdits();
    const bool interrupt_refusal_ok = ProbeInterruptRefusals();
#if defined(_WIN32)
    // Reported as skipped rather than as a pass. There is no abandoned request
    // on this host to have an opinion about, and a line that says "true" for a
    // check that never ran is the kind of thing a later session believes.
    const bool abandon_ok = true;
    const char* const abandon_label = "host_thread_interrupt_abandon_skipped";
#else
    const bool abandon_ok = ProbeInterruptTimeoutAbandon();
    const char* const abandon_label = "host_thread_interrupt_abandon";
#endif
    const bool all = run_ok && still_active_ok && refusals_ok &&
        interrupt_ok && interrupt_refusal_ok && abandon_ok;
    std::cout << "host_thread_run=" << (run_ok ? "true" : "false")
              << "\nhost_thread_still_active_exit_code="
              << (still_active_ok ? "true" : "false")
              << "\nhost_thread_refusals=" << (refusals_ok ? "true" : "false")
              << "\nhost_thread_interrupt="
              << (interrupt_ok ? "true" : "false")
              << "\nhost_thread_interrupt_refusals="
              << (interrupt_refusal_ok ? "true" : "false")
              << "\n" << abandon_label << "="
              << (abandon_ok ? "true" : "false")
              << "\nhost_thread_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
