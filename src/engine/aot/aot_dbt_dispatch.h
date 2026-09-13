#pragma once

#include "execution/thread_context.h"

#include <cstdint>
#include <string_view>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{

// After a DBT-mode HLE handler fully emulates an instruction and advances guest
// EIP, resume directly at an existing shared AOT cache entry from either a
// cache boundary or legacy fallback. A cache miss or other failure leaves the
// caller's TF fallback state unchanged.
bool TryResumeAotAfterHandledHle(repiu::platform::GuestCpuContext* win32_context,
                                 ThreadContext* context,
                                 std::uint32_t handled_guest_eip);

// Emit the existing opt-in HLE re-entry trace at a dispatcher boundary.
void TraceAotHleReentryState(
    const char* stage,
    const ThreadContext* context,
    const repiu::platform::GuestCpuContext* registers,
    std::uint32_t handled_guest_eip,
    std::uint32_t current_guest_eip);

bool ResolveAotDbtPostHleTranslationEnabled(std::string_view setting);

}  // namespace repiu::engine
