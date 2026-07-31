#include "glide_texture_census_probe.h"

#include "repiu/platform/win32/glide_texture_census.h"

#include <iostream>
#include <vector>

namespace repiu::tools
{

bool RunGlideTextureCensusProbe()
{
    using platform::win32::HashGlideTexturePixels;
    using platform::win32::RecordGlideTextureUpload;
    using platform::win32::ResolveGlideTextureDumpDirectory;
    using platform::win32::SnapshotGlideTextureCensus;
    using platform::win32::Win32GlideTextureCensus;
    using platform::win32::Win32GlideTextureUpload;

    Win32GlideTextureUpload upload;
    upload.start_address = 0x1000U;
    upload.format = 5U;
    upload.large_lod = 8U;
    upload.aspect_ratio = 3U;
    upload.width = 64U;
    upload.height = 64U;
    upload.s_extent = 256U;
    upload.t_extent = 256U;
    upload.source_size = 8192U;

    const std::vector<std::uint8_t> pixels_a(64U * 64U * 4U, 0x40U);
    std::vector<std::uint8_t> pixels_b(64U * 64U * 4U, 0x40U);
    pixels_b[7] = 0xFFU;

    Win32GlideTextureCensus census;
    // First upload is distinct; the same bytes again are a wasted repeat; changed
    // bytes are a real one. That three-way split is what the census exists for.
    RecordGlideTextureUpload(&census, upload, pixels_a.data(), pixels_a.size());
    RecordGlideTextureUpload(&census, upload, pixels_a.data(), pixels_a.size());
    RecordGlideTextureUpload(&census, upload, pixels_b.data(), pixels_b.size());
    // A changed repeat must update the stored hash, so re-uploading the new
    // content counts as identical rather than changed again.
    RecordGlideTextureUpload(&census, upload, pixels_b.data(), pixels_b.size());

    Win32GlideTextureUpload other = upload;
    other.start_address = 0x2000U;
    RecordGlideTextureUpload(&census, other, pixels_a.data(), pixels_a.size());

    const auto snapshot = SnapshotGlideTextureCensus(census);
    const bool classified =
        snapshot.upload_count == 5U &&
        snapshot.distinct_address_count == 2U &&
        snapshot.identical_repeat_count == 2U &&
        snapshot.changed_repeat_count == 1U;

    // A decode failure must be recorded rather than dropped, and must not be
    // counted as an upload -- nothing decoded.
    Win32GlideTextureUpload failing = upload;
    failing.format = 11U;
    RecordGlideTextureUpload(&census, failing, nullptr, 0U);
    const auto after_failure = SnapshotGlideTextureCensus(census);
    const bool failure_recorded =
        after_failure.decode_failure_count == 1U &&
        after_failure.last_failed_format == 11U &&
        after_failure.upload_count == 5U;

    // Glide spans 256 along the longer axis whatever the LOD, so a 64x64 texture
    // addressed 0..256 is an extent mismatch by definition.
    const bool extent_counted = after_failure.extent_mismatch_count == 5U;

    const bool byte_total =
        after_failure.decoded_byte_total ==
        static_cast<std::uint64_t>(pixels_a.size()) * 5U;

    // Format and dimension histograms must land in the right buckets: format 5,
    // and a longer edge of 64 in the log2 bucket 6.
    const bool histograms = after_failure.format_counts[5] == 5U &&
        after_failure.dimension_counts[6] == 5U;

    // A palettized format arriving without a palette decodes to wrong colours
    // rather than failing, so it is counted rather than left to inference.
    Win32GlideTextureCensus palette_census;
    Win32GlideTextureUpload palettized = upload;
    palettized.format = 5U;
    palettized.has_palette = false;
    RecordGlideTextureUpload(&palette_census, palettized, pixels_a.data(),
                             pixels_a.size());
    palettized.start_address = 0x3000U;
    palettized.has_palette = true;
    RecordGlideTextureUpload(&palette_census, palettized, pixels_a.data(),
                             pixels_a.size());
    const bool palette_counted =
        SnapshotGlideTextureCensus(palette_census)
            .palettized_without_palette_count == 1U;

    const bool hashing =
        HashGlideTexturePixels(pixels_a.data(), pixels_a.size()) !=
            HashGlideTexturePixels(pixels_b.data(), pixels_b.size()) &&
        HashGlideTexturePixels(nullptr, 16U) == 0U;

    const bool dump_setting =
        ResolveGlideTextureDumpDirectory("").empty() &&
        ResolveGlideTextureDumpDirectory("0").empty() &&
        !ResolveGlideTextureDumpDirectory("1").empty() &&
        ResolveGlideTextureDumpDirectory("D:\\tex").string() == "D:\\tex";

    Win32GlideTextureCensus untouched;
    RecordGlideTextureUpload(nullptr, upload, pixels_a.data(),
                             pixels_a.size());
    const bool inert =
        SnapshotGlideTextureCensus(untouched).upload_count == 0U &&
        !SnapshotGlideTextureCensus(untouched).enabled;

    const bool all = classified && failure_recorded && extent_counted &&
        byte_total && histograms && palette_counted && hashing &&
        dump_setting && inert;
    std::cout << "glide_texture_census_classified="
              << (classified ? "true" : "false")
              << "\nglide_texture_census_failure_recorded="
              << (failure_recorded ? "true" : "false")
              << "\nglide_texture_census_extent_counted="
              << (extent_counted ? "true" : "false")
              << "\nglide_texture_census_byte_total="
              << (byte_total ? "true" : "false")
              << "\nglide_texture_census_histograms="
              << (histograms ? "true" : "false")
              << "\nglide_texture_census_palette_counted="
              << (palette_counted ? "true" : "false")
              << "\nglide_texture_census_hashing="
              << (hashing ? "true" : "false")
              << "\nglide_texture_census_dump_setting="
              << (dump_setting ? "true" : "false")
              << "\nglide_texture_census_inert=" << (inert ? "true" : "false")
              << "\nglide_texture_census_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
