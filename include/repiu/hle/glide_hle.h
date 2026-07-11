#ifndef REPIU_HLE_GLIDE_HLE_H_
#define REPIU_HLE_GLIDE_HLE_H_

#include "repiu/exe/executable_headers.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::hle
{

enum class GlideReturnKind : std::uint8_t
{
    kVoid,
    kFxBool,
    kUInt32,
};

struct GlideSignature
{
    const char* name = nullptr;
    std::uint32_t argument_byte_count = 0;
    GlideReturnKind return_kind = GlideReturnKind::kVoid;
};

struct GlideExportGate
{
    std::string name;
    std::uint16_t ordinal = 0;
    std::uint32_t argument_byte_count = 0;
    std::uint32_t gate_offset = 0;
};

struct GlideGatePlan
{
    bool valid = false;
    std::uint32_t first_gate_offset = 0;
    std::uint32_t gate_stride = 0;
    std::vector<GlideExportGate> exports;
    std::vector<std::uint8_t> image;
    std::string message;
};

struct GlideAlphaCombineState
{
    std::uint32_t function = 0;
    std::uint32_t factor = 0;
    std::uint32_t local = 0;
    std::uint32_t other = 0;
    bool invert = false;
    bool valid = false;
};

using GlideColorCombineState = GlideAlphaCombineState;

struct GlideAlphaBlendState
{
    std::uint32_t rgb_source = 0;
    std::uint32_t rgb_destination = 0;
    std::uint32_t alpha_source = 0;
    std::uint32_t alpha_destination = 0;
    bool valid = false;
};

struct GlideLogicalState
{
    bool initialized = false;
    std::uint32_t selected_board = 0;
    bool window_open = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t color_format = 0;
    std::uint32_t origin = 0;
    std::uint32_t color_buffer_count = 0;
    std::uint32_t auxiliary_buffer_count = 0;
    std::uint32_t texture_memory_bytes = 0;
    std::uint32_t lfb_write_color_format = 0;
    GlideAlphaCombineState alpha_combine;
    GlideColorCombineState color_combine;
    GlideAlphaBlendState alpha_blend;
    std::uint32_t alpha_test_function = 0;
    std::uint32_t depth_buffer_function = 0;
    std::uint32_t fog_mode = 0;
    std::uint32_t clip_min_x = 0;
    std::uint32_t clip_min_y = 0;
    std::uint32_t clip_max_x = 0;
    std::uint32_t clip_max_y = 0;
    std::uint32_t cull_mode = 0;
};

constexpr std::uint32_t kPiuBansheeVirtualTextureMemoryBytes =
    8U * 1024U * 1024U;

bool BuildGlideGatePlan(
    const std::vector<exe::LeResidentName>& resident_names,
    std::uint32_t first_gate_offset,
    std::uint32_t gate_stride,
    std::uint32_t maximum_image_size,
    GlideGatePlan* plan);

const GlideExportGate* FindGlideExportByName(
    const GlideGatePlan& plan,
    const std::string& name);

const GlideExportGate* DecodeGlideGate(
    const GlideGatePlan& plan,
    std::uint32_t gate_offset);

const GlideSignature* FindGlideSignature(const std::string& name);

bool DecodeGlideResolution(std::uint32_t resolution,
                           std::uint32_t* width,
                           std::uint32_t* height);

bool CalculateGlideTextureMaxAddress(std::uint32_t texture_memory_bytes,
                                     std::uint32_t* maximum_address);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_HLE_H_
