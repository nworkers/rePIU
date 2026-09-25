#include "repiu/engine/glide_lfb_staging_shadow.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace repiu::engine
{
namespace
{

using Reason = GlideLfbStagingShadowInvalidation;
using Gate = repiu::hle::GlideGateId;

std::size_t ReasonIndex(const Reason reason)
{
    const auto index = static_cast<std::size_t>(reason);
    return index < kGlideLfbStagingShadowReasonCount ? index : 0U;
}

// The bytes a 565 surface of these dimensions occupies, or zero when the
// dimensions cannot describe one this layer stores.
std::size_t ShadowByteCount(const std::uint32_t width,
                            const std::uint32_t height)
{
    constexpr std::uint64_t kMaximumShadowBytes = 32ULL * 1024ULL * 1024ULL;
    if (width == 0U || height == 0U)
    {
        return 0U;
    }
    const std::uint64_t required =
        static_cast<std::uint64_t>(width) * height * 2ULL;
    if (required > kMaximumShadowBytes ||
        required > std::numeric_limits<std::size_t>::max())
    {
        return 0U;
    }
    return static_cast<std::size_t>(required);
}

}  // namespace

bool GlideLfbStagingShadowBufferIndexed(const std::uint32_t buffer)
{
    return buffer < kGlideLfbStagingShadowBufferCount;
}

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

bool GlideOrdinalPreservesLfbStagingShadow(const Gate gate_id)
{
    switch (gate_id)
    {
        // A swap does not destroy either buffer; it exchanges them, which
        // `RotateGlideLfbStagingShadows` reproduces. This is the one entry
        // that changed after Task 729 measured the guest's actual pattern.
        case Gate::kGrBufferSwap:
        // The lock and unlock paths carry the shadow themselves.
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
            // Draws, clears, both region gates, render-target and origin
            // changes, LFB format and swizzle changes, window and Glide
            // lifecycle, `kUnknown`, and anything added after this was written.
            return false;
    }
}

GlideLfbStagingShadowInvalidation ClassifyGlideLfbStagingShadowGate(
    const Gate gate_id)
{
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

bool GlideOrdinalTargetsRenderBufferOnly(const Gate gate_id)
{
    switch (ClassifyGlideLfbStagingShadowGate(gate_id))
    {
        case Reason::kDrawGate:
        case Reason::kClearGate:
            return true;
        default:
            // A region gate names its own buffer and this layer does not track
            // which, and an unclassified gate is unknown by definition. Both
            // invalidate the whole set.
            return false;
    }
}

void SetGlideLfbStagingShadowRenderBuffer(
    GlideLfbStagingShadowState* const state, const std::uint32_t buffer)
{
    if (state == nullptr || !GlideLfbStagingShadowBufferIndexed(buffer))
    {
        return;
    }
    state->render_buffer = buffer;
}

bool CanReuseGlideLfbStagingShadow(const GlideLfbStagingShadowState& state,
                                   const std::uint32_t buffer,
                                   const std::uint32_t color_format,
                                   const std::uint32_t width,
                                   const std::uint32_t height)
{
    if (!GlideLfbStagingShadowBufferIndexed(buffer))
    {
        return false;
    }
    const GlideLfbBufferShadow& shadow = state.buffers[buffer];
    const std::size_t required = ShadowByteCount(width, height);
    return shadow.valid && required != 0U &&
        shadow.pixels.size() == required &&
        shadow.color_format == color_format && shadow.width == width &&
        shadow.height == height;
}

bool LoadGlideLfbStagingShadow(const GlideLfbStagingShadowState& state,
                               const std::uint32_t buffer,
                               std::uint8_t* const pixels,
                               const std::size_t byte_count,
                               const std::uint32_t color_format,
                               const std::uint32_t width,
                               const std::uint32_t height)
{
    if (pixels == nullptr ||
        !CanReuseGlideLfbStagingShadow(state, buffer, color_format, width,
                                       height))
    {
        return false;
    }
    const GlideLfbBufferShadow& shadow = state.buffers[buffer];
    if (shadow.pixels.size() > byte_count)
    {
        return false;
    }
    std::memcpy(pixels, shadow.pixels.data(), shadow.pixels.size());
    return true;
}

void StoreGlideLfbStagingShadow(GlideLfbStagingShadowState* const state,
                                const std::uint32_t buffer,
                                const std::uint8_t* const pixels,
                                const std::size_t byte_count,
                                const std::uint32_t color_format,
                                const std::uint32_t width,
                                const std::uint32_t height)
{
    if (state == nullptr)
    {
        return;
    }
    if (!GlideLfbStagingShadowBufferIndexed(buffer))
    {
        InvalidateGlideLfbStagingShadows(state, Reason::kSurfaceMismatch);
        return;
    }
    const std::size_t required = ShadowByteCount(width, height);
    if (pixels == nullptr || required == 0U || required > byte_count)
    {
        InvalidateGlideLfbStagingShadowBuffer(state, buffer,
                                              Reason::kSurfaceMismatch);
        return;
    }
    GlideLfbBufferShadow& shadow = state->buffers[buffer];
    // A failed allocation leaves no half-written shadow behind: the entry is
    // invalidated and the next lock seeds from the GPU as before.
    try
    {
        shadow.pixels.resize(required);
    }
    catch (const std::bad_alloc&)
    {
        InvalidateGlideLfbStagingShadowBuffer(state, buffer,
                                              Reason::kStorageFailure);
        return;
    }
    std::memcpy(shadow.pixels.data(), pixels, required);
    shadow.color_format = color_format;
    shadow.width = width;
    shadow.height = height;
    shadow.valid = true;
    ++state->store_count;
}

void RotateGlideLfbStagingShadows(GlideLfbStagingShadowState* const state)
{
    if (state == nullptr)
    {
        return;
    }
    std::swap(state->buffers[0], state->buffers[1]);
    ++state->swap_rotation_count;
}

void InvalidateGlideLfbStagingShadowBuffer(
    GlideLfbStagingShadowState* const state,
    const std::uint32_t buffer,
    const Reason reason)
{
    if (state == nullptr || !GlideLfbStagingShadowBufferIndexed(buffer) ||
        !state->buffers[buffer].valid)
    {
        return;
    }
    state->buffers[buffer].valid = false;
    ++state->invalidate_counts[ReasonIndex(reason)];
}

void InvalidateGlideLfbStagingShadows(GlideLfbStagingShadowState* const state,
                                      const Reason reason)
{
    for (std::uint32_t buffer = 0U;
         buffer < kGlideLfbStagingShadowBufferCount; ++buffer)
    {
        InvalidateGlideLfbStagingShadowBuffer(state, buffer, reason);
    }
}

void ApplyGlideLfbStagingShadowGate(GlideLfbStagingShadowState* const state,
                                    const Gate gate_id)
{
    if (state == nullptr)
    {
        return;
    }
    if (gate_id == Gate::kGrBufferSwap)
    {
        RotateGlideLfbStagingShadows(state);
        return;
    }
    if (GlideOrdinalPreservesLfbStagingShadow(gate_id))
    {
        return;
    }
    const Reason reason = ClassifyGlideLfbStagingShadowGate(gate_id);
    if (GlideOrdinalTargetsRenderBufferOnly(gate_id))
    {
        InvalidateGlideLfbStagingShadowBuffer(state, state->render_buffer,
                                              reason);
        return;
    }
    InvalidateGlideLfbStagingShadows(state, reason);
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
    snapshot.store_count = state.store_count;
    snapshot.swap_rotation_count = state.swap_rotation_count;
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
        case Reason::kPresentFailed:
            return "present-failed";
        case Reason::kFlippedPresent:
            return "flipped-present";
        case Reason::kSurfaceMismatch:
            return "surface-mismatch";
        case Reason::kStorageFailure:
            return "storage-failure";
        case Reason::kCount:
            break;
    }
    return "unknown";
}

}  // namespace repiu::engine
