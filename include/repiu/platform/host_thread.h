#ifndef REPIU_PLATFORM_HOST_THREAD_H_
#define REPIU_PLATFORM_HOST_THREAD_H_

#include "repiu/platform/guest_cpu_context.h"

#include <cstdint>

// Task 503d-15. The operating system's own number for the calling thread.
//
// The engine asks this for one reason and reports it for another. The dispatch
// path compares it against the guest thread's, because a fault raised on any
// other thread is not the engine's to handle; and the shared telemetry block
// publishes it so a second process, and a person with a debugger attached, can
// say which thread the numbers belong to.
//
// That second use is why this is the operating system's identifier rather than
// a hash of `std::thread::id`: the number has to be the one the debugger shows.
// `GetCurrentThreadId` and `gettid` both are, on their own hosts.

namespace repiu::platform
{

[[nodiscard]] std::uint32_t CurrentThreadId();

// Task 503d-18. Creating a thread, asking whether it is still running, and
// collecting what it returned.
//
// The engine starts two: the AOT translation worker and the guest thread. Their
// shape comes from the four call sites rather than from the kernel32 table
// 3d-14 fenced -- the entry returns a `std::uint32_t`, which is neither
// Windows' `DWORD WINAPI(LPVOID)` nor POSIX's `void*(void*)` but what the
// callers actually pass and read.
using HostThreadEntry = std::uint32_t (*)(void* parameter);

struct HostThread
{
    // Windows keeps a HANDLE here; POSIX keeps a record holding the pthread_t,
    // a completion flag, and the exit code, because `pthread_t` carries none of
    // that and `pthread_join` waits when the caller must not.
    void* handle = nullptr;
    std::uint32_t id = 0;
    bool valid = false;
};

// What a thread is doing, without the ambiguity Windows has here.
//
// `GetExitCodeThread` answers 259 for a running thread, and 259 is also a legal
// exit code -- so a caller comparing against `STILL_ACTIVE`, as both of this
// engine's did, would read a thread that exited with 259 as one that never
// stops. Separating the two questions removes that, and the Windows backend
// decides `running` with a zero-length wait rather than from the code.
struct HostThreadStatus
{
    bool running = true;
    std::uint32_t exit_code = 0;
};

// `host_error` receives the host's own error number on failure, for the
// message; control flow reads the return value.
[[nodiscard]] bool CreateHostThread(HostThreadEntry entry,
                                    void* parameter,
                                    HostThread* thread,
                                    std::uint32_t* host_error);

// Asks without waiting, which is what the host poll loop needs: it has Glide
// commands to pump and timer ticks to deliver between questions.
[[nodiscard]] HostThreadStatus QueryHostThread(const HostThread& thread);

// Waits up to the timeout. Returns false if the thread was still running when
// it expired.
[[nodiscard]] bool JoinHostThread(const HostThread& thread,
                                  std::uint32_t timeout_milliseconds,
                                  std::uint32_t* exit_code);

// Releases what `CreateHostThread` allocated. The thread must have exited; a
// caller that has not established that has a bug rather than a cleanup step.
//
// The POSIX backend joins here to reclaim the thread's stack, which is why the
// precondition is a precondition and not advice: called on a thread still
// running, this waits for it forever. A caller that cannot establish the thread
// exited wants `DetachHostThread` instead.
void CloseHostThread(HostThread* thread);

// Task 507. The other half of `CloseHostThread`, for the caller that cannot
// establish the thread stopped.
//
// The shutdown path has one such caller. When a budget expires, the window
// closes, or the watchdog fires, the engine asks the guest thread to leave
// through the recovery entry -- and that request can be refused, because the
// thread may be somewhere the engine will not redirect it from, or may not
// answer at all. Windows finishes the job with `TerminateThread`; Linux has no
// counterpart and 3d-18 decided not to invent one, so the honest end of that
// path is a thread still running while the process goes down.
//
// This releases the caller's claim on such a thread without waiting for it.
// Windows closes the handle. POSIX detaches the thread and **deliberately keeps
// the record**: a thread that is still alive writes its completion flag and exit
// code into it when it ends, so freeing it here would be a use-after-free, while
// keeping it costs one allocation on the way out of the process. The loader
// makes the same trade for the relocated image, which it releases only when the
// run was not interrupted.
void DetachHostThread(HostThread* thread);

// Task 503d-20. Looking at another thread's registers, and changing them.
//
// Two callers want this and both want the same thing. The execution watchdog
// stops a guest that will not stop by pointing its context at the recovery
// entry; and asking a stalled guest where it is means reading its EIP while it
// runs. On Windows both are the same SuspendThread / GetThreadContext pair, and
// on Linux both are the same single signal, so this is one function rather than
// a sampling API beside an editing one.
//
// **The callback runs on a different thread on each host.** Windows freezes the
// target and runs the callback on the *calling* thread; Linux runs it on the
// *target thread itself*, inside a signal handler. That difference cannot be
// hidden and both callers have to be written for it: the callback must not
// block, must not allocate, and must not take a lock the target might already
// hold -- the same constraints 3c's fault callback carries, for the same reason.
//
// Edits to `registers` take effect when the target resumes. On Linux that is the
// signal return; on Windows it is SetThreadContext. The context is written back
// unconditionally on both, because asking whether the callback changed anything
// would mean comparing the whole structure -- which costs more than the write it
// would save.
using ThreadInterruptCallback = void (*)(GuestCpuContext* registers,
                                         void* user_data);

// Why an interrupt did not happen. A diagnostic that can only say "it failed"
// is half a diagnostic, and these three want different responses: a refusal is
// a caller's bug, a delivery failure means the thread is gone, and a timeout
// means it is there but not answering -- which is itself a finding about the
// thread being sampled.
enum class ThreadInterruptFailure : std::uint8_t
{
    kNone,
    kRefused,
    kNotDelivered,
    kTimedOut,
};

// Returns false if the target did not answer within the deadline, which is a
// real outcome rather than a failure to report: a thread that is not scheduled,
// or one stopped inside a signal handler of its own, will not.
//
// A deadline rather than an unbounded wait, because the caller asking is
// usually the one investigating a stall, and joining the thing being
// investigated is not an improvement.
//
// `failure` is optional and receives why, on the same terms `host_error` is
// passed to `CreateHostThread`: for the record, not for control flow.
[[nodiscard]] bool InterruptHostThread(const HostThread& thread,
                                       ThreadInterruptCallback callback,
                                       void* user_data,
                                       std::uint32_t timeout_milliseconds,
                                       ThreadInterruptFailure* failure =
                                           nullptr);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_THREAD_H_
