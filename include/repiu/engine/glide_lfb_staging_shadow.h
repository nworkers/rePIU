#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "repiu/hle/glide_hle.h"

namespace repiu::engine
{

// Task 728. Whether the LFB staging surface still holds the frame buffer image
// of a particular buffer.
//
// Every write `grLfbLock` seeds the staging surface with a full
// `ReadbackFramebuffer` plus a 565 encode, which Task 724 measured as the whole
// of that gate's cost. The seed exists because a write lock may touch only some
// pixels, so the rest have to survive the unlock blit. But `grLfbUnlock`
// presents the *entire* staging surface, so right after a successful unlock the
// frame buffer holds what the staging surface holds -- and the next lock does
// not need to ask the GPU for it back.
//
// This is the same idea Task 476 applied to the region gates on the same
// surface; it is extended here to the lock/unlock path, where the state has to
// survive the state-setter gates a guest issues between two locks.
enum class GlideLfbStagingShadowInvalidation : std::uint32_t
{
    // A gate that may change frame buffer pixels, or one this layer does not
    // recognize. Split by kind, because "the shadow did not survive" is only
    // useful next to what did not let it: a swap is a property of the frame
    // loop and cannot be argued with, while an incidental gate might be.
    kSwapGate = 0,
    kDrawGate,
    kClearGate,
    kRegionGate,
    kOtherGate,
    // The lock handed the surface to the guest, which now owns its contents.
    kLockHandoff,
    // The unlock blit failed, so what the frame buffer holds is unknown.
    kPresentFailed,
    // The unlock presented flipped, so the frame buffer is no longer row-wise
    // equal to the staging surface.
    kFlippedPresent,
    // Geometry or pixel format moved out from under the shadow.
    kSurfaceMismatch,
    // Teardown.
    kShutdown,
    kCount
};

inline constexpr std::size_t kGlideLfbStagingShadowReasonCount =
    static_cast<std::size_t>(GlideLfbStagingShadowInvalidation::kCount);

struct GlideLfbStagingShadowState
{
    bool valid = false;
    // What the shadow stands for while `valid`. A lock naming anything else
    // re-seeds rather than handing out another buffer's pixels.
    std::uint32_t buffer = 0;
    std::uint32_t color_format = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t lock_count = 0;
    // Locks whose shadow matched. Counted whether or not the seed was actually
    // skipped, so census mode measures the opportunity without taking it.
    std::uint32_t reusable_lock_count = 0;
    std::uint32_t reused_lock_count = 0;
    std::uint32_t seed_count = 0;
    std::uint32_t validate_count = 0;
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
    std::uint32_t validate_count = 0;
    std::uint32_t invalidate_counts[kGlideLfbStagingShadowReasonCount] = {};
};

bool ResolveGlideLfbStagingShadowSetting(std::string_view setting);

// Counting only, no behavior change. Implied by reuse, because a run that skips
// seeds without counting them cannot be read afterwards.
bool GlideLfbStagingShadowCensusEnabled();

// Actually skip the seed when the shadow matches.
bool GlideLfbStagingReuseEnabled();

// True only for gates confirmed to leave frame buffer pixels and LFB encoding
// alone. Everything else, including `kUnknown` and any gate added later,
// invalidates -- a gate nobody classified must not be assumed harmless.
bool GlideOrdinalPreservesLfbStagingShadow(repiu::hle::GlideGateId gate_id);

// Which invalidation a non-preserving gate is. Callers pass any gate id; a
// preserving one still classifies as `kOtherGate`, because the caller decides
// whether to invalidate at all.
GlideLfbStagingShadowInvalidation ClassifyGlideLfbStagingShadowGate(
    repiu::hle::GlideGateId gate_id);

bool CanReuseGlideLfbStagingShadow(const GlideLfbStagingShadowState& state,
                                   std::uint32_t buffer,
                                   std::uint32_t color_format,
                                   std::uint32_t width,
                                   std::uint32_t height);

void ValidateGlideLfbStagingShadow(GlideLfbStagingShadowState* state,
                                   std::uint32_t buffer,
                                   std::uint32_t color_format,
                                   std::uint32_t width,
                                   std::uint32_t height);

void InvalidateGlideLfbStagingShadow(
    GlideLfbStagingShadowState* state,
    GlideLfbStagingShadowInvalidation reason);

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
