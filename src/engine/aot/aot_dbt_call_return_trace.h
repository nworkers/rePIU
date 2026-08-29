#pragma once

#include "repiu/engine/execution_trampoline.h"

#include <cstdint>

namespace repiu::engine
{

struct ThreadContext;

std::uint32_t RecordAotDbtCallReturnCall(
    ThreadContext* context,
    AotTransferOrigin origin,
    std::uint32_t source,
    std::uint32_t target,
    std::uint32_t return_address,
    std::uint32_t entry_esp);

void RecordAotDbtCallReturnReturn(
    ThreadContext* context,
    AotTransferOrigin origin,
    std::uint32_t source,
    std::uint32_t target,
    std::uint32_t esp,
    std::uint32_t call_sequence,
    std::uint32_t expected_source,
    std::uint32_t expected_target,
    std::uint32_t expected_return_address,
    std::uint32_t call_entry_esp);

}  // namespace repiu::engine
