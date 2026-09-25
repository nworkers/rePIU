#include "repiu/engine/glide_lfb_native_store_census.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>

namespace repiu::engine
{
namespace
{

std::atomic<std::uint32_t> g_glide_lfb_native_store_census_active = 0U;

struct GlideLfbNativeStoreSourceState
{
    const GlideLfbNativeStoreCensusProfile* owner = nullptr;
    std::array<GlideLfbNativeStoreCensusSourceEntry,
               kGlideLfbNativeStoreCensusSourceCapacity>
        entries = {};
    std::uint64_t sample_count = 0;
    std::uint64_t overflow_sample_count = 0;
    std::uint64_t overflow_sampled_byte_count = 0;
};

GlideLfbNativeStoreSourceState g_glide_lfb_native_store_source_state;

bool IsValidRange(const std::uint32_t base, const std::uint32_t byte_count)
{
    return base != 0U && byte_count != 0U &&
        static_cast<std::uint64_t>(base) + byte_count <=
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
}

void ClearRange(GlideLfbNativeStoreCensusProfile* const profile)
{
    profile->range_active = false;
    profile->range_base = 0U;
    profile->range_byte_count = 0U;
}

void UpdateSourceEntry(GlideLfbNativeStoreCensusSourceEntry* const entry,
                       const std::uint32_t overlap)
{
    ++entry->sample_count;
    entry->sampled_byte_count += overlap;
    entry->max_sampled_byte_count = std::max(
        entry->max_sampled_byte_count, overlap);
}

void RecordSourceOverlap(GlideLfbNativeStoreCensusProfile* const profile,
                         const std::uint32_t guest_eip,
                         const std::uint32_t overlap)
{
    GlideLfbNativeStoreSourceState& state =
        g_glide_lfb_native_store_source_state;
    if (state.owner != profile)
    {
        state = GlideLfbNativeStoreSourceState{};
        state.owner = profile;
    }
    ++state.sample_count;
    std::size_t empty_slot = state.entries.size();
    for (GlideLfbNativeStoreCensusSourceEntry& entry : state.entries)
    {
        if (entry.sample_count == 0U)
        {
            if (empty_slot == state.entries.size())
            {
                empty_slot = static_cast<std::size_t>(&entry -
                    state.entries.data());
            }
            continue;
        }
        if (entry.guest_eip != guest_eip)
        {
            continue;
        }
        UpdateSourceEntry(&entry, overlap);
        return;
    }
    if (empty_slot < state.entries.size())
    {
        GlideLfbNativeStoreCensusSourceEntry& entry =
            state.entries[empty_slot];
        entry.guest_eip = guest_eip;
        entry.sample_count = 1U;
        entry.sampled_byte_count = overlap;
        entry.max_sampled_byte_count = overlap;
        return;
    }
    ++state.overflow_sample_count;
    state.overflow_sampled_byte_count += overlap;
}

}  // namespace

bool ResolveGlideLfbNativeStoreCensusEnabled(const std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideLfbNativeStoreCensusEnabled()
{
    static const bool aggregate_enabled = [] {
        const char* const value = std::getenv("REPIU_LINUX_X64_LFB_STORE_CENSUS");
        return value != nullptr && ResolveGlideLfbNativeStoreCensusEnabled(value);
    }();
    return aggregate_enabled || GlideLfbNativeStoreSourceCensusEnabled();
}

bool GlideLfbNativeStoreSourceCensusEnabled()
{
    static const bool enabled = [] {
        const char* const value =
            std::getenv("REPIU_LINUX_X64_LFB_STORE_SOURCE_CENSUS");
        return value != nullptr && ResolveGlideLfbNativeStoreCensusEnabled(value);
    }();
    return enabled;
}

std::uintptr_t GlideLfbNativeStoreCensusActiveFlagAddress()
{
    return reinterpret_cast<std::uintptr_t>(
        &g_glide_lfb_native_store_census_active);
}

void BeginGlideLfbNativeStoreCensus(
    GlideLfbNativeStoreCensusProfile* const profile,
    const std::uint32_t range_base,
    const std::uint32_t range_byte_count)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->write_lock_count;
    ClearRange(profile);
    g_glide_lfb_native_store_census_active.store(0U, std::memory_order_release);
    if (!IsValidRange(range_base, range_byte_count))
    {
        ++profile->malformed_range_count;
        return;
    }
    profile->range_base = range_base;
    profile->range_byte_count = range_byte_count;
    profile->range_active = true;
    g_glide_lfb_native_store_census_active.store(1U, std::memory_order_release);
}

void EndGlideLfbNativeStoreCensus(GlideLfbNativeStoreCensusProfile* const profile)
{
    if (profile == nullptr || !profile->range_active)
    {
        return;
    }
    ++profile->completed_write_lock_count;
    ClearRange(profile);
    g_glide_lfb_native_store_census_active.store(0U, std::memory_order_release);
}

void RecordGlideLfbNativeStoreCensus(
    GlideLfbNativeStoreCensusProfile* const profile,
    const bool decoded,
    const std::uint32_t destination,
    const std::uint32_t byte_count)
{
    if (profile == nullptr || !profile->enabled)
    {
        return;
    }
    ++profile->observer_call_count;
    if (!decoded)
    {
        return;
    }
    ++profile->decoded_store_count;
    if (!profile->range_active)
    {
        return;
    }
    ++profile->active_decoded_store_count;
    if (byte_count == 0U)
    {
        return;
    }
    const std::uint64_t store_begin = destination;
    const std::uint64_t store_end = store_begin + byte_count;
    const std::uint64_t range_begin = profile->range_base;
    const std::uint64_t range_end = range_begin + profile->range_byte_count;
    const std::uint64_t overlap_begin = std::max(store_begin, range_begin);
    const std::uint64_t overlap_end = std::min(store_end, range_end);
    if (overlap_begin >= overlap_end)
    {
        return;
    }
    const std::uint64_t overlap = overlap_end - overlap_begin;
    ++profile->overlapping_store_count;
    profile->overlapping_byte_count += overlap;
    profile->max_overlapping_byte_count = std::max(
        profile->max_overlapping_byte_count,
        static_cast<std::uint32_t>(overlap));
}

void RecordGlideLfbNativeStoreCensusSource(
    GlideLfbNativeStoreCensusProfile* const profile,
    const bool decoded,
    const std::uint32_t guest_eip,
    const std::uint32_t destination,
    const std::uint32_t byte_count)
{
    if (profile == nullptr || !profile->enabled || !decoded ||
        !profile->range_active || byte_count == 0U ||
        (profile->overlapping_store_count %
         kGlideLfbNativeStoreCensusSourceSampleStride) != 0U)
    {
        return;
    }
    const std::uint64_t store_begin = destination;
    const std::uint64_t store_end = store_begin + byte_count;
    const std::uint64_t range_begin = profile->range_base;
    const std::uint64_t range_end = range_begin + profile->range_byte_count;
    const std::uint64_t overlap_begin = std::max(store_begin, range_begin);
    const std::uint64_t overlap_end = std::min(store_end, range_end);
    if (overlap_begin < overlap_end)
    {
        RecordSourceOverlap(profile, guest_eip,
                            static_cast<std::uint32_t>(
                                overlap_end - overlap_begin));
    }
}

GlideLfbNativeStoreCensusSnapshot SnapshotGlideLfbNativeStoreCensus(
    const GlideLfbNativeStoreCensusProfile& profile)
{
    GlideLfbNativeStoreCensusSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.write_lock_count = profile.write_lock_count;
    snapshot.completed_write_lock_count = profile.completed_write_lock_count;
    snapshot.malformed_range_count = profile.malformed_range_count;
    snapshot.observer_call_count = profile.observer_call_count;
    snapshot.decoded_store_count = profile.decoded_store_count;
    snapshot.active_decoded_store_count = profile.active_decoded_store_count;
    snapshot.overlapping_store_count = profile.overlapping_store_count;
    snapshot.overlapping_byte_count = profile.overlapping_byte_count;
    snapshot.max_overlapping_byte_count = profile.max_overlapping_byte_count;
    const GlideLfbNativeStoreSourceState& source_state =
        g_glide_lfb_native_store_source_state;
    if (source_state.owner == &profile)
    {
        snapshot.source_entries = source_state.entries;
        snapshot.source_sample_count = source_state.sample_count;
        snapshot.source_overflow_sample_count = source_state.overflow_sample_count;
        snapshot.source_overflow_sampled_byte_count =
            source_state.overflow_sampled_byte_count;
    }
    return snapshot;
}

void RankGlideLfbNativeStoreCensusSources(
    const GlideLfbNativeStoreCensusSourceEntry* const entries,
    const std::size_t entry_count,
    GlideLfbNativeStoreCensusSourceEntry* const ranks,
    const std::size_t rank_capacity)
{
    if (ranks == nullptr || rank_capacity == 0U)
    {
        return;
    }
    for (std::size_t slot = 0U; slot < rank_capacity; ++slot)
    {
        ranks[slot] = GlideLfbNativeStoreCensusSourceEntry{};
    }
    if (entries == nullptr)
    {
        return;
    }
    for (std::size_t index = 0U; index < entry_count; ++index)
    {
        const GlideLfbNativeStoreCensusSourceEntry& entry = entries[index];
        if (entry.sample_count == 0U)
        {
            continue;
        }
        for (std::size_t slot = 0U; slot < rank_capacity; ++slot)
        {
            const GlideLfbNativeStoreCensusSourceEntry& current = ranks[slot];
            const bool ranks_ahead =
                entry.sample_count > current.sample_count ||
                (current.sample_count != 0U &&
                 entry.sample_count == current.sample_count &&
                 entry.guest_eip < current.guest_eip);
            if (!ranks_ahead)
            {
                continue;
            }
            for (std::size_t shift = rank_capacity - 1U; shift > slot; --shift)
            {
                ranks[shift] = ranks[shift - 1U];
            }
            ranks[slot] = entry;
            break;
        }
    }
}

}  // namespace repiu::engine
