#pragma once

#include "execution/thread_context.h"

#include <cstdint>
#include <string_view>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{

enum class AotHleResumeOrigin
{
    kPendingExecution,
    kHandledGuestBoundary,
};

// After a DBT-mode HLE handler or an explicitly recognized guest boundary
// advances EIP, resume directly at a shared AOT cache entry. Ordinary callers
// require a pending boundary or legacy fallback; a handled boundary may opt in
// explicitly. A cache miss or other failure leaves TF fallback state unchanged.
bool TryResumeAotAfterHandledHle(repiu::platform::GuestCpuContext* win32_context,
                                 ThreadContext* context,
                                 std::uint32_t handled_guest_eip,
                                 AotHleResumeOrigin origin =
                                     AotHleResumeOrigin::kPendingExecution);

// Emit the existing opt-in HLE re-entry trace at a dispatcher boundary.
void TraceAotHleReentryState(
    const char* stage,
    const ThreadContext* context,
    const repiu::platform::GuestCpuContext* registers,
    std::uint32_t handled_guest_eip,
    std::uint32_t current_guest_eip);

bool ResolveAotDbtPostHleTranslationEnabled(std::string_view setting);

}  // namespace repiu::engine
