#pragma once

// Task 568. Writing the bytes of a segment-override patch, apart from deciding
// which memory to open for it.
//
// The two are different kinds of knowledge and they were in the same place.
// What to write is a fact about what the emitter emitted, and it belongs beside
// the emitter; unprotecting a page and flushing an instruction cache is the
// engine's business. Keeping them together meant the only way to exercise the
// patcher was to link the whole engine -- which on Linux drags OpenGL into a
// probe deliberately built to have no platform layer, so the x64 slot could not
// be verified against the real patcher at all.
//
// The split is on that seam. `PatchAotSegmentOverrideSites` takes bytes that
// are already writable and does nothing else.

#include "repiu/runtime/aot_code_cache.h"

#include <cstdint>
#include <vector>

namespace repiu::runtime
{

enum class AotSegmentAccessPolicy : std::uint8_t
{
    kUnresolved = 0,
    kNativeFolded,
    kHleLowMemory,
};

struct AotSegmentResolution
{
    std::uint32_t shadow_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t base = 0;
    std::uint32_t limit = 0;
    std::uint32_t flags = 0;
    AotSegmentAccessPolicy policy = AotSegmentAccessPolicy::kUnresolved;
};

struct AotSegmentTable
{
    AotSegmentResolution segments[6];
};

struct AotSegmentOverridePatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t hle_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
};

// Patch every segment-override site in `sites` into `bytes`, which must already
// be writable and must be the base of the placed image the offsets refer to.
// Returns how many sites were considered.
std::uint32_t PatchAotSegmentOverrideSites(
    std::uint8_t* bytes,
    const std::vector<AotSegmentOverrideSite>& sites,
    const AotSegmentTable& table,
    AotSegmentOverridePatchStats* stats);

}  // namespace repiu::runtime
