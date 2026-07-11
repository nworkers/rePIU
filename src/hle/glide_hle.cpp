#include "repiu/hle/glide_hle.h"

#include <algorithm>
#include <array>

namespace repiu::hle
{
namespace
{

constexpr std::array<GlideSignature, 22> kObservedSignatures = {{
    {"_GRGLIDEINIT@0", 0U, GlideReturnKind::kVoid},
    {"_GRSSTQUERYHARDWARE@4", 4U, GlideReturnKind::kFxBool},
    {"_GRSSTSELECT@4", 4U, GlideReturnKind::kVoid},
    {"_GRSSTWINOPEN@28", 28U, GlideReturnKind::kFxBool},
    {"_GRSSTSCREENWIDTH@0", 0U, GlideReturnKind::kUInt32},
    {"_GRSSTSCREENHEIGHT@0", 0U, GlideReturnKind::kUInt32},
    {"_GRTEXMINADDRESS@4", 4U, GlideReturnKind::kUInt32},
    {"_GRTEXMAXADDRESS@4", 4U, GlideReturnKind::kUInt32},
    {"_GRCOLORMASK@8", 8U, GlideReturnKind::kVoid},
    {"_GRRENDERBUFFER@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHMASK@4", 4U, GlideReturnKind::kVoid},
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
    {"_GRGLIDESHUTDOWN@0", 0U, GlideReturnKind::kVoid},
}};

}  // namespace

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

}  // namespace repiu::hle
