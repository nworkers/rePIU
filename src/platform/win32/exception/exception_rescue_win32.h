#pragma once

// Win32 vectored-exception-handling scaffolding extracted from
// execution_trampoline.cpp (Phase 1 increment 3). The heavy dispatch logic
// stays in execution_trampoline.cpp as DispatchGuestException (external
// linkage); only the RAII telemetry scope and the thin VEH entry point live
// here.

#include "thread_context.h"

#include <cstdint>
#include <windows.h>

namespace repiu::platform::win32
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
            InterlockedExchange(
                &context_->shared_live_telemetry->last_eip,
                static_cast<long>(eip));
            InterlockedIncrement(
                &context_->shared_live_telemetry->dispatch_entry_count);
            InterlockedIncrement(
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
            InterlockedIncrement(
                &context_->shared_live_telemetry->dispatch_exit_count);
            InterlockedIncrement(
                &context_->shared_live_telemetry->heartbeat);
        }
    }

    ExceptionDispatchScope(const ExceptionDispatchScope&) = delete;
    ExceptionDispatchScope& operator=(const ExceptionDispatchScope&) = delete;

private:
    ThreadContext* context_;
};

// Full VEH dispatch logic. Defined in execution_trampoline.cpp; carries
// external linkage so the thin entry below can call it across TUs.
LONG DispatchGuestException(EXCEPTION_POINTERS* exception_info);

// Thin vectored-exception entry registered via AddVectoredExceptionHandler.
// Defined in exception_rescue_win32.cpp; forwards to DispatchGuestException.
LONG WINAPI GuestStackVectoredExceptionHandler(EXCEPTION_POINTERS* exception_info);

} // namespace repiu::platform::win32
