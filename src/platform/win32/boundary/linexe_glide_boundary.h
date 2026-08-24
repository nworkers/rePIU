#pragma once

// linexe far-transfer boundary, Glide gate boundary, and allocator control-flow
// exception recording extracted from execution_trampoline.cpp (Phase 1 increment 11).

#include "execution/thread_context.h"

#include <cstddef>
#include <cstdint>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/fault_handler.h"

// Task 503d-2. EXCEPTION_POINTERS is forward declared by its underlying tag so
// this header needs no <windows.h>: a pointer to an incomplete type is all a
// declaration requires, and on Windows it resolves to the very same type.
struct _EXCEPTION_POINTERS;

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
    const repiu::platform::FaultEvent& fault,
    ThreadContext* context);

bool HandleLinexeFarTransferBoundary(repiu::platform::GuestCpuContext* win32_context,
                                     ThreadContext* context);

bool HandleGlideGateBoundary(repiu::platform::GuestCpuContext* win32_context,
                             ThreadContext* context);

} // namespace repiu::platform::win32
