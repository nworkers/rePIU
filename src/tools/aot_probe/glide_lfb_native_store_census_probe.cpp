#include "glide_lfb_native_store_census_probe.h"

#include "repiu/engine/glide_lfb_native_store_census.h"

#include <iostream>

namespace repiu::tools
{

bool RunGlideLfbNativeStoreCensusProbe()
{
    using engine::BeginGlideLfbNativeStoreCensus;
    using engine::EndGlideLfbNativeStoreCensus;
    using engine::GlideLfbNativeStoreCensusProfile;
    using engine::GlideLfbNativeStoreCensusSourceEntry;
    using engine::RankGlideLfbNativeStoreCensusSources;
    using engine::RecordGlideLfbNativeStoreCensus;
    using engine::SnapshotGlideLfbNativeStoreCensus;

    const bool policy =
        !engine::ResolveGlideLfbNativeStoreCensusEnabled("") &&
        !engine::ResolveGlideLfbNativeStoreCensusEnabled("0") &&
        engine::ResolveGlideLfbNativeStoreCensusEnabled("1") &&
        engine::ResolveGlideLfbNativeStoreCensusEnabled("on") &&
        engine::ResolveGlideLfbNativeStoreCensusEnabled("true");

    GlideLfbNativeStoreCensusProfile profile;
    BeginGlideLfbNativeStoreCensus(&profile, 0x1000U, 16U);
    RecordGlideLfbNativeStoreCensus(&profile, false, 0U, 0U);
    RecordGlideLfbNativeStoreCensus(&profile, true, 0x0FFEU, 4U);
    RecordGlideLfbNativeStoreCensus(&profile, true, 0x1004U, 8U);
    RecordGlideLfbNativeStoreCensus(&profile, true, 0x1010U, 4U);
    EndGlideLfbNativeStoreCensus(&profile);
    RecordGlideLfbNativeStoreCensus(&profile, true, 0x1000U, 4U);
    const auto snapshot = SnapshotGlideLfbNativeStoreCensus(profile);
    const bool aggregation =
        snapshot.enabled &&
        snapshot.write_lock_count == 1U &&
        snapshot.completed_write_lock_count == 1U &&
        snapshot.malformed_range_count == 0U &&
        snapshot.observer_call_count == 5U &&
        snapshot.decoded_store_count == 4U &&
        snapshot.active_decoded_store_count == 3U &&
        snapshot.overlapping_store_count == 2U &&
        snapshot.overlapping_byte_count == 10U &&
        snapshot.max_overlapping_byte_count == 8U &&
        snapshot.source_sample_count == 0U;

    GlideLfbNativeStoreCensusProfile source_profile;
    BeginGlideLfbNativeStoreCensus(&source_profile, 0x1000U, 4U);
    for (std::uint64_t index = 1U;
         index <= 2U * engine::kGlideLfbNativeStoreCensusSourceSampleStride;
         ++index)
    {
        const std::uint32_t eip =
            index == engine::kGlideLfbNativeStoreCensusSourceSampleStride
                ? 0x3000U
                : index == 2U * engine::kGlideLfbNativeStoreCensusSourceSampleStride
                    ? 0x2000U
                    : 0x5000U;
        RecordGlideLfbNativeStoreCensus(
            &source_profile, true, 0x1000U, 4U);
        engine::RecordGlideLfbNativeStoreCensusSource(
            &source_profile, true, eip, 0x1000U, 4U);
    }
    const auto source_snapshot = SnapshotGlideLfbNativeStoreCensus(source_profile);

    GlideLfbNativeStoreCensusSourceEntry ranks[2] = {};
    RankGlideLfbNativeStoreCensusSources(
        source_snapshot.source_entries.data(),
        source_snapshot.source_entries.size(), ranks, 2U);
    const bool ranking =
        source_snapshot.source_sample_count == 2U &&
        ranks[0].guest_eip == 0x2000U &&
        ranks[0].sample_count == 1U &&
        ranks[1].guest_eip == 0x3000U &&
        ranks[1].sample_count == 1U;

    GlideLfbNativeStoreCensusProfile malformed_profile;
    BeginGlideLfbNativeStoreCensus(&malformed_profile, 0U, 16U);
    BeginGlideLfbNativeStoreCensus(&malformed_profile, 0xFFFFFFF0U, 32U);
    const bool malformed =
        SnapshotGlideLfbNativeStoreCensus(malformed_profile).malformed_range_count == 2U;

    GlideLfbNativeStoreCensusProfile overflow_profile;
    BeginGlideLfbNativeStoreCensus(&overflow_profile, 0x1000U, 4U);
    for (std::uint64_t index = 1U;
         index <= (engine::kGlideLfbNativeStoreCensusSourceCapacity + 1U) *
             engine::kGlideLfbNativeStoreCensusSourceSampleStride;
         ++index)
    {
        const std::uint32_t eip =
            (index % engine::kGlideLfbNativeStoreCensusSourceSampleStride) == 0U
                ? 0x8000U + static_cast<std::uint32_t>(
                    index / engine::kGlideLfbNativeStoreCensusSourceSampleStride)
                : 0x7000U;
        RecordGlideLfbNativeStoreCensus(
            &overflow_profile, true, 0x1000U, 4U);
        engine::RecordGlideLfbNativeStoreCensusSource(
            &overflow_profile, true, eip, 0x1000U, 4U);
    }
    const auto overflow = SnapshotGlideLfbNativeStoreCensus(overflow_profile);
    const bool source_overflow =
        overflow.source_sample_count ==
            engine::kGlideLfbNativeStoreCensusSourceCapacity + 1U &&
        overflow.source_overflow_sample_count == 1U &&
        overflow.source_overflow_sampled_byte_count == 4U;

    const bool all = policy && aggregation && ranking && malformed && source_overflow;
    std::cout << "glide_lfb_native_store_census_policy="
              << (policy ? "true" : "false")
              << "\nglide_lfb_native_store_census_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_lfb_native_store_census_ranking="
              << (ranking ? "true" : "false")
              << "\nglide_lfb_native_store_census_malformed="
              << (malformed ? "true" : "false")
              << "\nglide_lfb_native_store_census_source_overflow="
              << (source_overflow ? "true" : "false")
              << "\nglide_lfb_native_store_census_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
