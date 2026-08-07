#include "glide_setter_state_cache_probe.h"

#include "repiu/platform/win32/glide_setter_state_cache.h"

#include <iostream>
#include <memory>

namespace repiu::tools
{
namespace
{

using go = repiu::hle::GlideGateId;
using platform::win32::BuildGlideSetterStateKey;
using platform::win32::Win32GlideSetterStateCache;
using platform::win32::Win32GlideSetterStateKey;

using CachePtr = std::unique_ptr<Win32GlideSetterStateCache>;

CachePtr MakeCache()
{
    return std::make_unique<Win32GlideSetterStateCache>();
}

Win32GlideSetterStateKey OneWordKey(std::uint32_t value,
                                    std::uint32_t generation = 0U)
{
    return BuildGlideSetterStateKey(&value, 1U, generation);
}

// The batch-one list must never contain a gate the shared model does not treat as
// a state setter, or the elision would be acting on a key that does not describe
// the state. Checked exhaustively rather than by spot check.
bool ElisionListIsSubsetOfStateGates()
{
    using platform::win32::IsGlideSetterElisionGate;
    using platform::win32::IsGlideSetterStateGate;
    for (std::uint32_t raw = 0; raw <= static_cast<std::uint32_t>(
                                          go::kGrTexSource); ++raw)
    {
        const auto gate_id = static_cast<go>(raw);
        if (IsGlideSetterElisionGate(gate_id) &&
            !IsGlideSetterStateGate(gate_id))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

bool RunGlideSetterStateCacheProbe()
{
    using platform::win32::BumpGlideSetterStateCacheTextureGeneration;
    using platform::win32::InvalidateGlideSetterStateCache;
    using platform::win32::IsGlideSetterElisionGate;
    using platform::win32::RecordGlideSetterStateApplied;
    using platform::win32::RecordGlideSetterStateElided;
    using platform::win32::RecordGlideSetterStateVoided;
    using platform::win32::ShouldElideGlideSetterState;
    using platform::win32::SnapshotGlideSetterStateCache;

    // Absent means enabled: this is a default-on optimization with a kill switch.
    const bool policy =
        platform::win32::ResolveGlideSetterElisionEnabled("") &&
        platform::win32::ResolveGlideSetterElisionEnabled("1") &&
        platform::win32::ResolveGlideSetterElisionEnabled("on") &&
        !platform::win32::ResolveGlideSetterElisionEnabled("0") &&
        !platform::win32::ResolveGlideSetterElisionEnabled("off") &&
        !platform::win32::ResolveGlideSetterElisionEnabled("false");

    const bool membership =
        ElisionListIsSubsetOfStateGates() &&
        IsGlideSetterElisionGate(go::kGrColorMask) &&
        IsGlideSetterElisionGate(go::kGrAlphaBlendFunction) &&
        IsGlideSetterElisionGate(go::kGrClipWindow) &&
        IsGlideSetterElisionGate(go::kGrAlphaTestFunction) &&
        IsGlideSetterElisionGate(go::kGrFogMode) &&
        IsGlideSetterElisionGate(go::kGrCullMode) &&
        IsGlideSetterElisionGate(go::kGrDepthBufferFunction) &&
        // Batch-one exclusions, each for a recorded reason.
        !IsGlideSetterElisionGate(go::kGrTexSource) &&
        !IsGlideSetterElisionGate(go::kGrDepthMask) &&
        !IsGlideSetterElisionGate(go::kGrConstantColorValue) &&
        !IsGlideSetterElisionGate(go::kGrColorCombine) &&
        !IsGlideSetterElisionGate(go::kGrAlphaCombine) &&
        !IsGlideSetterElisionGate(go::kGrTexClampMode) &&
        // Never a setter at all.
        !IsGlideSetterElisionGate(go::kGrDrawTriangle) &&
        !IsGlideSetterElisionGate(go::kGrBufferSwap);

    // Task 437 batch two. The list is a pure classification; whether it is
    // consulted is the opt-in switch, whose unset-is-off convention is pinned by
    // the env toggle probe. What must hold here is the membership itself.
    using platform::win32::IsGlideSetterStateGate;
    using platform::win32::IsGlideSetterTextureStateElisionGate;
    const bool texture_membership =
        IsGlideSetterTextureStateElisionGate(go::kGrTexClampMode) &&
        IsGlideSetterTextureStateElisionGate(go::kGrTexFilterMode) &&
        IsGlideSetterTextureStateElisionGate(go::kGrTexMipMapMode) &&
        // `grTexSource` carries a `GrTexInfo*`, so an identical key does not
        // prove identical state. It must stay out of both batches.
        !IsGlideSetterTextureStateElisionGate(go::kGrTexSource) &&
        !IsGlideSetterElisionGate(go::kGrTexSource) &&
        // Batch one is unchanged: the two lists are disjoint, so turning the
        // switch on can only widen what is elided, never redefine it.
        !IsGlideSetterElisionGate(go::kGrTexClampMode) &&
        !IsGlideSetterElisionGate(go::kGrTexFilterMode) &&
        !IsGlideSetterElisionGate(go::kGrTexMipMapMode) &&
        !IsGlideSetterTextureStateElisionGate(go::kGrColorMask) &&
        // Same rule as batch one: nothing may be elided that the shared model
        // does not treat as a state setter.
        IsGlideSetterStateGate(go::kGrTexClampMode) &&
        IsGlideSetterStateGate(go::kGrTexFilterMode) &&
        IsGlideSetterStateGate(go::kGrTexMipMapMode) &&
        !IsGlideSetterTextureStateElisionGate(go::kGrDrawTriangle) &&
        !IsGlideSetterTextureStateElisionGate(go::kGrTexDownloadMipMapLevel);

    // Task 442 batch three. `grTexSource` is elidable here and nowhere else, and
    // the three lists stay disjoint so a switch can only widen what is covered,
    // never redefine it.
    using platform::win32::IsGlideSetterBatchThreeElisionGate;
    const bool batch_three_membership =
        IsGlideSetterBatchThreeElisionGate(go::kGrTexSource) &&
        IsGlideSetterBatchThreeElisionGate(go::kGrConstantColorValue) &&
        IsGlideSetterBatchThreeElisionGate(go::kGrDepthMask) &&
        IsGlideSetterStateGate(go::kGrTexSource) &&
        IsGlideSetterStateGate(go::kGrConstantColorValue) &&
        IsGlideSetterStateGate(go::kGrDepthMask) &&
        !IsGlideSetterElisionGate(go::kGrTexSource) &&
        !IsGlideSetterElisionGate(go::kGrConstantColorValue) &&
        !IsGlideSetterElisionGate(go::kGrDepthMask) &&
        !IsGlideSetterTextureStateElisionGate(go::kGrTexSource) &&
        !IsGlideSetterBatchThreeElisionGate(go::kGrTexClampMode) &&
        !IsGlideSetterBatchThreeElisionGate(go::kGrColorMask) &&
        // Still never a draw, and never a value-returning gate.
        !IsGlideSetterBatchThreeElisionGate(go::kGrDrawTriangle) &&
        !IsGlideSetterBatchThreeElisionGate(go::kGrLfbLock) &&
        !IsGlideSetterBatchThreeElisionGate(go::kGrBufferSwap);

    // Task 443 batch four: two setters measured with a single distinct value for
    // the life of the process. `grFogTable` is excluded because its argument is
    // a pointer to a table, so an identical pointer proves nothing about the
    // contents -- the same hazard `grTexSource` was wrongly accused of.
    using platform::win32::IsGlideSetterBatchFourElisionGate;
    const bool batch_four_membership =
        IsGlideSetterBatchFourElisionGate(go::kGrFogColorValue) &&
        IsGlideSetterBatchFourElisionGate(go::kGrDitherMode) &&
        IsGlideSetterStateGate(go::kGrFogColorValue) &&
        IsGlideSetterStateGate(go::kGrDitherMode) &&
        !IsGlideSetterBatchFourElisionGate(go::kGrFogTable) &&
        !IsGlideSetterElisionGate(go::kGrFogColorValue) &&
        !IsGlideSetterElisionGate(go::kGrDitherMode) &&
        !IsGlideSetterBatchThreeElisionGate(go::kGrFogColorValue) &&
        !IsGlideSetterTextureStateElisionGate(go::kGrDitherMode) &&
        !IsGlideSetterBatchFourElisionGate(go::kGrTexSource) &&
        !IsGlideSetterBatchFourElisionGate(go::kGrDrawTriangle) &&
        !IsGlideSetterBatchFourElisionGate(go::kGrLfbLock);

    constexpr std::uint16_t kColorMask = 91U;
    const CachePtr cache = MakeCache();
    // Nothing applied yet, so nothing may be elided.
    const bool cold = !ShouldElideGlideSetterState(
        cache.get(), kColorMask, OneWordKey(1U));
    RecordGlideSetterStateApplied(cache.get(), kColorMask, OneWordKey(1U));
    const bool warm = ShouldElideGlideSetterState(
        cache.get(), kColorMask, OneWordKey(1U));
    // A different argument is never a repeat.
    const bool differs = !ShouldElideGlideSetterState(
        cache.get(), kColorMask, OneWordKey(0U));
    // A different ordinal has its own record.
    const bool per_ordinal = !ShouldElideGlideSetterState(
        cache.get(), 79U, OneWordKey(1U));

    // A voided record must not be trusted: eliding after a decline would assume a
    // host state that never landed.
    RecordGlideSetterStateVoided(cache.get(), kColorMask);
    const bool voided = !ShouldElideGlideSetterState(
        cache.get(), kColorMask, OneWordKey(1U));

    // Invalidation clears every record, matching the rule the census measured with.
    const CachePtr invalidated_cache = MakeCache();
    RecordGlideSetterStateApplied(
        invalidated_cache.get(), kColorMask, OneWordKey(1U));
    RecordGlideSetterStateApplied(invalidated_cache.get(), 79U, OneWordKey(2U));
    InvalidateGlideSetterStateCache(invalidated_cache.get());
    const bool invalidation =
        !ShouldElideGlideSetterState(
            invalidated_cache.get(), kColorMask, OneWordKey(1U)) &&
        !ShouldElideGlideSetterState(
            invalidated_cache.get(), 79U, OneWordKey(2U)) &&
        SnapshotGlideSetterStateCache(*invalidated_cache)
                .invalidation_count == 1U;

    // A texture download must make an otherwise identical texture-dependent key
    // compare unequal. Batch one has no texture setter, but the wiring is verified
    // so a later batch inherits it correctly.
    const CachePtr texture_cache = MakeCache();
    RecordGlideSetterStateApplied(
        texture_cache.get(), 138U,
        OneWordKey(0x1000U, texture_cache->texture_generation));
    const bool texture_before = ShouldElideGlideSetterState(
        texture_cache.get(), 138U,
        OneWordKey(0x1000U, texture_cache->texture_generation));
    BumpGlideSetterStateCacheTextureGeneration(texture_cache.get());
    const bool texture_after = !ShouldElideGlideSetterState(
        texture_cache.get(), 138U,
        OneWordKey(0x1000U, texture_cache->texture_generation));
    const bool texture_generation =
        texture_before && texture_after &&
        texture_cache->texture_generation == 1U;

    const CachePtr counted_cache = MakeCache();
    RecordGlideSetterStateApplied(
        counted_cache.get(), kColorMask, OneWordKey(1U));
    RecordGlideSetterStateElided(counted_cache.get(), kColorMask);
    RecordGlideSetterStateElided(counted_cache.get(), kColorMask);
    RecordGlideSetterStateVoided(counted_cache.get(), kColorMask);
    const auto counted = SnapshotGlideSetterStateCache(*counted_cache);
    const bool counters =
        counted.enabled &&
        counted.active_entry_count == 1U &&
        counted.applied_count == 1U &&
        counted.elided_count == 2U &&
        counted.voided_count == 1U &&
        counted_cache->entries[kColorMask].elided_count == 2U &&
        counted_cache->entries[kColorMask].applied_count == 1U;

    const CachePtr overflow_cache = MakeCache();
    RecordGlideSetterStateApplied(overflow_cache.get(), 300U, OneWordKey(1U));
    const bool overflow =
        SnapshotGlideSetterStateCache(*overflow_cache)
                .ordinal_overflow_count == 1U &&
        !ShouldElideGlideSetterState(
            overflow_cache.get(), 300U, OneWordKey(1U));

    RecordGlideSetterStateApplied(nullptr, kColorMask, OneWordKey(1U));
    RecordGlideSetterStateElided(nullptr, kColorMask);
    RecordGlideSetterStateVoided(nullptr, kColorMask);
    InvalidateGlideSetterStateCache(nullptr);
    BumpGlideSetterStateCacheTextureGeneration(nullptr);
    const bool inert =
        !ShouldElideGlideSetterState(nullptr, kColorMask, OneWordKey(1U)) &&
        !SnapshotGlideSetterStateCache(*MakeCache()).enabled;

    const bool all = policy && membership && texture_membership &&
        batch_three_membership && batch_four_membership && cold && warm &&
        differs && per_ordinal && voided && invalidation && texture_generation &&
        counters && overflow && inert;
    std::cout << "glide_setter_state_cache_policy="
              << (policy ? "true" : "false")
              << "\nglide_setter_state_cache_membership="
              << (membership ? "true" : "false")
              << "\nglide_setter_state_cache_texture_membership="
              << (texture_membership ? "true" : "false")
              << "\nglide_setter_state_cache_batch_three_membership="
              << (batch_three_membership ? "true" : "false")
              << "\nglide_setter_state_cache_batch_four_membership="
              << (batch_four_membership ? "true" : "false")
              << "\nglide_setter_state_cache_cold="
              << (cold ? "true" : "false")
              << "\nglide_setter_state_cache_warm="
              << (warm ? "true" : "false")
              << "\nglide_setter_state_cache_differs="
              << (differs ? "true" : "false")
              << "\nglide_setter_state_cache_per_ordinal="
              << (per_ordinal ? "true" : "false")
              << "\nglide_setter_state_cache_voided="
              << (voided ? "true" : "false")
              << "\nglide_setter_state_cache_invalidation="
              << (invalidation ? "true" : "false")
              << "\nglide_setter_state_cache_texture_generation="
              << (texture_generation ? "true" : "false")
              << "\nglide_setter_state_cache_counters="
              << (counters ? "true" : "false")
              << "\nglide_setter_state_cache_overflow="
              << (overflow ? "true" : "false")
              << "\nglide_setter_state_cache_inert="
              << (inert ? "true" : "false")
              << "\nglide_setter_state_cache_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
