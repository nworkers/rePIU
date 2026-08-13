#pragma once

// linexe far-transfer boundary, Glide gate boundary, and allocator control-flow
// exception recording extracted from execution_trampoline.cpp (Phase 1 increment 11).

#include "execution/thread_context.h"

#include <cstddef>
#include <cstdint>

namespace repiu::platform::win32
{

struct GlideTexDownloadTableCall
{
    std::uint32_t tmu = 0U;
    std::uint32_t type = 0U;
    std::uint32_t data = 0U;
    std::uint32_t stack_advance = 0U;
};

bool DecodeGlideTexDownloadTableCall(const std::uint32_t* guest_stack,
                                     std::size_t word_count,
                                     GlideTexDownloadTableCall* call);

void RecordAllocatorControlFlowException(
    EXCEPTION_POINTERS* exception_info,
    ThreadContext* context);

bool HandleLinexeFarTransferBoundary(CONTEXT* win32_context,
                                     ThreadContext* context);

bool HandleGlideGateBoundary(CONTEXT* win32_context,
                             ThreadContext* context);

} // namespace repiu::platform::win32
