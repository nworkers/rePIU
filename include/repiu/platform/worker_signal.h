#ifndef REPIU_PLATFORM_WORKER_SIGNAL_H_
#define REPIU_PLATFORM_WORKER_SIGNAL_H_

// Task 503d-6. The handshake between the guest thread and the AOT translation
// worker.
//
// It is one pattern used three times: reset the completion signal, publish the
// request, signal it, and block until the worker signals back. Windows spells
// that with two auto-reset events, `CreateEventA(nullptr, FALSE, FALSE,
// nullptr)`; what is named here is the pattern, not the spelling.
//
// A signal is passed around as `void*` rather than an object, because that is
// how ThreadContext already stores these and changing that would ripple through
// a structure this stage has no business reshaping. The Windows implementation
// is a HANDLE, exactly as before, so nothing about the timing or the wake
// latency Task 327 measures changes there.
//
// Auto-reset semantics are deliberate and load-bearing: a signal wakes exactly
// one waiter and consumes itself. A manual-reset signal here would let a stale
// completion satisfy the next request, and the guest thread would read a
// translation that had not happened.
//
// See docs/design/20260822-503-linux-execution-engine.md.

namespace repiu::platform
{

// Returns nullptr on failure. The signal starts unsignalled.
[[nodiscard]] void* CreateWorkerSignal();
void DestroyWorkerSignal(void* signal);

// Wakes one waiter. False means the host refused, which the callers treat as
// terminal -- a worker that cannot be signalled will never answer.
bool SignalWorker(void* signal);

// Blocks until signalled, then consumes the signal. False means the wait itself
// failed rather than that it timed out; there is no timeout.
bool WaitForWorkerSignal(void* signal);

// Discards a pending signal without waiting, so a request cannot be satisfied
// by a completion left over from the one before it.
void ResetWorkerSignal(void* signal);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_WORKER_SIGNAL_H_
