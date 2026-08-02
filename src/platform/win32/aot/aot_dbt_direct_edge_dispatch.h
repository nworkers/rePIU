#pragma once

#include <cstdint>

namespace repiu::platform::win32
{

struct ThreadContext;

void* GetAotDbtDirectEdgeDispatchThunkAddress();

bool FindAotDbtDirectEdgeFallbackTarget(
    const ThreadContext* context,
    std::uint32_t cache_address,
    std::uint32_t* guest_target);

}  // namespace repiu::platform::win32
