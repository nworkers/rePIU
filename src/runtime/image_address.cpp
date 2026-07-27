#include "repiu/runtime/image_address.h"

#include <algorithm>

namespace repiu::runtime
{

bool BuildRelocatedImageByteWindow(const RelocatedRuntimeImage& image,
                                   std::uint32_t linear_address,
                                   std::uint32_t bytes_before,
                                   std::uint32_t bytes_after,
                                   RelocatedImageByteWindow* window)
{
    if (window == nullptr)
    {
        return false;
    }

    *window = RelocatedImageByteWindow{};
    window->requested_address = linear_address;

    if (!image.valid)
    {
        window->message = "relocated image is not valid";
        return false;
    }

    for (const RelocatedRuntimeObject& object : image.objects)
    {
        const std::uint32_t object_base = object.relocated_base_address;
        // Task 329: reads go through the shared accessors so an external view
        // is windowed like an owning object instead of looking empty.
        const std::uint8_t* object_bytes =
            RelocatedRuntimeObjectBytes(object);
        const std::uint32_t object_size =
            static_cast<std::uint32_t>(
                RelocatedRuntimeObjectByteCount(object));
        if (object_size == 0 || linear_address < object_base)
        {
            continue;
        }

        const std::uint32_t object_offset = linear_address - object_base;
        if (object_offset >= object_size)
        {
            continue;
        }

        const std::uint32_t begin_offset =
            object_offset > bytes_before ? object_offset - bytes_before : 0;
        const std::uint32_t requested_end = object_offset + bytes_after + 1;
        const std::uint32_t end_offset = std::min(requested_end, object_size);

        window->object_index = object.object_index;
        window->window_base = object_base + begin_offset;
        window->focus_offset = object_offset - begin_offset;
        window->bytes.assign(object_bytes + begin_offset,
                             object_bytes + end_offset);
        window->valid = true;
        window->message = "relocated image byte window is ready";
        return true;
    }

    window->message = "linear address is outside relocated image bytes";
    return false;
}

}  // namespace repiu::runtime
