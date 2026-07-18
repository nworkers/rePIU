#pragma once

// linexe far-transfer boundary, Glide gate boundary, and allocator control-flow
// exception recording extracted from execution_trampoline.cpp (Phase 1 increment 11).

#include "thread_context.h"

#include <cstdint>

namespace repiu::platform::win32
{

void RecordAllocatorControlFlowException(
    EXCEPTION_POINTERS* exception_info,
    ThreadContext* context);

bool HandleLinexeFarTransferBoundary(CONTEXT* win32_context,
                                     ThreadContext* context);

bool HandleGlideGateBoundary(CONTEXT* win32_context,
                             ThreadContext* context);

} // namespace repiu::platform::win32
