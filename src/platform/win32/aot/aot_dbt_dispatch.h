#pragma once

#include "thread_context.h"

#include <cstdint>

namespace repiu::platform::win32
{

// Task 276: after a DBT-mode HLE handler fully emulates a boundary and advances
// guest EIP, resume directly at an existing shared AOT cache entry instead of
// executing one additional guest instruction under TF. A cache miss or other
// failure leaves the caller's TF fallback state unchanged.
bool TryResumeAotAfterHandledHle(CONTEXT* win32_context,
                                 ThreadContext* context,
                                 std::uint32_t handled_guest_eip);

}  // namespace repiu::platform::win32
