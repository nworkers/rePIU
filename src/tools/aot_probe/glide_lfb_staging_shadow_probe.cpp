#include "glide_lfb_staging_shadow_probe.h"

#include "repiu/engine/glide_lfb_staging_shadow.h"

#include <array>
#include <iostream>
#include <string_view>
#include <vector>

namespace repiu::tools
{
namespace
{

using engine::GlideLfbStagingShadowInvalidation;
using engine::GlideLfbStagingShadowState;
using Gate = repiu::hle::GlideGateId;

constexpr std::uint32_t kFrontBuffer = 0U;
constexpr std::uint32_t kBackBuffer = 1U;
constexpr std::uint32_t kFormat565 = 0U;
// Small enough to compare byte for byte, large enough to be a real surface.
constexpr std::uint32_t kWidth = 8U;
constexpr std::uint32_t kHeight = 4U;
constexpr std::size_t kSurfaceBytes =
    static_cast<std::size_t>(kWidth) * kHeight * 2U;

std::uint32_t ReasonCount(const GlideLfbStagingShadowState& state,
                          const GlideLfbStagingShadowInvalidation reason)
{
    return state.invalidate_counts[static_cast<std::size_t>(reason)];
}

std::vector<std::uint8_t> Surface(const std::uint8_t fill)
{
    return std::vector<std::uint8_t>(kSurfaceBytes, fill);
}

void Store(GlideLfbStagingShadowState* const state,
           const std::uint32_t buffer,
           const std::uint8_t fill)
{
    const std::vector<std::uint8_t> pixels = Surface(fill);
    engine::StoreGlideLfbStagingShadow(state, buffer, pixels.data(),
                                       pixels.size(), kFormat565, kWidth,
                                       kHeight);
}

// Reads a buffer's shadow back through the public load path, returning the
// fill byte or -1 when it could not be served.
int Load(const GlideLfbStagingShadowState& state, const std::uint32_t buffer)
{
    std::vector<std::uint8_t> out(kSurfaceBytes, 0xAAU);
    if (!engine::LoadGlideLfbStagingShadow(state, buffer, out.data(),
                                           out.size(), kFormat565, kWidth,
                                           kHeight))
    {
        return -1;
    }
    for (const std::uint8_t byte : out)
    {
        if (byte != out[0])
        {
            return -2;
        }
    }
    return out[0];
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
    // later from silently inheriting reuse. `kGrBufferSwap` is deliberately
    // absent: Task 729 made it a rotation rather than a loss.
    const bool invalidating_gates =
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kUnknown) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideInit) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideShutdown) &&
        !engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrBufferClear) &&
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

    const bool preserving_gates =
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrBufferSwap) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrLfbLock) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrLfbUnlock) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrTexSource) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrTexDownloadMipMap) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrColorCombine) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrAlphaBlendFunction) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrClipWindow) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrConstantColorValue) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrGlideGetState) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(Gate::kGrSstIdle) &&
        engine::GlideOrdinalPreservesLfbStagingShadow(
            Gate::kGrBufferNumPending);

    const bool gate_classes =
        engine::ClassifyGlideLfbStagingShadowGate(Gate::kGrBufferSwap) ==
            GlideLfbStagingShadowInvalidation::kSwapGate &&
        engine::ClassifyGlideLfbStagingShadowGate(Gate::kGrBufferClear) ==
            GlideLfbStagingShadowInvalidation::kClearGate &&
        engine::ClassifyGlideLfbStagingShadowGate(Gate::kGrDrawTriangle) ==
            GlideLfbStagingShadowInvalidation::kDrawGate &&
        engine::ClassifyGlideLfbStagingShadowGate(Gate::kGrLfbWriteRegion) ==
            GlideLfbStagingShadowInvalidation::kRegionGate &&
        engine::ClassifyGlideLfbStagingShadowGate(Gate::kUnknown) ==
            GlideLfbStagingShadowInvalidation::kOtherGate &&
        engine::GlideOrdinalTargetsRenderBufferOnly(Gate::kGrDrawTriangle) &&
        engine::GlideOrdinalTargetsRenderBufferOnly(Gate::kGrBufferClear) &&
        !engine::GlideOrdinalTargetsRenderBufferOnly(Gate::kGrLfbWriteRegion) &&
        !engine::GlideOrdinalTargetsRenderBufferOnly(Gate::kUnknown);

    // Store, then read back the same bytes.
    GlideLfbStagingShadowState store_state;
    Store(&store_state, kBackBuffer, 0x11U);
    const bool store_round_trip =
        Load(store_state, kBackBuffer) == 0x11 &&
        Load(store_state, kFrontBuffer) == -1 && store_state.store_count == 1U;

    // The page flip: what the back buffer held is what the front buffer holds
    // afterwards, and the other way round.
    GlideLfbStagingShadowState rotate_state;
    Store(&rotate_state, kBackBuffer, 0x22U);
    Store(&rotate_state, kFrontBuffer, 0x33U);
    engine::RotateGlideLfbStagingShadows(&rotate_state);
    const bool rotation =
        Load(rotate_state, kFrontBuffer) == 0x22 &&
        Load(rotate_state, kBackBuffer) == 0x33 &&
        rotate_state.swap_rotation_count == 1U &&
        // A rotation is not a loss, so nothing is counted as invalidated.
        ReasonCount(rotate_state,
                    GlideLfbStagingShadowInvalidation::kSwapGate) == 0U;

    // The guest's measured sequence: unlock stores the back buffer, one swap
    // rotates it to the front, and the next lock reads the back buffer, which
    // now holds what the front buffer held before -- the frame before last.
    GlideLfbStagingShadowState sequence;
    Store(&sequence, kBackBuffer, 0xA0U);
    engine::ApplyGlideLfbStagingShadowGate(&sequence, Gate::kGrBufferSwap);
    Store(&sequence, kBackBuffer, 0xA1U);
    engine::ApplyGlideLfbStagingShadowGate(&sequence, Gate::kGrBufferSwap);
    const bool page_flip_sequence =
        Load(sequence, kBackBuffer) == 0xA0 &&
        Load(sequence, kFrontBuffer) == 0xA1;

    // A draw or clear breaks only the render target. With the target on the
    // back buffer, the front shadow survives and a swap brings it back.
    GlideLfbStagingShadowState targeted;
    Store(&targeted, kBackBuffer, 0x44U);
    Store(&targeted, kFrontBuffer, 0x55U);
    engine::SetGlideLfbStagingShadowRenderBuffer(&targeted, kBackBuffer);
    engine::ApplyGlideLfbStagingShadowGate(&targeted, Gate::kGrBufferClear);
    engine::ApplyGlideLfbStagingShadowGate(&targeted, Gate::kGrDrawTriangle);
    const bool targeted_invalidation =
        Load(targeted, kBackBuffer) == -1 &&
        Load(targeted, kFrontBuffer) == 0x55 &&
        ReasonCount(targeted,
                    GlideLfbStagingShadowInvalidation::kClearGate) == 1U &&
        ReasonCount(targeted,
                    GlideLfbStagingShadowInvalidation::kDrawGate) == 0U;

    // Moving the render target moves what a draw breaks.
    GlideLfbStagingShadowState retarget;
    Store(&retarget, kBackBuffer, 0x66U);
    Store(&retarget, kFrontBuffer, 0x77U);
    engine::SetGlideLfbStagingShadowRenderBuffer(&retarget, kFrontBuffer);
    engine::ApplyGlideLfbStagingShadowGate(&retarget, Gate::kGrDrawTriangle);
    const bool render_target_tracked =
        Load(retarget, kBackBuffer) == 0x66 &&
        Load(retarget, kFrontBuffer) == -1;

    // A gate this layer cannot attribute to one buffer takes both.
    GlideLfbStagingShadowState untargeted;
    Store(&untargeted, kBackBuffer, 0x88U);
    Store(&untargeted, kFrontBuffer, 0x99U);
    engine::ApplyGlideLfbStagingShadowGate(&untargeted, Gate::kUnknown);
    const bool untargeted_invalidation =
        Load(untargeted, kBackBuffer) == -1 &&
        Load(untargeted, kFrontBuffer) == -1 &&
        ReasonCount(untargeted,
                    GlideLfbStagingShadowInvalidation::kOtherGate) == 2U;

    // A shadow of one pixel format or resolution is not a shadow of another,
    // and a state setter leaves it alone.
    GlideLfbStagingShadowState mismatch;
    Store(&mismatch, kBackBuffer, 0xCCU);
    engine::ApplyGlideLfbStagingShadowGate(&mismatch, Gate::kGrTexSource);
    std::vector<std::uint8_t> out(kSurfaceBytes, 0U);
    const bool mismatch_rejected =
        engine::CanReuseGlideLfbStagingShadow(mismatch, kBackBuffer,
                                              kFormat565, kWidth, kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch, kBackBuffer,
                                               kFormat565 + 1U, kWidth,
                                               kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch, kBackBuffer,
                                               kFormat565, kWidth + 1U,
                                               kHeight) &&
        !engine::CanReuseGlideLfbStagingShadow(mismatch, kBackBuffer,
                                               kFormat565, kWidth,
                                               kHeight + 1U) &&
        !engine::CanReuseGlideLfbStagingShadow(
            mismatch, engine::kGlideLfbStagingShadowBufferCount, kFormat565,
            kWidth, kHeight) &&
        // A destination smaller than the shadow is refused rather than
        // partially filled.
        !engine::LoadGlideLfbStagingShadow(mismatch, kBackBuffer, out.data(),
                                           kSurfaceBytes - 1U, kFormat565,
                                           kWidth, kHeight);

    // A degenerate surface cannot be stored, and an out-of-range buffer takes
    // the whole set down rather than being silently dropped.
    GlideLfbStagingShadowState refused;
    Store(&refused, kBackBuffer, 0xDDU);
    const std::vector<std::uint8_t> pixels = Surface(0xEEU);
    engine::StoreGlideLfbStagingShadow(&refused, kBackBuffer, pixels.data(),
                                       pixels.size(), kFormat565, 0U, kHeight);
    const bool degenerate_refused =
        Load(refused, kBackBuffer) == -1 &&
        ReasonCount(refused, GlideLfbStagingShadowInvalidation::
                                 kSurfaceMismatch) == 1U &&
        refused.store_count == 1U;

    // Lock accounting: reusable without reuse still counts a seed.
    GlideLfbStagingShadowState counted;
    engine::NoteGlideLfbStagingShadowLock(&counted, false, false);
    engine::NoteGlideLfbStagingShadowLock(&counted, true, false);
    engine::NoteGlideLfbStagingShadowLock(&counted, true, true);
    const auto snapshot = engine::SnapshotGlideLfbStagingShadow(counted);
    const bool lock_accounting =
        snapshot.lock_count == 3U && snapshot.reusable_lock_count == 2U &&
        snapshot.reused_lock_count == 1U && snapshot.seed_count == 2U;

    // Null states are tolerated and every reason has a name.
    engine::StoreGlideLfbStagingShadow(nullptr, kBackBuffer, pixels.data(),
                                       pixels.size(), kFormat565, kWidth,
                                       kHeight);
    engine::RotateGlideLfbStagingShadows(nullptr);
    engine::ApplyGlideLfbStagingShadowGate(nullptr, Gate::kGrBufferSwap);
    engine::InvalidateGlideLfbStagingShadows(
        nullptr, GlideLfbStagingShadowInvalidation::kOtherGate);
    engine::SetGlideLfbStagingShadowRenderBuffer(nullptr, kBackBuffer);
    engine::NoteGlideLfbStagingShadowLock(nullptr, true, true);
    bool names = true;
    for (std::size_t index = 0U;
         index < engine::kGlideLfbStagingShadowReasonCount; ++index)
    {
        const char* const name = engine::GlideLfbStagingShadowInvalidationName(
            static_cast<GlideLfbStagingShadowInvalidation>(index));
        names = names && name != nullptr &&
            std::string_view(name) != "unknown";
    }

    const bool all = policy && invalidating_gates && preserving_gates &&
        gate_classes && store_round_trip && rotation && page_flip_sequence &&
        targeted_invalidation && render_target_tracked &&
        untargeted_invalidation && mismatch_rejected && degenerate_refused &&
        lock_accounting && names;
    std::cout << "glide_lfb_staging_shadow_policy="
              << (policy ? "true" : "false")
              << "\nglide_lfb_staging_shadow_invalidating_gates="
              << (invalidating_gates ? "true" : "false")
              << "\nglide_lfb_staging_shadow_preserving_gates="
              << (preserving_gates ? "true" : "false")
              << "\nglide_lfb_staging_shadow_gate_classes="
              << (gate_classes ? "true" : "false")
              << "\nglide_lfb_staging_shadow_store_round_trip="
              << (store_round_trip ? "true" : "false")
              << "\nglide_lfb_staging_shadow_rotation="
              << (rotation ? "true" : "false")
              << "\nglide_lfb_staging_shadow_page_flip_sequence="
              << (page_flip_sequence ? "true" : "false")
              << "\nglide_lfb_staging_shadow_targeted_invalidation="
              << (targeted_invalidation ? "true" : "false")
              << "\nglide_lfb_staging_shadow_render_target_tracked="
              << (render_target_tracked ? "true" : "false")
              << "\nglide_lfb_staging_shadow_untargeted_invalidation="
              << (untargeted_invalidation ? "true" : "false")
              << "\nglide_lfb_staging_shadow_mismatch_rejected="
              << (mismatch_rejected ? "true" : "false")
              << "\nglide_lfb_staging_shadow_degenerate_refused="
              << (degenerate_refused ? "true" : "false")
              << "\nglide_lfb_staging_shadow_lock_accounting="
              << (lock_accounting ? "true" : "false")
              << "\nglide_lfb_staging_shadow_names="
              << (names ? "true" : "false")
              << "\nglide_lfb_staging_shadow_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
