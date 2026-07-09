#include "repiu/runtime/runtime_memory_arena.h"

#include <limits>

namespace repiu::runtime
{
namespace
{

bool AlignUp(std::uint64_t value,
             std::uint32_t alignment,
             std::uint32_t* aligned_value)
{
    if (aligned_value == nullptr)
    {
        return false;
    }

    *aligned_value = 0;
    if (alignment == 0)
    {
        if (value > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        *aligned_value = static_cast<std::uint32_t>(value);
        return true;
    }

    const std::uint64_t remainder = value % alignment;
    if (remainder == 0)
    {
        if (value > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        *aligned_value = static_cast<std::uint32_t>(value);
        return true;
    }

    const std::uint64_t aligned = value + (alignment - remainder);
    if (aligned > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    *aligned_value = static_cast<std::uint32_t>(aligned);
    return true;
}

}  // namespace

bool BuildRuntimeMemoryArenaPlan(std::uint32_t base_address,
                                 std::uint32_t image_reserve_size,
                                 std::uint32_t expansion_slack_size,
                                 RuntimeMemoryArenaPlan* plan)
{
    if (plan == nullptr)
    {
        return false;
    }

    *plan = RuntimeMemoryArenaPlan{};
    plan->base_address = base_address;
    plan->image_reserve_size = image_reserve_size;
    plan->expansion_slack_size = expansion_slack_size;

    if (base_address == 0 || image_reserve_size == 0)
    {
        plan->message = "runtime memory arena input is not valid";
        return true;
    }

    const std::uint64_t requested_size =
        static_cast<std::uint64_t>(image_reserve_size) +
        expansion_slack_size;
    if (requested_size > std::numeric_limits<std::uint32_t>::max())
    {
        plan->message = "runtime memory arena reserve size overflows";
        return true;
    }

    std::uint32_t arena_reserve_size = 0;
    if (!AlignUp(requested_size, 4096, &arena_reserve_size))
    {
        plan->message = "runtime memory arena aligned reserve size overflows";
        return true;
    }
    const std::uint64_t arena_end =
        static_cast<std::uint64_t>(base_address) + arena_reserve_size;
    if (arena_end > std::numeric_limits<std::uint32_t>::max())
    {
        plan->message = "runtime memory arena end address overflows";
        return true;
    }

    plan->valid = true;
    plan->arena_reserve_size = arena_reserve_size;
    plan->arena_end_address = static_cast<std::uint32_t>(arena_end);
    plan->message = "runtime memory arena plan is ready";
    return true;
}

}  // namespace repiu::runtime
