#pragma once

// Win32 vectored-exception-handling scaffolding extracted from
// execution_trampoline.cpp (Phase 1 increment 3). The heavy dispatch logic
// stays in execution_trampoline.cpp as DispatchGuestException (external
// linkage); only the RAII telemetry scope and the thin VEH entry point live
// here.

#include "thread_context.h"

// Task 503d-14. The Windows headers stay, fenced: the two declarations at the
// bottom describe how Windows delivers a fault, so the operating system names
// their types. Everything above them is platform neutral.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include "repiu/platform/atomic_ops.h"
#include "repiu/platform/fault_handler.h"

namespace repiu::engine
{

class ExceptionDispatchScope
{
public:
    ExceptionDispatchScope(ThreadContext* context, std::uint32_t eip)
        : context_(context)
    {
        context_->exception_dispatch_last_eip.store(
            eip,
            std::memory_order_relaxed);
        context_->exception_dispatch_entry_count.fetch_add(
            1,
            std::memory_order_relaxed);
        context_->live_telemetry_phase.store(2, std::memory_order_relaxed);
        context_->live_telemetry_heartbeat.fetch_add(
            1,
            std::memory_order_relaxed);
        if (context_->shared_live_telemetry != nullptr)
        {
            repiu::platform::AtomicExchange(
                &context_->shared_live_telemetry->last_eip,
                static_cast<long>(eip));
            repiu::platform::AtomicIncrement(
                &context_->shared_live_telemetry->dispatch_entry_count);
            repiu::platform::AtomicIncrement(
                &context_->shared_live_telemetry->heartbeat);
        }
    }

    ~ExceptionDispatchScope()
    {
        context_->exception_dispatch_exit_count.fetch_add(
            1,
            std::memory_order_relaxed);
        context_->live_telemetry_phase.store(3, std::memory_order_relaxed);
        context_->live_telemetry_heartbeat.fetch_add(
            1,
            std::memory_order_relaxed);
        if (context_->shared_live_telemetry != nullptr)
        {
            repiu::platform::AtomicIncrement(
                &context_->shared_live_telemetry->dispatch_exit_count);
            repiu::platform::AtomicIncrement(
                &context_->shared_live_telemetry->heartbeat);
        }
    }

    ExceptionDispatchScope(const ExceptionDispatchScope&) = delete;
    ExceptionDispatchScope& operator=(const ExceptionDispatchScope&) = delete;

private:
    ThreadContext* context_;
};

// Task 503d-5. The dispatch logic proper, defined in execution_trampoline.cpp.
// It names no Windows type: what reaches it is the fault as the platform layer
// reports it, whichever host produced it.
repiu::platform::FaultDisposition DispatchGuestFault(
    repiu::platform::FaultEvent& fault);

#if defined(_WIN32)
// The Win32-shaped entry to the above, and all that is left of that shape:
// validates the structure Windows handed over (Task 296) and builds the event.
//
// Task 503d-14: fenced rather than converted. Both declarations describe how
// Windows delivers a fault -- a structure the kernel fills in, and the callback
// AddVectoredExceptionHandler takes. Linux delivers the same faults through the
// 3c handler installed by repiu::platform::InstallFaultHandler, which arrives at
// DispatchGuestFault above without passing through here at all. There is
// nothing on the other host for these two to become.
LONG DispatchGuestException(EXCEPTION_POINTERS* exception_info);

// Thin vectored-exception entry registered via AddVectoredExceptionHandler.
// Defined in exception_rescue_win32.cpp; forwards to DispatchGuestException.
LONG WINAPI GuestStackVectoredExceptionHandler(EXCEPTION_POINTERS* exception_info);
#endif

} // namespace repiu::engine
