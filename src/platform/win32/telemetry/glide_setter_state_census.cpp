#include "repiu/platform/win32/glide_setter_state_census.h"

#include <algorithm>
#include <cstdlib>

namespace repiu::platform::win32
{
namespace
{

bool ReadGlideSetterCensusSetting()
{
    const char* value = std::getenv("REPIU_GLIDE_SETTER_CENSUS");
    return value != nullptr && ResolveGlideSetterCensusEnabled(value);
}

Win32GlideSetterCensusEntry* FindEntry(
    Win32GlideSetterCensusProfile* profile,
    std::uint16_t ordinal)
{
    if (profile == nullptr)
    {
        return nullptr;
    }
    profile->enabled = true;
    if (ordinal >= profile->entries.size())
    {
        ++profile->ordinal_overflow_count;
        return nullptr;
    }
    return &profile->entries[ordinal];
}

void TrackDistinctKey(Win32GlideSetterCensusEntry* entry,
                      const Win32GlideSetterStateKey& key)
{
    for (std::uint32_t index = 0; index < entry->distinct_key_count; ++index)
    {
        if (GlideSetterStateKeysEqual(entry->distinct_keys[index], key))
        {
            return;
        }
    }
    if (entry->distinct_key_count >= entry->distinct_keys.size())
    {
        ++entry->distinct_overflow_count;
        return;
    }
    entry->distinct_keys[entry->distinct_key_count] = key;
    ++entry->distinct_key_count;
}

}  // namespace

bool ResolveGlideSetterCensusEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideSetterCensusEnabled()
{
    static const bool enabled = ReadGlideSetterCensusSetting();
    return enabled;
}

void RecordGlideSetterCensusCall(
    Win32GlideSetterCensusProfile* profile,
    std::uint16_t ordinal,
    const Win32GlideSetterStateKey& key,
    Win32GlideSetterCensusOutcome outcome)
{
    Win32GlideSetterCensusEntry* entry = FindEntry(profile, ordinal);
    if (entry == nullptr)
    {
        return;
    }

    ++entry->call_count;
    ++entry->frame_call_count;
    TrackDistinctKey(entry, key);

    // Classification order matters: a failed or unsupported call tells us
    // nothing about host state, so it can neither count as a repeat nor become
    // the applied record.
    if (outcome == Win32GlideSetterCensusOutcome::kFailed)
    {
        ++entry->failure_count;
        entry->applied_valid = false;
        entry->current_repeat_run = 0;
        return;
    }
    if (outcome == Win32GlideSetterCensusOutcome::kUnsupported)
    {
        ++entry->unsupported_count;
        entry->applied_valid = false;
        entry->current_repeat_run = 0;
        return;
    }

    if (!entry->applied_valid)
    {
        ++entry->first_count;
        ++entry->frame_change_count;
        entry->current_repeat_run = 0;
        entry->applied_key = key;
        entry->applied_valid = true;
        return;
    }
    if (GlideSetterStateKeysEqual(entry->applied_key, key))
    {
        ++entry->same_count;
        ++entry->current_repeat_run;
        entry->max_repeat_run =
            std::max(entry->max_repeat_run, entry->current_repeat_run);
        return;
    }
    ++entry->changed_count;
    ++entry->frame_change_count;
    entry->current_repeat_run = 0;
    entry->applied_key = key;
}

void RecordGlideSetterCensusKeyOverflow(
    Win32GlideSetterCensusProfile* profile,
    std::uint16_t ordinal)
{
    Win32GlideSetterCensusEntry* entry = FindEntry(profile, ordinal);
    if (entry == nullptr)
    {
        return;
    }
    ++entry->key_overflow_count;
}

void RecordGlideSetterCensusInvalidation(
    Win32GlideSetterCensusProfile* profile)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->invalidation_count;
    for (Win32GlideSetterCensusEntry& entry : profile->entries)
    {
        entry.applied_valid = false;
        entry.current_repeat_run = 0;
    }
}

void RecordGlideSetterCensusTextureGeneration(
    Win32GlideSetterCensusProfile* profile)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    // Bumping the counter is the whole invalidation: a texture-state key built
    // after the download carries the new generation, so it cannot compare equal
    // to a record taken against the previous one. Applied records are left
    // alone deliberately — rewriting their generation would preserve equality,
    // which is the opposite of what a download means.
    ++profile->texture_generation;
}

void RecordGlideSetterCensusFrameBoundary(
    Win32GlideSetterCensusProfile* profile)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->frame_count;
    for (Win32GlideSetterCensusEntry& entry : profile->entries)
    {
        entry.max_frame_call_count =
            std::max(entry.max_frame_call_count, entry.frame_call_count);
        entry.max_frame_change_count =
            std::max(entry.max_frame_change_count, entry.frame_change_count);
        entry.frame_call_count = 0;
        entry.frame_change_count = 0;
    }
}

Win32GlideSetterCensusSnapshot SnapshotGlideSetterCensus(
    const Win32GlideSetterCensusProfile& profile)
{
    Win32GlideSetterCensusSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.ordinal_overflow_count = profile.ordinal_overflow_count;
    snapshot.invalidation_count = profile.invalidation_count;
    snapshot.frame_count = profile.frame_count;
    snapshot.texture_generation = profile.texture_generation;
    for (const Win32GlideSetterCensusEntry& entry : profile.entries)
    {
        if (entry.call_count == 0U && entry.key_overflow_count == 0U)
        {
            continue;
        }
        ++snapshot.active_entry_count;
        snapshot.call_count += entry.call_count;
        snapshot.first_count += entry.first_count;
        snapshot.same_count += entry.same_count;
        snapshot.changed_count += entry.changed_count;
        snapshot.failure_count += entry.failure_count;
        snapshot.unsupported_count += entry.unsupported_count;
        snapshot.key_overflow_count += entry.key_overflow_count;
        snapshot.distinct_overflow_count += entry.distinct_overflow_count;
    }
    return snapshot;
}

}  // namespace repiu::platform::win32
