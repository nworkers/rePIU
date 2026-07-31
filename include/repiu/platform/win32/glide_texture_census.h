#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace repiu::platform::win32
{

// Task 375: what the log could say about textures before this was upload counts,
// a byte total, and the first sixteen uploads reduced to one texel each. Neither
// attributes nor pixels were inspectable, so a suspicion about textures could not
// be settled either way. The census answers the attribute half on every run; the
// dump answers the pixel half on request.
constexpr std::uint32_t kGlideTextureFormatBuckets = 16U;
constexpr std::uint32_t kGlideTextureDimensionBuckets = 12U;
constexpr std::uint32_t kDefaultGlideTextureDumpLimit = 512U;

struct Win32GlideTextureCensus
{
    bool enabled = false;
    std::uint32_t upload_count = 0;
    std::uint32_t distinct_address_count = 0;
    // The number that decides whether an upload cache is worth building: a repeat
    // whose content hash is unchanged is work the host did for nothing.
    std::uint32_t identical_repeat_count = 0;
    std::uint32_t changed_repeat_count = 0;
    std::uint32_t decode_failure_count = 0;
    std::uint32_t last_failed_format = 0;
    // Task 332's hazard: Glide spans 256 along the longer axis whatever the LOD,
    // so an extent equal to the pixel size is the exception rather than the rule.
    // Counted rather than warned about, because it is normal in isolation and
    // only interesting in bulk.
    std::uint32_t extent_mismatch_count = 0;
    // P_8 and AP_88 index a palette downloaded separately by grTexDownloadTable.
    // An upload of either arriving without one decodes to the wrong colours
    // rather than failing, so it would otherwise be invisible.
    std::uint32_t palettized_without_palette_count = 0;
    std::uint64_t decoded_byte_total = 0;
    std::array<std::uint32_t, kGlideTextureFormatBuckets> format_counts = {};
    std::array<std::uint32_t, kGlideTextureDimensionBuckets>
        dimension_counts = {};
    std::uint32_t dump_written_count = 0;
    bool dump_limit_reached = false;
    // Last content hash per guest address. Not reported; it exists so a repeat
    // can be classified.
    std::unordered_map<std::uint32_t, std::uint64_t> address_hashes;
};

struct Win32GlideTextureCensusSnapshot
{
    bool enabled = false;
    std::uint32_t upload_count = 0;
    std::uint32_t distinct_address_count = 0;
    std::uint32_t identical_repeat_count = 0;
    std::uint32_t changed_repeat_count = 0;
    std::uint32_t decode_failure_count = 0;
    std::uint32_t last_failed_format = 0;
    std::uint32_t extent_mismatch_count = 0;
    std::uint32_t palettized_without_palette_count = 0;
    std::uint64_t decoded_byte_total = 0;
    std::array<std::uint32_t, kGlideTextureFormatBuckets> format_counts = {};
    std::array<std::uint32_t, kGlideTextureDimensionBuckets>
        dimension_counts = {};
    std::uint32_t dump_written_count = 0;
    bool dump_limit_reached = false;
};

struct Win32GlideTextureUpload
{
    std::uint32_t start_address = 0;
    std::uint32_t format = 0;
    std::uint32_t large_lod = 0;
    std::uint32_t aspect_ratio = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t s_extent = 0;
    std::uint32_t t_extent = 0;
    std::uint32_t source_size = 0;
    bool has_palette = false;
};

// Content hash over the decoded pixels. Only used to tell a wasteful repeat from
// a real one, so speed matters more than cryptographic strength.
std::uint64_t HashGlideTexturePixels(const std::uint8_t* rgba8,
                                     std::size_t byte_count);

// `rgba8` may be null, which records the upload as a decode failure -- a silently
// dropped texture is exactly what would not otherwise show up anywhere.
void RecordGlideTextureUpload(Win32GlideTextureCensus* census,
                              const Win32GlideTextureUpload& upload,
                              const std::uint8_t* rgba8,
                              std::size_t byte_count);

Win32GlideTextureCensusSnapshot SnapshotGlideTextureCensus(
    const Win32GlideTextureCensus& census);

// Empty when dumping is off. "1" selects build/texture_dumps; anything else is
// taken as the directory itself.
std::filesystem::path ResolveGlideTextureDumpDirectory(
    std::string_view setting);
std::filesystem::path GlideTextureDumpDirectory();
std::uint32_t GlideTextureDumpLimit();

// Writes one uncompressed 32-bit TGA plus a manifest row. Top-origin, because
// Glide addresses textures from the top left and a flipped dump would send the
// reader hunting a decode bug that is not there.
bool WriteGlideTextureDump(const std::filesystem::path& directory,
                           std::uint32_t sequence,
                           const Win32GlideTextureUpload& upload,
                           std::uint64_t content_hash,
                           const std::uint8_t* rgba8,
                           std::size_t byte_count);

}  // namespace repiu::platform::win32
