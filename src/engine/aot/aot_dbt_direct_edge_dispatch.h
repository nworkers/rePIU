#pragma once

#include <cstdint>

namespace repiu::engine
{

struct ThreadContext;

void* GetAotDbtDirectEdgeDispatchThunkAddress();

bool FindAotDbtDirectEdgeFallbackTarget(
    const ThreadContext* context,
    std::uint32_t cache_address,
    std::uint32_t* guest_target,
    std::uint32_t* guest_source = nullptr);

}  // namespace repiu::engine
