#include "glide_texture_table_stack_probe.h"

#include "boundary/linexe_glide_boundary.h"
#include "repiu/hle/glide_texture_decode.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{

bool RunGlideTextureTableStackProbe()
{
    constexpr std::uint32_t kReturnAddress = 0x04012345U;
    constexpr std::uint32_t kPaletteAddress = 0x04ACA200U;
    const std::array<std::uint32_t, 4> frame = {
        kReturnAddress, 0U, 2U, kPaletteAddress};

    engine::GlideTexDownloadTableCall call;
    if (!engine::DecodeGlideTexDownloadTableCall(
            frame.data(), frame.size(), &call) ||
        call.tmu != 0U || call.type != 2U ||
        call.data != kPaletteAddress || call.stack_advance != 16U)
    {
        std::cerr << "glide_texture_table_stack_probe failure: valid frame"
                  << " was decoded incorrectly\n";
        return false;
    }
    if (engine::DecodeGlideTexDownloadTableCall(
            frame.data(), frame.size() - 1U, &call) ||
        engine::DecodeGlideTexDownloadTableCall(
            frame.data(), frame.size(), nullptr))
    {
        std::cerr << "glide_texture_table_stack_probe failure: invalid frame"
                  << " was accepted\n";
        return false;
    }

    std::array<std::uint32_t, 256> source_palette{};
    source_palette[7] = 0x12ABCDEFU;
    std::array<std::uint8_t, 1024> rgba_palette{};
    if (!hle::DecodeGlidePaletteToRgba8(
            source_palette.data(), source_palette.size(), rgba_palette.data(),
            rgba_palette.size()) ||
        rgba_palette[28] != 0xABU || rgba_palette[29] != 0xCDU ||
        rgba_palette[30] != 0xEFU || rgba_palette[31] != 0xFFU)
    {
        std::cerr << "glide_texture_table_stack_probe failure: palette RGB"
                  << " or ignored alpha was decoded incorrectly\n";
        return false;
    }

    std::vector<std::uint8_t> decoded;
    const std::uint8_t p8_texel = 7U;
    const std::array<std::uint8_t, 2> ap88_texel = {7U, 0x40U};
    if (!hle::DecodeGlideTextureToRgba8(
            5U, 1U, 1U, &p8_texel, 1U, rgba_palette.data(), &decoded) ||
        decoded != std::vector<std::uint8_t>({0xABU, 0xCDU, 0xEFU, 0xFFU}) ||
        !hle::DecodeGlideTextureToRgba8(
            14U, 1U, 1U, ap88_texel.data(), ap88_texel.size(),
            rgba_palette.data(), &decoded) ||
        decoded != std::vector<std::uint8_t>({0xABU, 0xCDU, 0xEFU, 0x40U}))
    {
        std::cerr << "glide_texture_table_stack_probe failure: P_8/AP_88"
                  << " alpha semantics are incorrect\n";
        return false;
    }
    if (!hle::IsGlidePalettizedTextureFormat(5U) ||
        !hle::IsGlidePalettizedTextureFormat(14U) ||
        hle::IsGlidePalettizedTextureFormat(12U))
    {
        std::cerr << "glide_texture_table_stack_probe failure: palette refresh"
                  << " format selection is incorrect\n";
        return false;
    }

    std::cout << "glide_texture_table_stack_probe=pass\n";
    return true;
}

} // namespace repiu::tools
