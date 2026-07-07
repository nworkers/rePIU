#ifndef REPIU_RUNTIME_RUNTIME_MEMORY_H_
#define REPIU_RUNTIME_RUNTIME_MEMORY_H_

#include "repiu/exe/dos4gw_loader.h"

#include <cstdint>
#include <vector>

namespace repiu::runtime
{

struct RuntimeObjectRegion
{
    std::uint32_t object_index = 0;
    std::uint32_t base_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t copied_bytes = 0;
    std::uint32_t flags = 0;
};

struct RuntimeMemoryPlan
{
    bool valid = false;
    std::vector<RuntimeObjectRegion> object_regions;
    std::uint32_t entry_linear_address = 0;
    std::uint32_t stack_top_linear_address = 0;
    std::uint32_t hle_reserve_base = 0;
    std::uint64_t total_object_virtual_bytes = 0;
    bool entry_valid = false;
    bool stack_valid = false;
};

bool BuildRuntimeMemoryPlan(const exe::Dos4gwLoadResult& load_result,
                            RuntimeMemoryPlan* plan,
                            exe::ParseError* error);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_RUNTIME_MEMORY_H_
