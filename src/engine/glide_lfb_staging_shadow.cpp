#include "repiu/engine/glide_lfb_staging_shadow.h"

#include <cstdlib>

namespace repiu::engine
{
namespace
{

using Reason = GlideLfbStagingShadowInvalidation;

std::size_t ReasonIndex(const Reason reason)
{
    const auto index = static_cast<std::size_t>(reason);
    return index < kGlideLfbStagingShadowReasonCount ? index : 0U;
}

}  // namespace

bool ResolveGlideLfbStagingShadowSetting(const std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideLfbStagingReuseEnabled()
{
    static const bool enabled = [] {
        const char* const value =
            std::getenv("REPIU_GLIDE_LFB_STAGING_REUSE");
        return value != nullptr && ResolveGlideLfbStagingShadowSetting(value);
    }();
    return enabled;
}

bool GlideLfbStagingShadowCensusEnabled()
{
    static const bool enabled = [] {
        const char* const value =
            std::getenv("REPIU_GLIDE_LFB_STAGING_SHADOW_CENSUS");
        return value != nullptr && ResolveGlideLfbStagingShadowSetting(value);
    }();
    return enabled || GlideLfbStagingReuseEnabled();
}

bool GlideOrdinalPreservesLfbStagingShadow(const repiu::hle::GlideGateId gate_id)
{
    using Gate = repiu::hle::GlideGateId;
    switch (gate_id)
    {
        // The lock and unlock paths carry the state themselves: the lock hands
        // the surface to the guest and the unlock decides whether the blit left
        // the frame buffer equal to it. Treating them as invalidating here
        // would clear the state before either could read it.
        case Gate::kGrLfbLock:
        case Gate::kGrLfbUnlock:
        // Queries.
        case Gate::kGrBufferNumPending:
        case Gate::kGrSstQueryHardware:
        case Gate::kGrSstQueryBoards:
        case Gate::kGrSstScreenWidth:
        case Gate::kGrSstScreenHeight:
        case Gate::kGrSstIdle:
        case Gate::kGrSstIsBusy:
        case Gate::kGrSstStatus:
        case Gate::kGrSstVideoLine:
        case Gate::kGrSstVRetraceOn:
        case Gate::kGrSstPerfStats:
        case Gate::kGrSstResetPerfStats:
        case Gate::kGrGlideGetState:
        case Gate::kGrTexTextureMemRequired:
        case Gate::kGrTexCalcMemRequired:
        // Driver declarations and selection, neither of which draws.
        case Gate::kGrHints:
        case Gate::kGrSstSelect:
        // Pixel pipeline state. These decide how a later draw resolves, and a
        // later draw is itself an invalidating gate.
        case Gate::kGrColorMask:
        case Gate::kGrDepthMask:
        case Gate::kGrDepthBiasLevel:
        case Gate::kGrDepthBufferMode:
        case Gate::kGrDepthBufferFunction:
        case Gate::kGrAlphaCombine:
        case Gate::kGrColorCombine:
        case Gate::kGrAlphaBlendFunction:
        case Gate::kGrAlphaTestFunction:
        case Gate::kGrAlphaTestReferenceValue:
        case Gate::kGrChromaKeyMode:
        case Gate::kGrChromaKeyValue:
        case Gate::kGrConstantColorValue:
        case Gate::kGrConstantColorValue4:
        case Gate::kGrCullMode:
        case Gate::kGrDitherMode:
        case Gate::kGrClipWindow:
        case Gate::kGrGammaCorrectionValue:
        case Gate::kGrAlphaControlSitRgbLighting:
        // Fog state and its table generator.
        case Gate::kGrFogMode:
        case Gate::kGrFogColorValue:
        case Gate::kGrFogTable:
        case Gate::kGuFogGenerateExp:
        // Texture memory and texture unit state. A download writes texture
        // memory, never the color buffer.
        case Gate::kGrTexMinAddress:
        case Gate::kGrTexMaxAddress:
        case Gate::kGrTexDownloadMipMap:
        case Gate::kGrTexDownloadMipMapLevel:
        case Gate::kGrTexDownloadMipMapLevelPartial:
        case Gate::kGrTexDownloadTable:
        case Gate::kGrTexDownloadTablePartial:
        case Gate::kGrTexNccTable:
        case Gate::kGrTexClampMode:
        case Gate::kGrTexCombine:
        case Gate::kGrTexCombineFunction:
        case Gate::kGrTexDetailControl:
        case Gate::kGrTexFilterMode:
        case Gate::kGrTexLodBiasValue:
        case Gate::kGrTexMipMapMode:
        case Gate::kGrTexMultiBase:
        case Gate::kGrTexMultiBaseAddress:
        case Gate::kGrTexSource:
        // Constants a later region write reads. The region write is the gate
        // that moves pixels, and it invalidates.
        case Gate::kGrLfbConstantAlpha:
        case Gate::kGrLfbConstantDepth:
            return true;
        default:
            // Draws, clears, swaps, both region gates, render-target and
            // origin changes, LFB format and swizzle changes, window and
            // Glide lifecycle, `kUnknown`, and anything added after this was
            // written.
            return false;
    }
}

GlideLfbStagingShadowInvalidation ClassifyGlideLfbStagingShadowGate(
    const repiu::hle::GlideGateId gate_id)
{
    using Gate = repiu::hle::GlideGateId;
    switch (gate_id)
    {
        case Gate::kGrBufferSwap:
            return Reason::kSwapGate;
        case Gate::kGrBufferClear:
            return Reason::kClearGate;
        case Gate::kGrDrawLine:
        case Gate::kGrDrawPoint:
        case Gate::kGrDrawTriangle:
        case Gate::kGrDrawPlanarPolygon:
        case Gate::kGrDrawPlanarPolygonVertexList:
        case Gate::kGrDrawPolygon:
        case Gate::kGrDrawPolygonVertexList:
        case Gate::kGrAADrawPoint:
        case Gate::kGrAADrawLine:
        case Gate::kGrAADrawTriangle:
        case Gate::kGrAADrawPolygon:
        case Gate::kGrAADrawPolygonVertexList:
            return Reason::kDrawGate;
        case Gate::kGrLfbWriteRegion:
        case Gate::kGrLfbReadRegion:
            return Reason::kRegionGate;
        default:
            return Reason::kOtherGate;
    }
}

bool CanReuseGlideLfbStagingShadow(const GlideLfbStagingShadowState& state,
                                   const std::uint32_t buffer,
                                   const std::uint32_t color_format,
                                   const std::uint32_t width,
                                   const std::uint32_t height)
{
    return state.valid && width != 0U && height != 0U &&
        state.buffer == buffer && state.color_format == color_format &&
        state.width == width && state.height == height;
}

void ValidateGlideLfbStagingShadow(GlideLfbStagingShadowState* const state,
                                   const std::uint32_t buffer,
                                   const std::uint32_t color_format,
                                   const std::uint32_t width,
                                   const std::uint32_t height)
{
    if (state == nullptr)
    {
        return;
    }
    if (width == 0U || height == 0U)
    {
        InvalidateGlideLfbStagingShadow(state, Reason::kSurfaceMismatch);
        return;
    }
    state->valid = true;
    state->buffer = buffer;
    state->color_format = color_format;
    state->width = width;
    state->height = height;
    ++state->validate_count;
}

void InvalidateGlideLfbStagingShadow(GlideLfbStagingShadowState* const state,
                                     const Reason reason)
{
    if (state == nullptr || !state->valid)
    {
        return;
    }
    state->valid = false;
    ++state->invalidate_counts[ReasonIndex(reason)];
}

void NoteGlideLfbStagingShadowLock(GlideLfbStagingShadowState* const state,
                                   const bool reusable,
                                   const bool reused)
{
    if (state == nullptr)
    {
        return;
    }
    ++state->lock_count;
    if (reusable)
    {
        ++state->reusable_lock_count;
    }
    if (reused && reusable)
    {
        ++state->reused_lock_count;
    }
    else
    {
        ++state->seed_count;
    }
}

GlideLfbStagingShadowSnapshot SnapshotGlideLfbStagingShadow(
    const GlideLfbStagingShadowState& state)
{
    GlideLfbStagingShadowSnapshot snapshot;
    snapshot.census_enabled = GlideLfbStagingShadowCensusEnabled();
    snapshot.reuse_enabled = GlideLfbStagingReuseEnabled();
    snapshot.lock_count = state.lock_count;
    snapshot.reusable_lock_count = state.reusable_lock_count;
    snapshot.reused_lock_count = state.reused_lock_count;
    snapshot.seed_count = state.seed_count;
    snapshot.validate_count = state.validate_count;
    for (std::size_t index = 0U; index < kGlideLfbStagingShadowReasonCount;
         ++index)
    {
        snapshot.invalidate_counts[index] = state.invalidate_counts[index];
    }
    return snapshot;
}

const char* GlideLfbStagingShadowInvalidationName(const Reason reason)
{
    switch (reason)
    {
        case Reason::kSwapGate:
            return "swap-gate";
        case Reason::kDrawGate:
            return "draw-gate";
        case Reason::kClearGate:
            return "clear-gate";
        case Reason::kRegionGate:
            return "region-gate";
        case Reason::kOtherGate:
            return "other-gate";
        case Reason::kLockHandoff:
            return "lock-handoff";
        case Reason::kPresentFailed:
            return "present-failed";
        case Reason::kFlippedPresent:
            return "flipped-present";
        case Reason::kSurfaceMismatch:
            return "surface-mismatch";
        case Reason::kShutdown:
            return "shutdown";
        case Reason::kCount:
            break;
    }
    return "unknown";
}

}  // namespace repiu::engine
