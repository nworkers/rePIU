#pragma once

#include <optional>

#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{
struct ThreadContext;

// nullopt means the current instruction is not a supported 32-bit IRETD;
// false means that an IRETD candidate had an invalid frame.
std::optional<bool> HandleIretdInstruction(
    repiu::platform::GuestCpuContext* registers, ThreadContext* context);
}  // namespace repiu::engine
