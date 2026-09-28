#pragma once

#include <atomic>
#include <cstdint>

// Task 759. The Glide gate dispatch counters that the resolvers of both
// execution models add to: the direct model's resolver is in
// aot_dbt_glide_gate_dispatch.cpp, the cache model's in
// aot_dbt_dispatch_thunks_cache.cpp. Defined in aot_dbt_glide_gate_dispatch.cpp.

namespace repiu::engine::glide_gate_dispatch_state
{

extern std::atomic<std::uint32_t> entry_count;
extern std::atomic<std::uint32_t> success_count;
extern std::atomic<std::uint32_t> terminal_failure_count;

}  // namespace repiu::engine::glide_gate_dispatch_state
