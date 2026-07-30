#include "glide_setter_state_census_probe.h"

#include "repiu/platform/win32/glide_setter_state_census.h"

#include <iostream>
#include <memory>

namespace repiu::tools
{
namespace
{

using go = repiu::hle::GlideGateId;
using platform::win32::BuildGlideSetterStateKey;
using platform::win32::RecordGlideSetterCensusCall;
using platform::win32::Win32GlideSetterCensusOutcome;
using platform::win32::Win32GlideSetterCensusProfile;
using platform::win32::Win32GlideSetterStateKey;

// One profile holds 256 per-ordinal entries, so it lives on the heap here for
// the same reason the snapshot carries aggregates only.
using ProfilePtr = std::unique_ptr<Win32GlideSetterCensusProfile>;

ProfilePtr MakeProfile()
{
    return std::make_unique<Win32GlideSetterCensusProfile>();
}

Win32GlideSetterStateKey OneWordKey(std::uint32_t value,
                                    std::uint32_t generation = 0U)
{
    return BuildGlideSetterStateKey(&value, 1U, generation);
}

void Apply(Win32GlideSetterCensusProfile* profile,
           std::uint16_t ordinal,
           const Win32GlideSetterStateKey& key)
{
    RecordGlideSetterCensusCall(
        profile, ordinal, key, Win32GlideSetterCensusOutcome::kApplied);
}

}  // namespace

bool RunGlideSetterStateCensusProbe()
{
    using platform::win32::IsGlideSetterStateGate;
    using platform::win32::IsGlideSetterStateInvalidatingGate;
    using platform::win32::IsGlideSetterStateTextureGenerationGate;
    using platform::win32::IsGlideSetterStateTextureDependentGate;
    using platform::win32::RecordGlideSetterCensusFrameBoundary;
    using platform::win32::RecordGlideSetterCensusInvalidation;
    using platform::win32::RecordGlideSetterCensusKeyOverflow;
    using platform::win32::RecordGlideSetterCensusTextureGeneration;
    using platform::win32::SnapshotGlideSetterCensus;

    const bool policy =
        !platform::win32::ResolveGlideSetterCensusEnabled("") &&
        !platform::win32::ResolveGlideSetterCensusEnabled("0") &&
        platform::win32::ResolveGlideSetterCensusEnabled("1") &&
        platform::win32::ResolveGlideSetterCensusEnabled("on") &&
        platform::win32::ResolveGlideSetterCensusEnabled("true");

    // The target list must contain state setters and exclude every gate whose
    // arguments carry pointers, coordinates, or per-call payloads.
    const bool classification =
        IsGlideSetterStateGate(go::kGrDepthMask) &&
        IsGlideSetterStateGate(go::kGrAlphaBlendFunction) &&
        IsGlideSetterStateGate(go::kGrColorMask) &&
        IsGlideSetterStateGate(go::kGrFogMode) &&
        IsGlideSetterStateGate(go::kGrClipWindow) &&
        !IsGlideSetterStateGate(go::kGrDrawTriangle) &&
        !IsGlideSetterStateGate(go::kGrBufferSwap) &&
        !IsGlideSetterStateGate(go::kGrLfbLock) &&
        !IsGlideSetterStateGate(go::kGrTexDownloadMipMapLevel) &&
        !IsGlideSetterStateGate(go::kGrFogTable) &&
        IsGlideSetterStateInvalidatingGate(go::kGrSstWinOpen) &&
        IsGlideSetterStateInvalidatingGate(go::kGrGlideSetState) &&
        IsGlideSetterStateInvalidatingGate(go::kGrRenderBuffer) &&
        !IsGlideSetterStateInvalidatingGate(go::kGrDepthMask) &&
        IsGlideSetterStateTextureGenerationGate(
            go::kGrTexDownloadMipMapLevel) &&
        !IsGlideSetterStateTextureGenerationGate(go::kGrDepthMask) &&
        IsGlideSetterStateTextureDependentGate(go::kGrTexSource) &&
        !IsGlideSetterStateTextureDependentGate(go::kGrDepthMask);

    constexpr std::uint16_t kDepthMask = 34U;
    const ProfilePtr profile = MakeProfile();
    // First application, then two exact repeats, then a change, then a repeat.
    Apply(profile.get(), kDepthMask, OneWordKey(1U));
    Apply(profile.get(), kDepthMask, OneWordKey(1U));
    Apply(profile.get(), kDepthMask, OneWordKey(1U));
    Apply(profile.get(), kDepthMask, OneWordKey(0U));
    Apply(profile.get(), kDepthMask, OneWordKey(0U));
    RecordGlideSetterCensusCall(
        profile.get(), kDepthMask, OneWordKey(0U),
        Win32GlideSetterCensusOutcome::kFailed);
    // A failure voids the record, so the next identical call is a first, not a
    // repeat: eliding it would have assumed a host state that never landed.
    Apply(profile.get(), kDepthMask, OneWordKey(0U));
    RecordGlideSetterCensusCall(
        profile.get(), kDepthMask, OneWordKey(0U),
        Win32GlideSetterCensusOutcome::kUnsupported);
    Apply(profile.get(), kDepthMask, OneWordKey(0U));

    const auto snapshot = SnapshotGlideSetterCensus(*profile);
    const auto& entry = profile->entries[kDepthMask];
    const bool aggregation =
        snapshot.enabled &&
        snapshot.active_entry_count == 1U &&
        entry.call_count == 9U &&
        entry.first_count == 3U &&
        entry.same_count == 3U &&
        entry.changed_count == 1U &&
        entry.failure_count == 1U &&
        entry.unsupported_count == 1U &&
        entry.max_repeat_run == 2U &&
        entry.distinct_key_count == 2U;
    const bool identity =
        entry.first_count + entry.same_count + entry.changed_count +
            entry.failure_count + entry.unsupported_count ==
        entry.call_count;
    const bool totals =
        snapshot.call_count == entry.call_count &&
        snapshot.same_count == entry.same_count &&
        snapshot.changed_count == entry.changed_count &&
        snapshot.failure_count == entry.failure_count &&
        snapshot.unsupported_count == entry.unsupported_count;

    // Invalidation must void the applied record, matching the rule Task 365
    // has to obey, so the measured ceiling is a real ceiling.
    const ProfilePtr invalidation_profile = MakeProfile();
    Apply(invalidation_profile.get(), kDepthMask, OneWordKey(1U));
    Apply(invalidation_profile.get(), kDepthMask, OneWordKey(1U));
    RecordGlideSetterCensusInvalidation(invalidation_profile.get());
    Apply(invalidation_profile.get(), kDepthMask, OneWordKey(1U));
    const auto invalidated =
        SnapshotGlideSetterCensus(*invalidation_profile);
    const bool invalidation =
        invalidated.invalidation_count == 1U &&
        invalidation_profile->entries[kDepthMask].same_count == 1U &&
        invalidation_profile->entries[kDepthMask].first_count == 2U;

    // A texture download must make an otherwise identical texture-state key
    // compare unequal, because the contents behind the address changed.
    constexpr std::uint16_t kTexSource = 138U;
    const ProfilePtr texture_profile = MakeProfile();
    Apply(texture_profile.get(), kTexSource,
          OneWordKey(0x1000U, texture_profile->texture_generation));
    Apply(texture_profile.get(), kTexSource,
          OneWordKey(0x1000U, texture_profile->texture_generation));
    RecordGlideSetterCensusTextureGeneration(texture_profile.get());
    Apply(texture_profile.get(), kTexSource,
          OneWordKey(0x1000U, texture_profile->texture_generation));
    Apply(texture_profile.get(), kTexSource,
          OneWordKey(0x1000U, texture_profile->texture_generation));
    const auto textured = SnapshotGlideSetterCensus(*texture_profile);
    const auto& texture_entry = texture_profile->entries[kTexSource];
    const bool texture_generation =
        textured.texture_generation == 1U &&
        texture_entry.call_count == 4U &&
        texture_entry.same_count == 2U &&
        texture_entry.changed_count == 1U &&
        texture_entry.first_count == 1U;

    // Per-frame maxima roll at the swap boundary rather than accumulating.
    const ProfilePtr frame_profile = MakeProfile();
    Apply(frame_profile.get(), kDepthMask, OneWordKey(1U));
    Apply(frame_profile.get(), kDepthMask, OneWordKey(0U));
    Apply(frame_profile.get(), kDepthMask, OneWordKey(0U));
    RecordGlideSetterCensusFrameBoundary(frame_profile.get());
    Apply(frame_profile.get(), kDepthMask, OneWordKey(0U));
    RecordGlideSetterCensusFrameBoundary(frame_profile.get());
    const auto framed = SnapshotGlideSetterCensus(*frame_profile);
    const auto& frame_entry = frame_profile->entries[kDepthMask];
    const bool frames =
        framed.frame_count == 2U &&
        frame_entry.max_frame_call_count == 3U &&
        frame_entry.max_frame_change_count == 2U &&
        frame_entry.frame_call_count == 0U;

    // Wider-than-key setters are excluded rather than truncated into a
    // colliding key, and the exclusion is counted.
    const ProfilePtr overflow_profile = MakeProfile();
    RecordGlideSetterCensusKeyOverflow(overflow_profile.get(), kDepthMask);
    const std::uint32_t wide[9] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const auto clamped_key = BuildGlideSetterStateKey(wide, 9U, 0U);
    RecordGlideSetterCensusCall(overflow_profile.get(), 300U, clamped_key,
                                Win32GlideSetterCensusOutcome::kApplied);
    const auto overflowed = SnapshotGlideSetterCensus(*overflow_profile);
    const bool overflow =
        overflowed.key_overflow_count == 1U &&
        overflowed.ordinal_overflow_count == 1U &&
        clamped_key.word_count ==
            platform::win32::kWin32GlideSetterStateKeyWords;

    RecordGlideSetterCensusCall(nullptr, kDepthMask, OneWordKey(1U),
                                Win32GlideSetterCensusOutcome::kApplied);
    RecordGlideSetterCensusInvalidation(nullptr);
    RecordGlideSetterCensusTextureGeneration(nullptr);
    RecordGlideSetterCensusFrameBoundary(nullptr);
    RecordGlideSetterCensusKeyOverflow(nullptr, kDepthMask);
    const ProfilePtr untouched = MakeProfile();
    const auto inert_snapshot = SnapshotGlideSetterCensus(*untouched);
    const bool inert =
        !inert_snapshot.enabled &&
        inert_snapshot.call_count == 0U &&
        inert_snapshot.active_entry_count == 0U;

    const bool all = policy && classification && aggregation && identity &&
        totals && invalidation && texture_generation && frames && overflow &&
        inert;
    std::cout << "glide_setter_state_census_policy="
              << (policy ? "true" : "false")
              << "\nglide_setter_state_census_classification="
              << (classification ? "true" : "false")
              << "\nglide_setter_state_census_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_setter_state_census_identity="
              << (identity ? "true" : "false")
              << "\nglide_setter_state_census_totals="
              << (totals ? "true" : "false")
              << "\nglide_setter_state_census_invalidation="
              << (invalidation ? "true" : "false")
              << "\nglide_setter_state_census_texture_generation="
              << (texture_generation ? "true" : "false")
              << "\nglide_setter_state_census_frames="
              << (frames ? "true" : "false")
              << "\nglide_setter_state_census_overflow="
              << (overflow ? "true" : "false")
              << "\nglide_setter_state_census_inert="
              << (inert ? "true" : "false")
              << "\nglide_setter_state_census_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
