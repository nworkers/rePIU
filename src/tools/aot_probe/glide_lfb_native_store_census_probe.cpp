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
        snapshot.max_overlapping_byte_count == 8U;

    GlideLfbNativeStoreCensusProfile malformed_profile;
    BeginGlideLfbNativeStoreCensus(&malformed_profile, 0U, 16U);
    BeginGlideLfbNativeStoreCensus(&malformed_profile, 0xFFFFFFF0U, 32U);
    const bool malformed =
        SnapshotGlideLfbNativeStoreCensus(malformed_profile).malformed_range_count == 2U;

    const bool all = policy && aggregation && malformed;
    std::cout << "glide_lfb_native_store_census_policy="
              << (policy ? "true" : "false")
              << "\nglide_lfb_native_store_census_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_lfb_native_store_census_malformed="
              << (malformed ? "true" : "false")
              << "\nglide_lfb_native_store_census_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
