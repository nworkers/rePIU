#pragma once

#include <optional>

#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{
struct ThreadContext;

// nullopt means not a confirmed prefix-free mode16 RETF; false means rejected.
std::optional<bool> HandleMode16FarReturn(
    repiu::platform::GuestCpuContext* registers, ThreadContext* context);
}  // namespace repiu::engine
