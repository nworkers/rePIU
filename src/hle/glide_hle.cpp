#include "repiu/hle/glide_hle.h"

#include <algorithm>
#include <array>

namespace repiu::hle
{
namespace
{

constexpr std::array<GlideSignature, 44> kObservedSignatures = {{
    {"_GRGLIDEINIT@0", 0U, GlideReturnKind::kVoid},
    {"_GRBUFFERCLEAR@12", 12U, GlideReturnKind::kVoid},
    {"_GRBUFFERSWAP@4", 4U, GlideReturnKind::kVoid},
    {"_GRBUFFERNUMPENDING@0", 0U, GlideReturnKind::kUInt32},
    {"_GRHINTS@8", 8U, GlideReturnKind::kVoid},
    {"_GRSSTQUERYHARDWARE@4", 4U, GlideReturnKind::kFxBool},
    {"_GRSSTSELECT@4", 4U, GlideReturnKind::kVoid},
    {"_GRSSTWINCLOSE@0", 0U, GlideReturnKind::kVoid},
    {"_GRSSTWINOPEN@28", 28U, GlideReturnKind::kFxBool},
    {"_GRSSTSCREENWIDTH@0", 0U, GlideReturnKind::kUInt32},
    {"_GRSSTSCREENHEIGHT@0", 0U, GlideReturnKind::kUInt32},
    {"_GRTEXMINADDRESS@4", 4U, GlideReturnKind::kUInt32},
    {"_GRTEXMAXADDRESS@4", 4U, GlideReturnKind::kUInt32},
    {"_GRTEXTEXTUREMEMREQUIRED@8", 8U, GlideReturnKind::kUInt32},
    {"_GRTEXDOWNLOADMIPMAPLEVEL@32", 32U, GlideReturnKind::kVoid},
    {"_GRTEXCOMBINE@28", 28U, GlideReturnKind::kVoid},
    {"_GRTEXCLAMPMODE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXFILTERMODE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXMIPMAPMODE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXSOURCE@16", 16U, GlideReturnKind::kVoid},
    {"_GRCOLORMASK@8", 8U, GlideReturnKind::kVoid},
    {"_GRRENDERBUFFER@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHMASK@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHBIASLEVEL@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHBUFFERMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRLFBWRITECOLORFORMAT@4", 4U, GlideReturnKind::kVoid},
    {"_GRALPHACOMBINE@20", 20U, GlideReturnKind::kVoid},
    {"_GRCOLORCOMBINE@20", 20U, GlideReturnKind::kVoid},
    {"_GRALPHABLENDFUNCTION@16", 16U, GlideReturnKind::kVoid},
    {"_GRALPHATESTFUNCTION@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHBUFFERFUNCTION@4", 4U, GlideReturnKind::kVoid},
    {"_GRFOGMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRCLIPWINDOW@16", 16U, GlideReturnKind::kVoid},
    {"_GRCULLMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRGLIDEGETSTATE@4", 4U, GlideReturnKind::kVoid},
    {"_GRGLIDESETSTATE@4", 4U, GlideReturnKind::kVoid},
    {"_GRDITHERMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRDRAWLINE@8", 8U, GlideReturnKind::kVoid},
    {"_GRDRAWPOINT@4", 4U, GlideReturnKind::kVoid},
    {"_GRDRAWTRIANGLE@12", 12U, GlideReturnKind::kVoid},
    {"_GRDRAWPLANARPOLYGON@12", 12U, GlideReturnKind::kVoid},
    {"_GRDRAWPLANARPOLYGONVERTEXLIST@8", 8U, GlideReturnKind::kVoid},
    {"_GRDRAWPOLYGON@12", 12U, GlideReturnKind::kVoid},
    {"_GRGLIDESHUTDOWN@0", 0U, GlideReturnKind::kVoid},
}};

}  // namespace

bool BuildGlideStateImage(const GlideLogicalState& state,
                          GlideStateImage* image)
{
    if (image == nullptr)
    {
        return false;
    }
    image->fill(0U);
    std::size_t cursor = 0;
    const auto write_u32 = [&image, &cursor](std::uint32_t value) {
        if (cursor + 4U > image->size())
        {
            return false;
        }
        (*image)[cursor] = static_cast<std::uint8_t>(value);
        (*image)[cursor + 1U] = static_cast<std::uint8_t>(value >> 8U);
        (*image)[cursor + 2U] = static_cast<std::uint8_t>(value >> 16U);
        (*image)[cursor + 3U] = static_cast<std::uint8_t>(value >> 24U);
        cursor += 4U;
        return true;
    };
    constexpr std::uint32_t kStateMagic = 0x53504952U;
    constexpr std::uint32_t kStateVersion = 2U;
    return write_u32(kStateMagic) && write_u32(kStateVersion) &&
        write_u32(state.initialized ? 1U : 0U) &&
        write_u32(state.selected_board) &&
        write_u32(state.window_open ? 1U : 0U) &&
        write_u32(state.width) && write_u32(state.height) &&
        write_u32(state.color_format) && write_u32(state.origin) &&
        write_u32(state.color_buffer_count) &&
        write_u32(state.auxiliary_buffer_count) &&
        write_u32(state.texture_memory_bytes) &&
        write_u32(state.lfb_write_color_format) &&
        write_u32(state.alpha_test_function) &&
        write_u32(state.depth_buffer_function) &&
        write_u32(state.fog_mode) && write_u32(state.clip_min_x) &&
        write_u32(state.clip_min_y) && write_u32(state.clip_max_x) &&
        write_u32(state.clip_max_y) && write_u32(state.cull_mode) &&
        write_u32(state.dither_mode);
}

bool ParseGlideStateImage(const GlideStateImage& image,
                          GlideLogicalState* state)
{
    if (state == nullptr)
    {
        return false;
    }
    std::size_t cursor = 0;
    const auto read_u32 = [&image, &cursor](std::uint32_t* value) {
        if (value == nullptr || cursor + 4U > image.size())
        {
            return false;
        }
        *value = static_cast<std::uint32_t>(image[cursor]) |
            (static_cast<std::uint32_t>(image[cursor + 1U]) << 8U) |
            (static_cast<std::uint32_t>(image[cursor + 2U]) << 16U) |
            (static_cast<std::uint32_t>(image[cursor + 3U]) << 24U);
        cursor += 4U;
        return true;
    };
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t initialized = 0;
    std::uint32_t window_open = 0;
    GlideLogicalState restored = *state;
    constexpr std::uint32_t kStateMagic = 0x53504952U;
    constexpr std::uint32_t kStateVersion = 2U;
    if (!read_u32(&magic) || !read_u32(&version) ||
        magic != kStateMagic || version != kStateVersion ||
        !read_u32(&initialized) || !read_u32(&restored.selected_board) ||
        !read_u32(&window_open) || !read_u32(&restored.width) ||
        !read_u32(&restored.height) ||
        !read_u32(&restored.color_format) ||
        !read_u32(&restored.origin) ||
        !read_u32(&restored.color_buffer_count) ||
        !read_u32(&restored.auxiliary_buffer_count) ||
        !read_u32(&restored.texture_memory_bytes) ||
        !read_u32(&restored.lfb_write_color_format) ||
        !read_u32(&restored.alpha_test_function) ||
        !read_u32(&restored.depth_buffer_function) ||
        !read_u32(&restored.fog_mode) ||
        !read_u32(&restored.clip_min_x) ||
        !read_u32(&restored.clip_min_y) ||
        !read_u32(&restored.clip_max_x) ||
        !read_u32(&restored.clip_max_y) ||
        !read_u32(&restored.cull_mode) ||
        !read_u32(&restored.dither_mode))
    {
        return false;
    }
    restored.initialized = initialized != 0U;
    restored.window_open = window_open != 0U;
    *state = restored;
    return true;
}

bool BuildGlideGatePlan(
    const std::vector<exe::LeResidentName>& resident_names,
    std::uint32_t first_gate_offset,
    std::uint32_t gate_stride,
    std::uint32_t maximum_image_size,
    GlideGatePlan* plan)
{
    if (plan == nullptr || resident_names.empty() || gate_stride < 5U)
    {
        return false;
    }

    *plan = GlideGatePlan{};
    plan->first_gate_offset = first_gate_offset;
    plan->gate_stride = gate_stride;
    std::uint16_t maximum_ordinal = 0;
    for (const exe::LeResidentName& resident : resident_names)
    {
        if (resident.ordinal == 0)
        {
            continue;
        }
        if (!resident.decorated_argument_size_valid)
        {
            plan->message = "Glide export has no validated @N ABI suffix";
            return false;
        }
        maximum_ordinal = std::max(maximum_ordinal, resident.ordinal);
        plan->exports.push_back({resident.name,
                                 resident.ordinal,
                                 resident.argument_byte_count,
                                 first_gate_offset +
                                     resident.ordinal * gate_stride});
    }

    const std::uint64_t image_size =
        (static_cast<std::uint64_t>(maximum_ordinal) + 1U) * gate_stride;
    if (plan->exports.empty() || image_size > maximum_image_size)
    {
        plan->message = "Glide export gates do not fit in reserved code tail";
        return false;
    }

    plan->image.assign(static_cast<std::size_t>(image_size), 0x90U);
    for (const GlideExportGate& gate : plan->exports)
    {
        const std::size_t offset =
            static_cast<std::size_t>(gate.ordinal) * gate_stride;
        plan->image[offset] = 0x0FU;
        plan->image[offset + 1U] = 0x0BU;
        plan->image[offset + 2U] =
            static_cast<std::uint8_t>(gate.ordinal);
        plan->image[offset + 3U] =
            static_cast<std::uint8_t>(gate.ordinal >> 8U);
        plan->image[offset + 4U] = 0xC3U;
    }
    plan->valid = true;
    plan->message = "asset-derived Glide export gates are ready";
    return true;
}

const GlideExportGate* FindGlideExportByName(
    const GlideGatePlan& plan,
    const std::string& name)
{
    if (!plan.valid)
    {
        return nullptr;
    }
    const auto found = std::find_if(
        plan.exports.begin(),
        plan.exports.end(),
        [&name](const GlideExportGate& gate) { return gate.name == name; });
    return found != plan.exports.end() ? &*found : nullptr;
}

const GlideExportGate* DecodeGlideGate(const GlideGatePlan& plan,
                                       std::uint32_t gate_offset)
{
    if (!plan.valid || gate_offset < plan.first_gate_offset)
    {
        return nullptr;
    }
    const auto found = std::find_if(
        plan.exports.begin(),
        plan.exports.end(),
        [gate_offset](const GlideExportGate& gate) {
            return gate.gate_offset == gate_offset;
        });
    return found != plan.exports.end() ? &*found : nullptr;
}

const GlideSignature* FindGlideSignature(const std::string& name)
{
    const auto found = std::find_if(
        kObservedSignatures.begin(),
        kObservedSignatures.end(),
        [&name](const GlideSignature& signature) {
            return signature.name != nullptr && name == signature.name;
        });
    return found != kObservedSignatures.end() ? &*found : nullptr;
}

bool DecodeGlideResolution(std::uint32_t resolution,
                           std::uint32_t* width,
                           std::uint32_t* height)
{
    if (width == nullptr || height == nullptr || resolution != 7U)
    {
        return false;
    }
    *width = 640U;
    *height = 480U;
    return true;
}

bool CalculateGlideTextureMaxAddress(std::uint32_t texture_memory_bytes,
                                     std::uint32_t* maximum_address)
{
    constexpr std::uint32_t kTextureStartAlignment = 8U;
    if (maximum_address == nullptr ||
        texture_memory_bytes < kTextureStartAlignment ||
        texture_memory_bytes % kTextureStartAlignment != 0U)
    {
        return false;
    }
    *maximum_address = texture_memory_bytes - kTextureStartAlignment;
    return true;
}

bool CalculateGlideTextureMemoryRequired(std::uint32_t even_odd_mask,
                                       const GlideTextureInfo& info,
                                       std::uint32_t* required_bytes)
{
    constexpr std::uint32_t kEvenMask = 1U;
    constexpr std::uint32_t kOddMask = 2U;
    constexpr std::uint32_t kBothMask = kEvenMask | kOddMask;
    constexpr std::uint32_t kMaxLod = 8U;
    constexpr std::uint32_t kAspectSquare = 3U;
    constexpr std::uint32_t kMaxAspect = 6U;
    constexpr std::uint32_t kMaxFormat = 12U;
    constexpr std::uint32_t kTextureStartAlignment = 8U;
    if (required_bytes == nullptr || (even_odd_mask & ~kBothMask) != 0U ||
        even_odd_mask == 0U || info.small_lod > info.large_lod ||
        info.large_lod > kMaxLod || info.aspect_ratio > kMaxAspect ||
        info.format > kMaxFormat)
    {
        return false;
    }

    const std::uint32_t bytes_per_texel = info.format <= 5U ? 1U : 2U;
    std::uint64_t total = 0;
    for (std::uint32_t lod = info.small_lod; lod <= info.large_lod; ++lod)
    {
        const std::uint32_t lod_mask = (lod & 1U) == 0U ? kEvenMask : kOddMask;
        if ((even_odd_mask & lod_mask) == 0U)
        {
            continue;
        }
        const std::int32_t width_log2 = static_cast<std::int32_t>(lod) +
            static_cast<std::int32_t>(info.aspect_ratio) -
            static_cast<std::int32_t>(kAspectSquare);
        const std::int32_t height_log2 = static_cast<std::int32_t>(lod) -
            static_cast<std::int32_t>(info.aspect_ratio) +
            static_cast<std::int32_t>(kAspectSquare);
        const std::uint32_t width = 1U << std::max(width_log2, 0);
        const std::uint32_t height = 1U << std::max(height_log2, 0);
        total += static_cast<std::uint64_t>(width) * height * bytes_per_texel;
    }
    const std::uint64_t aligned = (total + kTextureStartAlignment - 1U) &
        ~(static_cast<std::uint64_t>(kTextureStartAlignment) - 1U);
    if (aligned > UINT32_MAX)
    {
        return false;
    }
    *required_bytes = static_cast<std::uint32_t>(aligned);
    return true;
}
}  // namespace repiu::hle
