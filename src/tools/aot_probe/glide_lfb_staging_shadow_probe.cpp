#include "glide_lfb_staging_shadow_probe.h"

#include "repiu/engine/glide_lfb_staging_shadow.h"

#include <iostream>
#include <string_view>

namespace repiu::tools
{
namespace
{

using engine::GlideLfbStagingShadowInvalidation;
using engine::GlideLfbStagingShadowState;
using Gate = repiu::hle::GlideGateId;

constexpr std::uint32_t kBackBuffer = 1U;
constexpr std::uint32_t kFrontBuffer = 0U;
constexpr std::uint32_t kFormat565 = 0U;
constexpr std::uint32_t kWidth = 640U;
constexpr std::uint32_t kHeight = 480U;

std::uint32_t ReasonCount(const GlideLfbStagingShadowState& state,
                          const GlideLfbStagingShadowInvalidation reason)
{
    return state.invalidate_counts[static_cast<std::size_t>(reason)];
}

// One write lock as the boundary drives it: decide reuse, record it, hand the
// surface to the guest, then unlock. Returns whether the seed was skipped.
bool RunLock(GlideLfbStagingShadowState* const state, const bool reuse_enabled)
{
    const bool reusable = engine::CanReuseGlideLfbStagingShadow(
        *state, kBackBuffer, kFormat565, kWidth, kHeight);
    const bool reused = reusable && reuse_enabled;
    engine::NoteGlideLfbStagingShadowLock(state, reusable, reused);
    engine::InvalidateGlideLfbStagingShadow(
        state, GlideLfbStagingShadowInvalidation::kLockHandoff);
    return reused;
}

void RunUnlock(GlideLfbStagingShadowState* const state)
{
    engine::ValidateGlideLfbStagingShadow(state, kBackBuffer, kFormat565,
                                          kWidth, kHeight);
}

}  // namespace

bool RunGlideLfbStagingShadowProbe()
{
    const bool policy =
        !engine::ResolveGlideLfbStagingShadowSetting("") &&
        !engine::ResolveGlideLfbStagingShadowSetting("0") &&
        !engine::ResolveGlideLfbStagingShadowSetting("yes") &&
        engine::ResolveGlideLfbStagingShadowSetting("1") &&
        engine::ResolveGlideLfbStagingShadowSetting("on") &&
        engine::ResolveGlideLfbStagingShadowSetting("true");

    // Every gate that can change frame buffer pixels, change what the 565
    // encoding means, or move the render target must invalidate. An unknown
    // gate must invalidate too, which is the property that keeps a gate added
    // later from silently inheriting reuse.
    const bool invalidating_gates =
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kUnknown) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideInit) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideShutdown) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrBufferClear) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrBufferSwap) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrDrawLine) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrDrawPoint) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrDrawTriangle) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrDrawPlanarPolygon) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrDrawPlanarPolygonVertexList) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrDrawPolygon) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrDrawPolygonVertexList) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrAADrawPoint) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrAADrawLine) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrAADrawTriangle) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrAADrawPolygon) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrAADrawPolygonVertexList) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrLfbWriteRegion) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrLfbReadRegion) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrLfbWriteColorFormat) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrLfbWriteColorSwizzle) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrRenderBuffer) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideSetState) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstWinOpen) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstWinClose) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstOrigin) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstVidMode) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstControl) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrSstConfigPipeline);

    // The gates a guest issues between two locks, which reuse exists to
    // survive. The lock and unlock gates are here too: their own paths carry
    // the state, so the dispatch-time predicate must leave it alone.
    const bool preserving_gates =
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrLfbLock) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrLfbUnlock) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrTexSource) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrTexDownloadMipMap) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrColorCombine) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrAlphaBlendFunction) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrClipWindow) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrConstantColorValue) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideGetState) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstIdle) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrBufferNumPending);

    // A first lock has nothing to reuse and seeds. Its unlock validates, so the
    // second lock reuses, and the third does too.
    GlideLfbStagingShadowState reuse_state;
    const bool first_seeded = !RunLock(&reuse_state, true);
    RunUnlock(&reuse_state);
    const bool second_reused = RunLock(&reuse_state, true);
    RunUnlock(&reuse_state);
    const bool third_reused = RunLock(&reuse_state, true);
    RunUnlock(&reuse_state);
    const auto reuse_snapshot =
        engine::SnapshotGlideLfbStagingShadow(reuse_state);
    const bool reuse_sequence =
        first_seeded && second_reused && third_reused &&
        reuse_snapshot.lock_count == 3U &&
        reuse_snapshot.reusable_lock_count == 2U &&
        reuse_snapshot.reused_lock_count == 2U &&
        reuse_snapshot.seed_count == 1U &&
        reuse_snapshot.validate_count == 3U &&
        // Two, not three: the first lock had no live shadow to give away, and
        // invalidation counts the valid-to-invalid transition rather than the
        // call.
        ReasonCount(reuse_state,
                    GlideLfbStagingShadowInvalidation::kLockHandoff) == 2U;

    // The same sequence with reuse disabled counts the opportunity and takes
    // none of it: three seeds, two of which were avoidable.
    GlideLfbStagingShadowState census_state;
    const bool census_first = !RunLock(&census_state, false);
    RunUnlock(&census_state);
    const bool census_second = !RunLock(&census_state, false);
    RunUnlock(&census_state);
    const auto census_snapshot =
        engine::SnapshotGlideLfbStagingShadow(census_state);
    const bool census_only =
        census_first && census_second &&
        census_snapshot.lock_count == 2U &&
        census_snapshot.reusable_lock_count == 1U &&
        census_snapshot.reused_lock_count == 0U &&
        census_snapshot.seed_count == 2U;

    // A gate between the unlock and the next lock costs the reuse, and the
    // reason is recorded once rather than once per gate.
    GlideLfbStagingShadowState gate_state;
    RunLock(&gate_state, true);
    RunUnlock(&gate_state);
    engine::InvalidateGlideLfbStagingShadow(
        &gate_state, GlideLfbStagingShadowInvalidation::kGate);
    engine::InvalidateGlideLfbStagingShadow(
        &gate_state, GlideLfbStagingShadowInvalidation::kGate);
    const bool after_gate_reused = RunLock(&gate_state, true);
    const bool gate_invalidation =
        !after_gate_reused &&
        ReasonCount(gate_state, GlideLfbStagingShadowInvalidation::kGate) == 1U;

    // A shadow of one buffer, pixel format or resolution is not a shadow of
    // another.
    GlideLfbStagingShadowState mismatch_state;
    engine::ValidateGlideLfbStagingShadow(&mismatch_state, kBackBuffer,
                                          kFormat565, kWidth, kHeight);
    const bool mismatch =
        engine::CanReuseGlideLfbStagingShadow(mismatch_state, kBackBuffer,
                                              kFormat565, kWidth, kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch_state, kFrontBuffer,
                                               kFormat565, kWidth, kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch_state, kBackBuffer,
                                               kFormat565 + 1U, kWidth,
                                               kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch_state, kBackBuffer,
                                               kFormat565, kWidth + 1U,
                                               kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch_state, kBackBuffer,
                                               kFormat565, kWidth,
                                               kHeight + 1U);

    // A failed or flipped present leaves the frame buffer unequal to the
    // surface, and a degenerate extent cannot be validated at all.
    GlideLfbStagingShadowState present_state;
    engine::ValidateGlideLfbStagingShadow(&present_state, kBackBuffer,
                                          kFormat565, kWidth, kHeight);
    engine::InvalidateGlideLfbStagingShadow(
        &present_state, GlideLfbStagingShadowInvalidation::kPresentFailed);
    const bool after_failed_present = present_state.valid;
    engine::ValidateGlideLfbStagingShadow(&present_state, kBackBuffer,
                                          kFormat565, kWidth, kHeight);
    engine::InvalidateGlideLfbStagingShadow(
        &present_state, GlideLfbStagingShadowInvalidation::kFlippedPresent);
    engine::ValidateGlideLfbStagingShadow(&present_state, kBackBuffer,
                                          kFormat565, kWidth, kHeight);
    engine::ValidateGlideLfbStagingShadow(&present_state, kBackBuffer,
                                          kFormat565, 0U, kHeight);
    const bool present_outcomes =
        !after_failed_present && !present_state.valid &&
        ReasonCount(present_state,
                    GlideLfbStagingShadowInvalidation::kPresentFailed) == 1U &&
        ReasonCount(present_state,
                    GlideLfbStagingShadowInvalidation::kFlippedPresent) == 1U &&
        ReasonCount(present_state,
                    GlideLfbStagingShadowInvalidation::kSurfaceMismatch) == 1U;

    // Null states are tolerated and every reason has a name, so a report can
    // print the table without a gap.
    engine::InvalidateGlideLfbStagingShadow(
        nullptr, GlideLfbStagingShadowInvalidation::kShutdown);
    engine::ValidateGlideLfbStagingShadow(nullptr, kBackBuffer, kFormat565,
                                          kWidth, kHeight);
    engine::NoteGlideLfbStagingShadowLock(nullptr, true, true);
    bool names = true;
    for (std::size_t index = 0U;
         index < engine::kGlideLfbStagingShadowReasonCount; ++index)
    {
        const char* const name = engine::GlideLfbStagingShadowInvalidationName(
            static_cast<GlideLfbStagingShadowInvalidation>(index));
        names = names && name != nullptr && name[0] != '\0' &&
            std::string_view(name) != "unknown";
    }

    const bool all = policy && invalidating_gates && preserving_gates &&
        reuse_sequence && census_only && gate_invalidation && mismatch &&
        present_outcomes && names;
    std::cout << "glide_lfb_staging_shadow_policy="
              << (policy ? "true" : "false")
              << "\nglide_lfb_staging_shadow_invalidating_gates="
              << (invalidating_gates ? "true" : "false")
              << "\nglide_lfb_staging_shadow_preserving_gates="
              << (preserving_gates ? "true" : "false")
              << "\nglide_lfb_staging_shadow_reuse_sequence="
              << (reuse_sequence ? "true" : "false")
              << "\nglide_lfb_staging_shadow_census_only="
              << (census_only ? "true" : "false")
              << "\nglide_lfb_staging_shadow_gate_invalidation="
              << (gate_invalidation ? "true" : "false")
              << "\nglide_lfb_staging_shadow_mismatch="
              << (mismatch ? "true" : "false")
              << "\nglide_lfb_staging_shadow_present_outcomes="
              << (present_outcomes ? "true" : "false")
              << "\nglide_lfb_staging_shadow_names="
              << (names ? "true" : "false")
              << "\nglide_lfb_staging_shadow_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
