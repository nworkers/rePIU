#ifndef REPIU_HLE_MESA_FX_TEXTURE_SOURCE_H_
#define REPIU_HLE_MESA_FX_TEXTURE_SOURCE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace repiu::hle
{

// Issue #37. PIU draws through OpenGL with Mesa 3.x's 3dfx driver linked in,
// and that driver cuts every RGBA texture to ARGB4444 (RGB to RGB565) before
// grTexDownloadMipMapLevel, keeping the original 8-bit image in Mesa's own
// texture object. This finds that original from the gate's registers and
// rebuilds the texture at full precision -- reading guest memory only.
//
// The layout is the one all 19 ROM sets share
// (docs/analysis/mesa-fx-texture-path.md): at the call inside Mesa's upload
// function, ESI is the fx texture info, EBP the Mesa level, and the caller's
// saved EBP at [ESP+0x34] the texture object. Every link is checked, and the
// result is used only if truncating it again the way Mesa does reproduces the
// game's data byte for byte.

enum class MesaFxSourceOutcome : std::uint8_t
{
    // The full-precision image was rebuilt and verified.
    kUsed,
    // Not a 4444 or 565 upload, or not a size this path handles.
    kNotApplicable,
    // A structure or the pixels could not be read.
    kUnreadable,
    // The saved register is not the texture object of this texture info.
    kLinkMismatch,
    // The level's converted data is not the buffer being uploaded.
    kDataMismatch,
    // The rebuilt image does not truncate back to the game's data.
    kVerifyMismatch,
    kCount,
};

constexpr std::uint32_t kMesaFxSourceOutcomeCount =
    static_cast<std::uint32_t>(MesaFxSourceOutcome::kCount);

const char* MesaFxSourceOutcomeName(MesaFxSourceOutcome outcome);

// Copies `size` bytes of guest memory at `address` into `out`, or returns
// false when the range is not readable.
using MesaFxGuestReader =
    std::function<bool(std::uint32_t address, void* out, std::size_t size)>;

struct MesaFxGateState
{
    std::uint32_t esi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t esp = 0;
    // The data argument of grTexDownloadMipMapLevel.
    std::uint32_t data_address = 0;
};

// `glide_data` is the game's converted level (`width * height` 16-bit texels)
// already read from guest memory. On kUsed, `rgba8` holds `width * height`
// RGBA texels; otherwise it is left empty.
MesaFxSourceOutcome BuildMesaFxFullPrecisionTexture(
    const MesaFxGuestReader& read, const MesaFxGateState& gate,
    std::uint32_t glide_format, std::uint32_t width, std::uint32_t height,
    const std::uint8_t* glide_data, std::size_t glide_data_size,
    std::vector<std::uint8_t>* rgba8);

}  // namespace repiu::hle

#endif  // REPIU_HLE_MESA_FX_TEXTURE_SOURCE_H_
