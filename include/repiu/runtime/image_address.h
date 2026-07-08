#ifndef REPIU_RUNTIME_IMAGE_ADDRESS_H_
#define REPIU_RUNTIME_IMAGE_ADDRESS_H_

#include "repiu/runtime/runtime_memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

struct RelocatedImageByteWindow
{
    bool valid = false;
    std::uint32_t requested_address = 0;
    std::uint32_t object_index = 0;
    std::uint32_t window_base = 0;
    std::uint32_t focus_offset = 0;
    std::vector<std::uint8_t> bytes;
    std::string message;
};

bool BuildRelocatedImageByteWindow(const RelocatedRuntimeImage& image,
                                   std::uint32_t linear_address,
                                   std::uint32_t bytes_before,
                                   std::uint32_t bytes_after,
                                   RelocatedImageByteWindow* window);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_IMAGE_ADDRESS_H_
