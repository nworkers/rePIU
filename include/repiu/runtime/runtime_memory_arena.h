#ifndef REPIU_RUNTIME_RUNTIME_MEMORY_ARENA_H_
#define REPIU_RUNTIME_RUNTIME_MEMORY_ARENA_H_

#include <cstdint>
#include <string>

namespace repiu::runtime
{

struct RuntimeMemoryArenaPlan
{
    bool valid = false;
    std::uint32_t base_address = 0;
    std::uint32_t image_reserve_size = 0;
    std::uint32_t expansion_slack_size = 0;
    std::uint32_t arena_reserve_size = 0;
    std::uint32_t arena_end_address = 0;
    std::string message;
};

bool BuildRuntimeMemoryArenaPlan(std::uint32_t base_address,
                                 std::uint32_t image_reserve_size,
                                 std::uint32_t expansion_slack_size,
                                 RuntimeMemoryArenaPlan* plan);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_RUNTIME_MEMORY_ARENA_H_
