#pragma once

#include "repiu/hle/glide_hle.h"
#include "repiu/hle/glide_vertex.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace repiu::engine
{

// Task 438: `grDrawTriangle` costs one host rendezvous per triangle, and a real
// gameplay section issues 670 of them per frame -- 69.9% of all Glide gate
// crossings. Queueing the triangles and handing them over once per ordering
// boundary replaces those with roughly one rendezvous per non-draw gate, about
// 100 per frame.
//
// This is worth doing only because Tasks 365 and 437 elide repeated setters:
// an elided setter never reaches the gate handler and so never forces a flush.
// Without the elision every one of the ~290 setter calls per frame would be a
// flush point and a batch would hold two or three triangles.

// Independent primitives only. `GL_TRIANGLE_FAN`, which `grDrawPolygon` uses,
// cannot be concatenated inside one `glBegin`/`glEnd` and is therefore never
// queued -- it draws immediately as before. PIU calls no polygon entry point.
constexpr std::size_t kGlideDrawBatchVertexCapacity = 3072U;

// The primitive kinds a batch can hold. Kept as our own enum rather than the GL
// constants so the boundary never depends on a GL header, and so that "nothing
// pending" has a value of its own -- `GL_POINTS` is zero, which would otherwise
// be indistinguishable from an empty batch.
enum class GlideBatchPrimitive : std::uint32_t
{
    kNone = 0,
    kPoints,
    kLines,
    kTriangles
};

// There is no teardown reason: `grBufferSwap`, `grSstWinClose` and
// `grGlideShutdown` are all non-draw gates, so the general rule already flushes
// before each of them. What can remain pending at process exit is a fraction of
// a frame that was never presented, and the snapshot reports it as
// `pending_vertex_count` rather than pretending it was drawn.
enum class GlideDrawBatchFlushReason
{
    kNonDrawGate,
    kPrimitiveChange,
    kCapacity,
    kCount
};

struct GlideDrawBatch
{
    bool enabled = false;
    // A pending batch always holds at least one primitive, all of this kind.
    GlideBatchPrimitive primitive = GlideBatchPrimitive::kNone;
    std::vector<repiu::hle::GlideDrawVertex> vertices;
    std::uint64_t queued_vertex_count = 0;
    std::uint64_t drawn_vertex_count = 0;
    // Primitives, not vertices, because primitives are what used to cost one
    // rendezvous each: drawn primitives divided by flushes *is* the reduction
    // factor, while a vertex mean would read three times too high for triangles.
    std::uint64_t queued_primitive_count = 0;
    std::uint64_t drawn_primitive_count = 0;
    std::uint32_t pending_primitive_count = 0;
    std::uint64_t flush_count = 0;
    std::uint64_t failure_count = 0;
    std::uint32_t max_batch_primitive_count = 0;
    std::array<std::uint64_t,
               static_cast<std::size_t>(
                   GlideDrawBatchFlushReason::kCount)>
        flush_reasons = {};
};

// Aggregates only, matching the setter cache snapshot, so the summary can be
// taken without copying the vertex storage.
struct GlideDrawBatchSnapshot
{
    bool enabled = false;
    std::uint64_t queued_vertex_count = 0;
    std::uint64_t drawn_vertex_count = 0;
    std::uint64_t queued_primitive_count = 0;
    std::uint64_t drawn_primitive_count = 0;
    std::uint64_t flush_count = 0;
    std::uint64_t failure_count = 0;
    std::uint32_t max_batch_primitive_count = 0;
    std::uint32_t pending_vertex_count = 0;
    std::uint32_t pending_primitive_count = 0;
    std::array<std::uint64_t,
               static_cast<std::size_t>(
                   GlideDrawBatchFlushReason::kCount)>
        flush_reasons = {};
};

// Task 439: on by default after a paired gameplay A/B. Unset and empty mean
// ON; an explicit `0|off|false` restores the per-primitive rendezvous as the
// regression control.
bool ResolveGlideDrawBatchEnabled(const char* setting);
bool GlideDrawBatchEnabled();

// True for the gates whose primitives may be queued. Everything else -- state,
// queries, swap, clear, LFB, downloads, and the polygon fans -- flushes first.
bool IsGlideDrawBatchGate(repiu::hle::GlideGateId gate_id);

// Appends one primitive. Returns false only when the batch could not take it
// and the caller must draw it directly; a flush failure is recorded and the
// vertices are dropped rather than drawn out of order.
bool QueueGlideDrawPrimitive(GlideDrawBatch* batch,
                             const repiu::hle::GlideDrawVertex* vertices,
                             std::size_t vertex_count,
                             GlideBatchPrimitive primitive,
                             bool* flush_required);

// Hands the pending vertices to `draw`, which must apply them in order. A batch
// that is empty is a success with no call, because the boundary flushes before
// every non-draw gate and most find nothing pending.
template <typename DrawFn>
bool FlushGlideDrawBatch(GlideDrawBatch* batch,
                         GlideDrawBatchFlushReason reason,
                         DrawFn&& draw)
{
    if (batch == nullptr || batch->vertices.empty())
    {
        return true;
    }
    const std::uint32_t count =
        static_cast<std::uint32_t>(batch->vertices.size());
    const std::uint32_t primitive_count = batch->pending_primitive_count;
    const GlideBatchPrimitive primitive = batch->primitive;
    const bool drawn = draw(batch->vertices.data(), batch->vertices.size(),
                            primitive);
    ++batch->flush_count;
    batch->flush_reasons[static_cast<std::size_t>(reason)] += 1U;
    if (drawn)
    {
        batch->drawn_vertex_count += count;
        batch->drawn_primitive_count += primitive_count;
    }
    else
    {
        ++batch->failure_count;
    }
    if (primitive_count > batch->max_batch_primitive_count)
    {
        batch->max_batch_primitive_count = primitive_count;
    }
    batch->vertices.clear();
    batch->pending_primitive_count = 0;
    batch->primitive = GlideBatchPrimitive::kNone;
    return drawn;
}

GlideDrawBatchSnapshot SnapshotGlideDrawBatch(
    const GlideDrawBatch& batch);

}  // namespace repiu::engine
