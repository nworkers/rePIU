#pragma once

#include <optional>

#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{
struct ThreadContext;

// nullopt means not a confirmed mode16 PUSH; false means rejected, not fallback.
std::optional<bool> HandleMode16StackPush(
    repiu::platform::GuestCpuContext* registers, ThreadContext* context);
}  // namespace repiu::engine
