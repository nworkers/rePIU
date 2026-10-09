#include "mesa_fx_texture_source_probe.h"

#include "repiu/hle/mesa_fx_texture_source.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using hle::BuildMesaFxFullPrecisionTexture;
using hle::MesaFxGateState;
using hle::MesaFxSourceOutcome;

// A flat stand-in for guest memory, laid out the way the shared Mesa build
// leaves things at the upload call.
constexpr std::uint32_t kBase = 0x00100000U;
constexpr std::uint32_t kStack = kBase + 0x0100U;
constexpr std::uint32_t kTextureObject = kBase + 0x1000U;
constexpr std::uint32_t kTextureInfo = kBase + 0x3000U;
constexpr std::uint32_t kImage = kBase + 0x4000U;
constexpr std::uint32_t kLevelData = kBase + 0x5000U;
constexpr std::uint32_t kSource = kBase + 0x10000U;
constexpr std::uint32_t kLevel = 2U;

struct FakeGuest
{
    std::vector<std::uint8_t> memory =
        std::vector<std::uint8_t>(0x80000U, 0U);
    bool deny_texture_object = false;

    void Put32(std::uint32_t address, std::uint32_t value)
    {
        std::memcpy(memory.data() + (address - kBase), &value, 4U);
    }

    hle::MesaFxGuestReader Reader()
    {
        return [this](std::uint32_t address, void* out, std::size_t size) {
            if (address < kBase || address - kBase + size > memory.size() ||
                (deny_texture_object && address >= kTextureObject &&
                 address < kTextureObject + 0x1000U))
            {
                return false;
            }
            std::memcpy(out, memory.data() + (address - kBase), size);
            return true;
        };
    }
};

std::uint8_t Channel(std::uint32_t x, std::uint32_t y, std::uint32_t c)
{
    return static_cast<std::uint8_t>((x * 37U + y * 11U + c * 71U) & 0xFFU);
}

// Lays out a source image of `sw` x `sh` texels with `texel_bytes` channels
// and returns the game's converted data at `w` x `h`, made the way Mesa does.
std::vector<std::uint8_t> Setup(FakeGuest* guest, std::uint32_t sw,
                                std::uint32_t sh, std::uint32_t w,
                                std::uint32_t h, bool argb4444)
{
    const std::uint32_t texel_bytes = argb4444 ? 4U : 3U;
    guest->Put32(kStack + 0x34U, kTextureObject);
    guest->Put32(kTextureObject + 0x484U, kTextureInfo);
    guest->Put32(kTextureObject + 0x50U + kLevel * 4U, kImage);
    guest->Put32(kTextureInfo + 0x14U + kLevel * 0x14U + 8U, kLevelData);
    guest->Put32(kImage + 0xCU, sw);
    guest->Put32(kImage + 0x10U, sh);
    guest->Put32(kImage + 0x34U, kSource);
    for (std::uint32_t y = 0; y < sh; ++y)
    {
        for (std::uint32_t x = 0; x < sw; ++x)
        {
            for (std::uint32_t c = 0; c < texel_bytes; ++c)
            {
                guest->memory[kSource - kBase +
                              (y * sw + x) * texel_bytes + c] =
                    Channel(x, y, c);
            }
        }
    }
    std::vector<std::uint8_t> game(static_cast<std::size_t>(w) * h * 2U);
    for (std::uint32_t y = 0; y < h; ++y)
    {
        for (std::uint32_t x = 0; x < w; ++x)
        {
            const std::uint32_t sx = x / (w / sw);
            const std::uint32_t sy = y / (h / sh);
            const std::uint8_t r = Channel(sx, sy, 0);
            const std::uint8_t g = Channel(sx, sy, 1);
            const std::uint8_t b = Channel(sx, sy, 2);
            const std::uint8_t a = argb4444 ? Channel(sx, sy, 3) : 0xFFU;
            const std::uint16_t packed = argb4444
                ? static_cast<std::uint16_t>(((a & 0xF0U) << 8) |
                                             ((r & 0xF0U) << 4) |
                                             (g & 0xF0U) | (b >> 4))
                : static_cast<std::uint16_t>(((r & 0xF8U) << 8) |
                                             ((g & 0xFCU) << 3) | (b >> 3));
            game[(y * w + x) * 2U] = static_cast<std::uint8_t>(packed);
            game[(y * w + x) * 2U + 1U] =
                static_cast<std::uint8_t>(packed >> 8);
        }
    }
    return game;
}

MesaFxGateState Gate()
{
    MesaFxGateState gate;
    gate.esi = kTextureInfo;
    gate.ebp = kLevel;
    gate.esp = kStack;
    gate.data_address = kLevelData;
    return gate;
}

MesaFxSourceOutcome Run(FakeGuest* guest, const MesaFxGateState& gate,
                        std::uint32_t format, std::uint32_t w,
                        std::uint32_t h, const std::vector<std::uint8_t>& game,
                        std::vector<std::uint8_t>* rgba8)
{
    return BuildMesaFxFullPrecisionTexture(guest->Reader(), gate, format, w, h,
                                           game.data(), game.size(), rgba8);
}

}  // namespace

bool RunMesaFxTextureSourceProbe()
{
    std::vector<std::uint8_t> rgba8;

    // 4444 at the same size: the original comes back untouched.
    FakeGuest same;
    const auto same_game = Setup(&same, 64U, 32U, 64U, 32U, true);
    const bool argb4444 =
        Run(&same, Gate(), 12U, 64U, 32U, same_game, &rgba8) ==
            MesaFxSourceOutcome::kUsed &&
        rgba8.size() == 64U * 32U * 4U && rgba8[0] == Channel(0, 0, 0) &&
        rgba8[4U * (5U * 64U + 7U) + 3U] == Channel(7, 5, 3);

    // 565 from RGB, alpha opaque.
    FakeGuest rgb;
    const auto rgb_game = Setup(&rgb, 32U, 32U, 32U, 32U, false);
    const bool rgb565 =
        Run(&rgb, Gate(), 10U, 32U, 32U, rgb_game, &rgba8) ==
            MesaFxSourceOutcome::kUsed &&
        rgba8[3] == 0xFFU && rgba8[4U * 33U + 2U] == Channel(1, 1, 2);

    // Mesa enlarged a 32x16 original to 128x64 by whole factors.
    FakeGuest scaled;
    const auto scaled_game = Setup(&scaled, 32U, 16U, 128U, 64U, true);
    const bool upscale =
        Run(&scaled, Gate(), 12U, 128U, 64U, scaled_game, &rgba8) ==
            MesaFxSourceOutcome::kUsed &&
        rgba8[4U * (9U * 128U + 13U)] == Channel(13 / 4, 9 / 4, 0);

    // Every check that must fall back to the game's data.
    MesaFxGateState wrong_info = Gate();
    wrong_info.esi += 4U;
    MesaFxGateState wrong_data = Gate();
    wrong_data.data_address += 2U;
    std::vector<std::uint8_t> tampered = same_game;
    tampered[100] ^= 0x01U;
    FakeGuest denied;
    const auto denied_game = Setup(&denied, 64U, 32U, 64U, 32U, true);
    denied.deny_texture_object = true;
    const bool fallbacks =
        Run(&same, wrong_info, 12U, 64U, 32U, same_game, &rgba8) ==
            MesaFxSourceOutcome::kLinkMismatch &&
        rgba8.empty() &&
        Run(&same, wrong_data, 12U, 64U, 32U, same_game, &rgba8) ==
            MesaFxSourceOutcome::kDataMismatch &&
        Run(&same, Gate(), 12U, 64U, 32U, tampered, &rgba8) ==
            MesaFxSourceOutcome::kVerifyMismatch &&
        rgba8.empty() &&
        Run(&denied, Gate(), 12U, 64U, 32U, denied_game, &rgba8) ==
            MesaFxSourceOutcome::kUnreadable &&
        Run(&same, Gate(), 11U, 64U, 32U, same_game, &rgba8) ==
            MesaFxSourceOutcome::kNotApplicable;

    // An original that is not a whole fraction of the Glide size is left alone.
    FakeGuest odd;
    const auto odd_game = Setup(&odd, 64U, 32U, 64U, 32U, true);
    odd.Put32(kImage + 0xCU, 48U);
    const bool odd_size =
        Run(&odd, Gate(), 12U, 64U, 32U, odd_game, &rgba8) ==
        MesaFxSourceOutcome::kNotApplicable;

    const bool all = argb4444 && rgb565 && upscale && fallbacks && odd_size;
    std::cout << "mesa_fx_texture_source_4444=" << (argb4444 ? "true" : "false")
              << "\nmesa_fx_texture_source_565=" << (rgb565 ? "true" : "false")
              << "\nmesa_fx_texture_source_upscale="
              << (upscale ? "true" : "false")
              << "\nmesa_fx_texture_source_fallbacks="
              << (fallbacks ? "true" : "false")
              << "\nmesa_fx_texture_source_odd_size="
              << (odd_size ? "true" : "false")
              << "\nmesa_fx_texture_source_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
