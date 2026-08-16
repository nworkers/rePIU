#include "repiu/platform/win32/glide_texture_census.h"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace repiu::platform::win32
{
namespace
{

// Log2 bucket over the longer edge, so 1..4096 lands in twelve slots and an
// absurd dimension still has somewhere to go rather than corrupting a neighbour.
std::uint32_t DimensionBucket(std::uint32_t width, std::uint32_t height)
{
    std::uint32_t longer = width > height ? width : height;
    std::uint32_t bucket = 0;
    while (longer > 1U && bucket + 1U < kGlideTextureDimensionBuckets)
    {
        longer >>= 1U;
        ++bucket;
    }
    return bucket;
}

void WriteLittleEndian16(std::ofstream& stream, std::uint16_t value)
{
    const char bytes[2] = {static_cast<char>(value & 0xFFU),
                           static_cast<char>((value >> 8U) & 0xFFU)};
    stream.write(bytes, 2);
}

}  // namespace

std::uint64_t HashGlideTexturePixels(const std::uint8_t* rgba8,
                                     std::size_t byte_count)
{
    // FNV-1a: one pass, no table, and good enough to tell "same bytes" from
    // "different bytes", which is the only question asked of it.
    std::uint64_t hash = 1469598103934665603ULL;
    if (rgba8 == nullptr)
    {
        return 0;
    }
    for (std::size_t index = 0; index < byte_count; ++index)
    {
        hash ^= static_cast<std::uint64_t>(rgba8[index]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void RecordGlideTextureUpload(Win32GlideTextureCensus* census,
                              const Win32GlideTextureUpload& upload,
                              const std::uint8_t* rgba8,
                              std::size_t byte_count)
{
    if (census == nullptr)
    {
        return;
    }
    census->enabled = true;

    if (rgba8 == nullptr || byte_count == 0U)
    {
        ++census->decode_failure_count;
        census->last_failed_format = upload.format;
        return;
    }

    ++census->upload_count;
    census->decoded_byte_total += byte_count;

    const std::uint32_t format_bucket =
        upload.format < kGlideTextureFormatBuckets
            ? upload.format
            : kGlideTextureFormatBuckets - 1U;
    ++census->format_counts[format_bucket];
    ++census->dimension_counts[DimensionBucket(upload.width, upload.height)];

    if (upload.s_extent != upload.width || upload.t_extent != upload.height)
    {
        ++census->extent_mismatch_count;
    }
    // Format 5 is P_8 and format 14 is AP_88; both index a palette that arrives
    // through grTexDownloadTable. Without one they decode to wrong colours
    // rather than failing, so the combination is counted rather than inferred.
    if ((upload.format == 5U || upload.format == 14U) && !upload.has_palette)
    {
        ++census->palettized_without_palette_count;
    }

    const std::uint64_t hash = HashGlideTexturePixels(rgba8, byte_count);
    const auto existing = census->address_hashes.find(upload.start_address);
    if (existing == census->address_hashes.end())
    {
        ++census->distinct_address_count;
        census->address_hashes.emplace(upload.start_address, hash);
        return;
    }
    if (existing->second == hash)
    {
        ++census->identical_repeat_count;
        return;
    }
    ++census->changed_repeat_count;
    existing->second = hash;
}

void RecordGlidePaletteDownload(Win32GlideTextureCensus* census,
                                const bool identical)
{
    if (census == nullptr)
    {
        return;
    }
    census->enabled = true;
    ++census->palette_download_count;
    if (identical)
    {
        ++census->palette_identical_count;
    }
    else
    {
        ++census->palette_changed_count;
    }
}

void RecordGlidePaletteRefresh(Win32GlideTextureCensus* census,
                               const bool success,
                               const std::size_t source_bytes,
                               const std::size_t rgba_bytes,
                               const std::uint64_t decode_nanoseconds,
                               const std::uint64_t upload_nanoseconds)
{
    if (census == nullptr)
    {
        return;
    }
    census->enabled = true;
    census->palette_refresh_decode_nanoseconds += decode_nanoseconds;
    census->palette_refresh_upload_nanoseconds += upload_nanoseconds;
    if (!success)
    {
        ++census->palette_refresh_failure_count;
        return;
    }
    ++census->palette_refresh_count;
    census->palette_refresh_source_bytes += source_bytes;
    census->palette_refresh_rgba_bytes += rgba_bytes;
}

Win32GlideTextureCensusSnapshot SnapshotGlideTextureCensus(
    const Win32GlideTextureCensus& census)
{
    Win32GlideTextureCensusSnapshot snapshot;
    snapshot.enabled = census.enabled;
    snapshot.upload_count = census.upload_count;
    snapshot.distinct_address_count = census.distinct_address_count;
    snapshot.identical_repeat_count = census.identical_repeat_count;
    snapshot.changed_repeat_count = census.changed_repeat_count;
    snapshot.decode_failure_count = census.decode_failure_count;
    snapshot.last_failed_format = census.last_failed_format;
    snapshot.extent_mismatch_count = census.extent_mismatch_count;
    snapshot.palettized_without_palette_count =
        census.palettized_without_palette_count;
    snapshot.decoded_byte_total = census.decoded_byte_total;
    snapshot.palette_download_count = census.palette_download_count;
    snapshot.palette_identical_count = census.palette_identical_count;
    snapshot.palette_changed_count = census.palette_changed_count;
    snapshot.palette_refresh_count = census.palette_refresh_count;
    snapshot.palette_refresh_failure_count =
        census.palette_refresh_failure_count;
    snapshot.palette_refresh_source_bytes =
        census.palette_refresh_source_bytes;
    snapshot.palette_refresh_rgba_bytes = census.palette_refresh_rgba_bytes;
    snapshot.palette_refresh_decode_nanoseconds =
        census.palette_refresh_decode_nanoseconds;
    snapshot.palette_refresh_upload_nanoseconds =
        census.palette_refresh_upload_nanoseconds;
    snapshot.format_counts = census.format_counts;
    snapshot.dimension_counts = census.dimension_counts;
    snapshot.dump_written_count = census.dump_written_count;
    snapshot.dump_limit_reached = census.dump_limit_reached;
    return snapshot;
}

std::filesystem::path ResolveGlideTextureDumpDirectory(
    std::string_view setting)
{
    if (setting.empty() || setting == "0")
    {
        return {};
    }
    if (setting == "1")
    {
        return std::filesystem::path("build") / "texture_dumps";
    }
    return std::filesystem::path(std::string(setting));
}

std::filesystem::path GlideTextureDumpDirectory()
{
    static const std::filesystem::path directory = [] {
        const char* value = std::getenv("REPIU_GLIDE_TEX_DUMP");
        return ResolveGlideTextureDumpDirectory(value == nullptr ? "" : value);
    }();
    return directory;
}

std::uint32_t GlideTextureDumpLimit()
{
    static const std::uint32_t limit = [] {
        const char* value = std::getenv("REPIU_GLIDE_TEX_DUMP_LIMIT");
        if (value == nullptr || *value == '\0')
        {
            return kDefaultGlideTextureDumpLimit;
        }
        std::uint32_t parsed = 0;
        const char* end = value;
        while (*end != '\0')
        {
            ++end;
        }
        const auto result = std::from_chars(value, end, parsed);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            return kDefaultGlideTextureDumpLimit;
        }
        return parsed;
    }();
    return limit;
}

bool WriteGlideTextureDump(const std::filesystem::path& directory,
                           std::uint32_t sequence,
                           const Win32GlideTextureUpload& upload,
                           std::uint64_t content_hash,
                           const std::uint8_t* rgba8,
                           std::size_t byte_count)
{
    if (directory.empty() || rgba8 == nullptr || upload.width == 0U ||
        upload.height == 0U ||
        byte_count < static_cast<std::size_t>(upload.width) * upload.height * 4U)
    {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);

    char name[192] = {};
    std::snprintf(name, sizeof(name),
                  "tex_%04u_%08X_%ux%u_fmt%u_lod%u_ar%u.tga", sequence,
                  upload.start_address, upload.width, upload.height,
                  upload.format, upload.large_lod, upload.aspect_ratio);

    {
        std::ofstream stream(directory / name, std::ios::binary);
        if (!stream)
        {
            return false;
        }
        const char header_prefix[8] = {0, 0, 2, 0, 0, 0, 0, 0};
        stream.write(header_prefix, sizeof(header_prefix));
        WriteLittleEndian16(stream, 0);  // x origin
        WriteLittleEndian16(stream, 0);  // y origin
        WriteLittleEndian16(stream, static_cast<std::uint16_t>(upload.width));
        WriteLittleEndian16(stream, static_cast<std::uint16_t>(upload.height));
        // 32 bits per pixel, 8 alpha bits, and bit 5 set so row 0 is the top --
        // Glide addresses textures from the top left, and a flipped dump would
        // send the reader hunting a decode bug that is not there.
        const char trailer[2] = {32, 0x28};
        stream.write(trailer, sizeof(trailer));
        // TGA stores BGRA; the decoder produced RGBA.
        for (std::uint32_t index = 0; index < upload.width * upload.height;
             ++index)
        {
            const std::uint8_t* texel = rgba8 + static_cast<std::size_t>(index) * 4U;
            const char bgra[4] = {static_cast<char>(texel[2]),
                                  static_cast<char>(texel[1]),
                                  static_cast<char>(texel[0]),
                                  static_cast<char>(texel[3])};
            stream.write(bgra, 4);
        }
        if (!stream)
        {
            return false;
        }
    }

    const std::filesystem::path manifest_path = directory / "manifest.csv";
    const bool needs_header = !std::filesystem::exists(manifest_path, error);
    std::ofstream manifest(manifest_path, std::ios::app);
    if (!manifest)
    {
        return false;
    }
    if (needs_header)
    {
        manifest << "file,sequence,address,format,large_lod,aspect_ratio,"
                    "width,height,s_extent,t_extent,source_size,palette,hash\n";
    }
    manifest << name << ',' << sequence << ",0x" << std::hex
             << upload.start_address << std::dec << ',' << upload.format << ','
             << upload.large_lod << ',' << upload.aspect_ratio << ','
             << upload.width << ',' << upload.height << ',' << upload.s_extent
             << ',' << upload.t_extent << ',' << upload.source_size << ','
             << (upload.has_palette ? 1 : 0) << ",0x" << std::hex
             << content_hash << std::dec << '\n';
    return static_cast<bool>(manifest);
}

}  // namespace repiu::platform::win32
