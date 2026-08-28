#include "repiu/engine/glide_draw_batch.h"

#include "repiu/runtime/env_toggle.h"

#include <cstdlib>

namespace repiu::engine
{
namespace
{

using go = repiu::hle::GlideGateId;

}  // namespace

bool ResolveGlideDrawBatchEnabled(const char* setting)
{
    // Task 439 promoted this on a paired gameplay A/B: batches averaged 16.02
    // primitives (peak 332), the Glide gate fell from 10.35% to 8.40% of
    // guest-run, per-crossing cost fell 23.7%, and failures, voided setters and
    // implementation gaps were all zero with no visual difference. An explicit
    // `0|off|false` keeps the per-primitive path as the regression control.
    return repiu::runtime::ResolvePromotedToggle(setting);
}

bool GlideDrawBatchEnabled()
{
    static const bool enabled =
        ResolveGlideDrawBatchEnabled(std::getenv("REPIU_GLIDE_DRAW_BATCH"));
    return enabled;
}

bool IsGlideDrawBatchGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrDrawTriangle:
        case go::kGrAADrawTriangle:
        case go::kGrDrawLine:
        case go::kGrAADrawLine:
        case go::kGrDrawPoint:
        case go::kGrAADrawPoint:
            return true;
        // The polygon entry points map to `GL_TRIANGLE_FAN`, which does not
        // concatenate: two fans in one `glBegin` would join into a single fan
        // around the first vertex. They keep drawing immediately. PIU calls
        // none of them, so nothing measurable is left behind.
        default:
            return false;
    }
}

bool QueueGlideDrawPrimitive(Win32GlideDrawBatch* batch,
                             const repiu::hle::GlideDrawVertex* vertices,
                             const std::size_t vertex_count,
                             const Win32GlideBatchPrimitive primitive,
                             bool* flush_required)
{
    if (flush_required != nullptr)
    {
        *flush_required = false;
    }
    if (batch == nullptr || vertices == nullptr || vertex_count == 0U ||
        primitive == Win32GlideBatchPrimitive::kNone)
    {
        return false;
    }
    // One primitive must never straddle a flush, so a batch that cannot hold
    // the whole primitive is flushed first rather than partially filled.
    if (vertex_count > kWin32GlideDrawBatchVertexCapacity)
    {
        return false;
    }
    if (!batch->vertices.empty() &&
        (batch->primitive != primitive ||
         batch->vertices.size() + vertex_count >
             kWin32GlideDrawBatchVertexCapacity))
    {
        if (flush_required != nullptr)
        {
            *flush_required = true;
        }
        return false;
    }
    if (batch->vertices.capacity() < kWin32GlideDrawBatchVertexCapacity)
    {
        batch->vertices.reserve(kWin32GlideDrawBatchVertexCapacity);
    }
    batch->enabled = true;
    batch->primitive = primitive;
    batch->vertices.insert(batch->vertices.end(), vertices,
                           vertices + vertex_count);
    batch->queued_vertex_count += vertex_count;
    ++batch->queued_primitive_count;
    ++batch->pending_primitive_count;
    return true;
}

Win32GlideDrawBatchSnapshot SnapshotGlideDrawBatch(
    const Win32GlideDrawBatch& batch)
{
    Win32GlideDrawBatchSnapshot snapshot;
    snapshot.enabled = batch.enabled;
    snapshot.queued_vertex_count = batch.queued_vertex_count;
    snapshot.drawn_vertex_count = batch.drawn_vertex_count;
    snapshot.queued_primitive_count = batch.queued_primitive_count;
    snapshot.drawn_primitive_count = batch.drawn_primitive_count;
    snapshot.flush_count = batch.flush_count;
    snapshot.failure_count = batch.failure_count;
    snapshot.max_batch_primitive_count = batch.max_batch_primitive_count;
    snapshot.pending_vertex_count =
        static_cast<std::uint32_t>(batch.vertices.size());
    snapshot.pending_primitive_count = batch.pending_primitive_count;
    snapshot.flush_reasons = batch.flush_reasons;
    return snapshot;
}

}  // namespace repiu::engine
