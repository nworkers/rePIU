#pragma once

#include "thread_context.h"

#include <cstdint>

namespace repiu::engine
{

enum class AotDbtHleFallbackReason : std::uint32_t
{
    kInvalidSite = 0,
    kVehRequired,
    kUnhandled,
    kTargetMiss,
    kStateMismatch,
    kUnknown,
};

void RecordAotDbtHleFallback(
    ThreadContext* context,
    AotDbtHleFallbackReason reason);

void* GetAotDbtHleDispatchThunkAddress();

}  // namespace repiu::engine
