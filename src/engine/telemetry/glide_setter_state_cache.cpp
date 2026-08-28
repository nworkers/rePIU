#include "repiu/engine/glide_setter_state_cache.h"

#include "repiu/runtime/env_toggle.h"

#include <cstdlib>

namespace repiu::engine
{
namespace
{

using go = repiu::hle::GlideGateId;

bool ReadGlideSetterElisionSetting()
{
    const char* value = std::getenv("REPIU_GLIDE_SETTER_ELIDE");
    // Absent means on: this is a default-enabled optimization with a kill switch,
    // not an opt-in experiment.
    return value == nullptr || ResolveGlideSetterElisionEnabled(value);
}

Win32GlideSetterStateCacheEntry* FindEntry(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal)
{
    if (cache == nullptr)
    {
        return nullptr;
    }
    cache->enabled = true;
    if (ordinal >= cache->entries.size())
    {
        ++cache->ordinal_overflow_count;
        return nullptr;
    }
    return &cache->entries[ordinal];
}

}  // namespace

bool ResolveGlideSetterElisionEnabled(std::string_view setting)
{
    return setting != "0" && setting != "off" && setting != "false";
}

bool GlideSetterElisionEnabled()
{
    static const bool enabled = ReadGlideSetterElisionSetting();
    return enabled;
}

bool GlideSetterTextureStateElisionEnabled()
{
    // Task 439 promoted this. A four-run A/B showed the three texture setters
    // are 99.76% redundant -- 385,197 newly covered calls produced 933
    // applications -- with zero voided entries and no visual difference. It also
    // enables Task 438's batching: an elided setter never reaches the gate
    // handler and so never forces a flush, which is what let batches reach 16
    // primitives. An explicit `0|off|false` opts out; the
    // `REPIU_GLIDE_SETTER_ELIDE=0` kill switch still wins over both, because the
    // boundary only consults this list when the cache exists.
    static const bool enabled = repiu::runtime::ResolvePromotedToggle(
        std::getenv("REPIU_GLIDE_SETTER_ELIDE_TEXTURE"));
    return enabled;
}

bool GlideSetterBatchThreeElisionEnabled()
{
    static const bool enabled = repiu::runtime::ResolvePromotedToggle(
        std::getenv("REPIU_GLIDE_SETTER_ELIDE_BATCH3"));
    return enabled;
}

bool GlideSetterBatchFourElisionEnabled()
{
    // Task 444 promoted this. Six gameplay runs kept the census `same` total
    // equal to the cache's `elided` count exactly, with zero voided entries,
    // zero implementation gaps and no visual difference, and `grDitherMode`
    // costing 95% less per call on non-overlapping distributions. `grFogTable`
    // is still excluded, for the pointer reason in the header.
    static const bool enabled = repiu::runtime::ResolvePromotedToggle(
        std::getenv("REPIU_GLIDE_SETTER_ELIDE_BATCH4"));
    return enabled;
}

bool IsGlideSetterBatchFourElisionGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        // One distinct value each, for the life of the process; see the header.
        case go::kGrFogColorValue:
        case go::kGrDitherMode:
            return true;
        default:
            return false;
    }
}

bool IsGlideSetterBatchThreeElisionGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        // Safe because only `startAddress` reaches the backend; see the header.
        case go::kGrTexSource:
        case go::kGrConstantColorValue:
        case go::kGrDepthMask:
            return true;
        default:
            return false;
    }
}

bool IsGlideSetterTextureStateElisionGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrTexClampMode:
        case go::kGrTexFilterMode:
        // Already an ABI-only no-op, so this one saves no rendezvous. It is
        // listed for a uniform account of the per-bind block rather than for a
        // saving, and the work log says so.
        case go::kGrTexMipMapMode:
            return true;
        default:
            return false;
    }
}

bool IsGlideSetterElisionGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrColorMask:
        case go::kGrAlphaBlendFunction:
        case go::kGrClipWindow:
        case go::kGrAlphaTestFunction:
        case go::kGrFogMode:
        case go::kGrCullMode:
        case go::kGrDepthBufferFunction:
            return true;
        // Deliberately excluded in batch one. `grTexSource` repeats only 32.24% of
        // the time; `grDepthMask` and `grConstantColorValue` change often enough to
        // deserve their own measurement; the combine setters route through shader
        // object state the LFB blit also touches; and the texture clamp/filter/
        // mipmap setters would validate texture-generation semantics in the same
        // batch as everything else.
        default:
            return false;
    }
}

bool ShouldElideGlideSetterState(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal,
    const Win32GlideSetterStateKey& key)
{
    if (cache == nullptr || ordinal >= cache->entries.size())
    {
        return false;
    }
    const Win32GlideSetterStateCacheEntry& entry = cache->entries[ordinal];
    return entry.applied_valid &&
        GlideSetterStateKeysEqual(entry.applied_key, key);
}

void RecordGlideSetterStateElided(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal)
{
    Win32GlideSetterStateCacheEntry* entry = FindEntry(cache, ordinal);
    if (entry == nullptr)
    {
        return;
    }
    ++entry->elided_count;
    ++cache->elided_count;
}

void RecordGlideSetterStateApplied(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal,
    const Win32GlideSetterStateKey& key)
{
    Win32GlideSetterStateCacheEntry* entry = FindEntry(cache, ordinal);
    if (entry == nullptr)
    {
        return;
    }
    entry->applied_key = key;
    entry->applied_valid = true;
    ++entry->applied_count;
    ++cache->applied_count;
}

void RecordGlideSetterStateVoided(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal)
{
    Win32GlideSetterStateCacheEntry* entry = FindEntry(cache, ordinal);
    if (entry == nullptr)
    {
        return;
    }
    entry->applied_valid = false;
    ++cache->voided_count;
}

void InvalidateGlideSetterStateCache(Win32GlideSetterStateCache* cache)
{
    if (cache == nullptr)
    {
        return;
    }
    cache->enabled = true;
    ++cache->invalidation_count;
    for (Win32GlideSetterStateCacheEntry& entry : cache->entries)
    {
        entry.applied_valid = false;
    }
}

void BumpGlideSetterStateCacheTextureGeneration(
    Win32GlideSetterStateCache* cache)
{
    if (cache == nullptr)
    {
        return;
    }
    cache->enabled = true;
    // Bumping the counter is the whole invalidation: a texture-dependent key built
    // after the download carries the new generation and cannot compare equal to a
    // record taken against the previous one. Applied records are left alone
    // deliberately -- rewriting their generation would preserve equality, the
    // opposite of what a download means.
    ++cache->texture_generation;
}

Win32GlideSetterStateCacheSnapshot SnapshotGlideSetterStateCache(
    const Win32GlideSetterStateCache& cache)
{
    Win32GlideSetterStateCacheSnapshot snapshot;
    snapshot.enabled = cache.enabled;
    // Read from the policy rather than the cache: the counters below were
    // produced under whichever batch this process is running, and a log that
    // does not say which one cannot be compared against its A/B partner.
    snapshot.texture_state = GlideSetterTextureStateElisionEnabled();
    snapshot.batch_three = GlideSetterBatchThreeElisionEnabled();
    snapshot.batch_four = GlideSetterBatchFourElisionEnabled();
    snapshot.texture_generation = cache.texture_generation;
    snapshot.elided_count = cache.elided_count;
    snapshot.applied_count = cache.applied_count;
    snapshot.voided_count = cache.voided_count;
    snapshot.invalidation_count = cache.invalidation_count;
    snapshot.ordinal_overflow_count = cache.ordinal_overflow_count;
    for (const Win32GlideSetterStateCacheEntry& entry : cache.entries)
    {
        if (entry.elided_count == 0U && entry.applied_count == 0U)
        {
            continue;
        }
        ++snapshot.active_entry_count;
    }
    return snapshot;
}

}  // namespace repiu::engine
