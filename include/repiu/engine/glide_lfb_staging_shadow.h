#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "repiu/hle/glide_hle.h"

namespace repiu::engine
{

// Tasks 728 and 729. Host-owned copies of what each Glide color buffer holds,
// so a write `grLfbLock` does not have to read the frame buffer back from the
// GPU to seed its staging surface.
//
// Task 728 kept one shadow and let it be the staging surface itself. Measured,
// it never once survived: all 304 invalidations in a 30-second run were
// `grBufferSwap`, because the guest's pattern is
// `lock -> write -> unlock -> swap -> lock`.
//
// Task 729 measured what fills that window and found it empty: 303 of 304
// intervals contained exactly one swap and nothing else -- no clear, no draw,
// no region write. So the obstacle was never that the frame buffer changes; it
// is that a swap moves which buffer the next lock names.
//
// That is what this models. A Glide buffer swap is a page flip, so after it the
// back buffer holds what the front buffer held. Keeping one copy per buffer and
// exchanging them at the swap reproduces that exactly, and a lock can then be
// served from the copy instead of the GPU.
//
// Two consequences of holding copies rather than the staging surface itself:
// the shadow survives the lock handing that surface to the guest, and it
// survives a swap.
enum class GlideLfbStagingShadowInvalidation : std::uint32_t
{
    // A gate that may change frame buffer pixels, or one this layer does not
    // recognize. Split by kind, because "the shadow did not survive" is only
    // useful next to what did not let it.
    kSwapGate = 0,
    kDrawGate,
    kClearGate,
    kRegionGate,
    kOtherGate,
    // The unlock blit failed, so what the frame buffer holds is unknown.
    kPresentFailed,
    // The unlock presented flipped, so the frame buffer is no longer row-wise
    // equal to the staging surface.
    kFlippedPresent,
    // Geometry or pixel format moved out from under the shadow.
    kSurfaceMismatch,
    // The host could not hold a copy.
    kStorageFailure,
    kCount
};

inline constexpr std::size_t kGlideLfbStagingShadowReasonCount =
    static_cast<std::size_t>(GlideLfbStagingShadowInvalidation::kCount);

// GrBuffer_t front is 0 and back is 1, which this indexes directly. Anything
// else is not a color buffer this layer shadows.
inline constexpr std::size_t kGlideLfbStagingShadowBufferCount = 2U;

bool GlideLfbStagingShadowBufferIndexed(std::uint32_t buffer);

// One buffer's copy of its 565 contents.
struct GlideLfbBufferShadow
{
    bool valid = false;
    std::uint32_t color_format = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;
};

struct GlideLfbStagingShadowState
{
    GlideLfbBufferShadow buffers[kGlideLfbStagingShadowBufferCount];
    // Where draws and clears land. Glide starts on the back buffer and
    // `grRenderBuffer` moves it; a 30-second run called that gate once, at
    // startup, so in practice only the back shadow is ever broken by drawing.
    std::uint32_t render_buffer = 1U;

    std::uint32_t lock_count = 0;
    // Locks whose shadow matched. Counted whether or not the seed was actually
    // skipped, so census mode measures the opportunity without taking it.
    std::uint32_t reusable_lock_count = 0;
    std::uint32_t reused_lock_count = 0;
    std::uint32_t seed_count = 0;
    std::uint32_t store_count = 0;
    std::uint32_t swap_rotation_count = 0;
    // Counted on the valid-to-invalid transition only. Counting every call
    // would count each gate of a run whose shadow is already invalid.
    std::uint32_t invalidate_counts[kGlideLfbStagingShadowReasonCount] = {};
};

struct GlideLfbStagingShadowSnapshot
{
    bool census_enabled = false;
    bool reuse_enabled = false;
    std::uint32_t lock_count = 0;
    std::uint32_t reusable_lock_count = 0;
    std::uint32_t reused_lock_count = 0;
    std::uint32_t seed_count = 0;
    std::uint32_t store_count = 0;
    std::uint32_t swap_rotation_count = 0;
    std::uint32_t invalidate_counts[kGlideLfbStagingShadowReasonCount] = {};
};

bool ResolveGlideLfbStagingShadowSetting(std::string_view setting);

// Counting only, no behavior change. Implied by reuse, because a run that skips
// seeds without counting them cannot be read afterwards.
bool GlideLfbStagingShadowCensusEnabled();

// Actually serve a lock from the shadow when it matches.
bool GlideLfbStagingReuseEnabled();

// True only for gates confirmed to leave frame buffer pixels and LFB encoding
// alone. Everything else, including `kUnknown` and any gate added later,
// invalidates -- a gate nobody classified must not be assumed harmless.
//
// `kGrBufferSwap` is preserving here: it does not destroy either buffer's
// contents, it exchanges them, which `RotateGlideLfbStagingShadows` does.
bool GlideOrdinalPreservesLfbStagingShadow(repiu::hle::GlideGateId gate_id);

// Which invalidation a non-preserving gate is.
GlideLfbStagingShadowInvalidation ClassifyGlideLfbStagingShadowGate(
    repiu::hle::GlideGateId gate_id);

// Whether a gate writes only the current render target, as opposed to leaving
// the whole shadow set untrustworthy.
bool GlideOrdinalTargetsRenderBufferOnly(repiu::hle::GlideGateId gate_id);

void SetGlideLfbStagingShadowRenderBuffer(GlideLfbStagingShadowState* state,
                                          std::uint32_t buffer);

bool CanReuseGlideLfbStagingShadow(const GlideLfbStagingShadowState& state,
                                   std::uint32_t buffer,
                                   std::uint32_t color_format,
                                   std::uint32_t width,
                                   std::uint32_t height);

// Copies the shadow of `buffer` into a staging surface. Returns false without
// touching `pixels` when the shadow cannot serve this surface.
bool LoadGlideLfbStagingShadow(const GlideLfbStagingShadowState& state,
                               std::uint32_t buffer,
                               std::uint8_t* pixels,
                               std::size_t byte_count,
                               std::uint32_t color_format,
                               std::uint32_t width,
                               std::uint32_t height);

// Takes a copy of a staging surface as the new contents of `buffer`. A copy it
// cannot hold invalidates rather than half-storing.
void StoreGlideLfbStagingShadow(GlideLfbStagingShadowState* state,
                                std::uint32_t buffer,
                                const std::uint8_t* pixels,
                                std::size_t byte_count,
                                std::uint32_t color_format,
                                std::uint32_t width,
                                std::uint32_t height);

// The page flip: after a buffer swap the back buffer holds what the front one
// held. Exchanges the two shadows rather than discarding them.
void RotateGlideLfbStagingShadows(GlideLfbStagingShadowState* state);

void InvalidateGlideLfbStagingShadowBuffer(
    GlideLfbStagingShadowState* state,
    std::uint32_t buffer,
    GlideLfbStagingShadowInvalidation reason);

void InvalidateGlideLfbStagingShadows(
    GlideLfbStagingShadowState* state,
    GlideLfbStagingShadowInvalidation reason);

// Applies one dispatched gate to the shadow set: rotate on a swap, invalidate
// the render target for a targeted gate, invalidate everything otherwise.
void ApplyGlideLfbStagingShadowGate(GlideLfbStagingShadowState* state,
                                    repiu::hle::GlideGateId gate_id);

// Records one write lock's outcome. `reused` implies `reusable`; a lock that
// was not reused seeded instead.
void NoteGlideLfbStagingShadowLock(GlideLfbStagingShadowState* state,
                                   bool reusable,
                                   bool reused);

GlideLfbStagingShadowSnapshot SnapshotGlideLfbStagingShadow(
    const GlideLfbStagingShadowState& state);

const char* GlideLfbStagingShadowInvalidationName(
    GlideLfbStagingShadowInvalidation reason);

}  // namespace repiu::engine
