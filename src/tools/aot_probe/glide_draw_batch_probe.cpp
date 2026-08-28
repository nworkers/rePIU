#include "glide_draw_batch_probe.h"

#include "repiu/engine/glide_draw_batch.h"

#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace repiu::tools
{
namespace
{

using go = repiu::hle::GlideGateId;
using engine::QueueGlideDrawPrimitive;
using engine::Win32GlideBatchPrimitive;
using engine::Win32GlideDrawBatch;
using engine::Win32GlideDrawBatchFlushReason;

repiu::hle::GlideDrawVertex MakeVertex(float x)
{
    repiu::hle::GlideDrawVertex vertex{};
    vertex.x = x;
    return vertex;
}

// Records what a flush was asked to draw, so the ordering the boundary depends
// on can be asserted rather than assumed.
struct FlushRecorder
{
    std::vector<float> drawn_x;
    std::vector<Win32GlideBatchPrimitive> kinds;
    bool result = true;

    bool operator()(const repiu::hle::GlideDrawVertex* vertices,
                    std::size_t vertex_count,
                    Win32GlideBatchPrimitive primitive)
    {
        for (std::size_t index = 0; index < vertex_count; ++index)
        {
            drawn_x.push_back(vertices[index].x);
        }
        kinds.push_back(primitive);
        return result;
    }
};

bool QueueOne(Win32GlideDrawBatch* batch, float x,
              Win32GlideBatchPrimitive primitive, std::size_t vertex_count,
              bool* flush_required)
{
    std::vector<repiu::hle::GlideDrawVertex> vertices;
    for (std::size_t index = 0; index < vertex_count; ++index)
    {
        vertices.push_back(MakeVertex(x + static_cast<float>(index) * 0.01F));
    }
    return QueueGlideDrawPrimitive(batch, vertices.data(), vertices.size(),
                                   primitive, flush_required);
}

}  // namespace

bool RunGlideDrawBatchProbe()
{
    using engine::FlushGlideDrawBatch;
    using engine::IsGlideDrawBatchGate;
    using engine::kWin32GlideDrawBatchVertexCapacity;
    using engine::ResolveGlideDrawBatchEnabled;
    using engine::SnapshotGlideDrawBatch;

    // Task 439: promoted, so unset and empty are ON and only an explicit
    // `0|off|false` opts out. An unrecognised spelling stays fail-closed OFF,
    // matching every other promoted switch.
    const bool policy =
        ResolveGlideDrawBatchEnabled(nullptr) &&
        ResolveGlideDrawBatchEnabled("") &&
        ResolveGlideDrawBatchEnabled("1") &&
        ResolveGlideDrawBatchEnabled("on") &&
        ResolveGlideDrawBatchEnabled("true") &&
        !ResolveGlideDrawBatchEnabled("0") &&
        !ResolveGlideDrawBatchEnabled("off") &&
        !ResolveGlideDrawBatchEnabled("false") &&
        !ResolveGlideDrawBatchEnabled("yes");

    // Only the independent-primitive draw gates may queue. Everything else must
    // flush first, and the polygon fans are excluded because two fans inside one
    // `glBegin` would join into a single fan.
    const bool membership =
        IsGlideDrawBatchGate(go::kGrDrawTriangle) &&
        IsGlideDrawBatchGate(go::kGrAADrawTriangle) &&
        IsGlideDrawBatchGate(go::kGrDrawLine) &&
        IsGlideDrawBatchGate(go::kGrAADrawLine) &&
        IsGlideDrawBatchGate(go::kGrDrawPoint) &&
        IsGlideDrawBatchGate(go::kGrAADrawPoint) &&
        !IsGlideDrawBatchGate(go::kGrDrawPolygon) &&
        !IsGlideDrawBatchGate(go::kGrAADrawPolygon) &&
        !IsGlideDrawBatchGate(go::kGrDrawPolygonVertexList) &&
        !IsGlideDrawBatchGate(go::kGrBufferSwap) &&
        !IsGlideDrawBatchGate(go::kGrBufferClear) &&
        !IsGlideDrawBatchGate(go::kGrTexSource) &&
        !IsGlideDrawBatchGate(go::kGrColorMask) &&
        !IsGlideDrawBatchGate(go::kGrLfbLock);

    // Vertices come out in the order they went in, once, under one primitive
    // kind. This is the property the whole ordering argument rests on.
    auto batch = std::make_unique<Win32GlideDrawBatch>();
    bool flush_required = false;
    const bool queued =
        QueueOne(batch.get(), 1.0F, Win32GlideBatchPrimitive::kTriangles, 3U,
                 &flush_required) &&
        !flush_required &&
        QueueOne(batch.get(), 2.0F, Win32GlideBatchPrimitive::kTriangles, 3U,
                 &flush_required) &&
        !flush_required && batch->vertices.size() == 6U;
    FlushRecorder recorder;
    const bool flushed =
        FlushGlideDrawBatch(batch.get(),
                            Win32GlideDrawBatchFlushReason::kNonDrawGate,
                            std::ref(recorder)) &&
        recorder.drawn_x.size() == 6U &&
        recorder.drawn_x.front() == 1.0F &&
        recorder.drawn_x[3] == 2.0F &&
        recorder.kinds.size() == 1U &&
        recorder.kinds.front() == Win32GlideBatchPrimitive::kTriangles &&
        batch->vertices.empty() &&
        // Two triangles handed over in one flush is the whole point: the
        // reduction factor is primitives per flush, not vertices per flush.
        SnapshotGlideDrawBatch(*batch).drawn_primitive_count == 2U &&
        SnapshotGlideDrawBatch(*batch).max_batch_primitive_count == 2U;

    // Flushing nothing is a success that draws nothing, because the boundary
    // flushes before every non-draw gate and most find an empty batch.
    FlushRecorder empty_recorder;
    const bool empty_flush =
        FlushGlideDrawBatch(batch.get(),
                            Win32GlideDrawBatchFlushReason::kNonDrawGate,
                            std::ref(empty_recorder)) &&
        empty_recorder.kinds.empty() &&
        FlushGlideDrawBatch(nullptr,
                            Win32GlideDrawBatchFlushReason::kNonDrawGate,
                            std::ref(empty_recorder));

    // A different primitive kind cannot join the pending batch; the caller is
    // told to flush and then succeeds.
    auto mixed = std::make_unique<Win32GlideDrawBatch>();
    QueueOne(mixed.get(), 1.0F, Win32GlideBatchPrimitive::kTriangles, 3U,
             &flush_required);
    const bool rejected_kind =
        !QueueOne(mixed.get(), 5.0F, Win32GlideBatchPrimitive::kLines, 2U,
                  &flush_required) &&
        flush_required;
    FlushRecorder mixed_recorder;
    FlushGlideDrawBatch(mixed.get(),
                        Win32GlideDrawBatchFlushReason::kPrimitiveChange,
                        std::ref(mixed_recorder));
    const bool primitive_change =
        rejected_kind &&
        QueueOne(mixed.get(), 5.0F, Win32GlideBatchPrimitive::kLines, 2U,
                 &flush_required) &&
        !flush_required &&
        mixed->primitive == Win32GlideBatchPrimitive::kLines;

    // The capacity bound holds and never splits a primitive across a flush.
    auto full = std::make_unique<Win32GlideDrawBatch>();
    bool capacity_flush_requested = false;
    std::size_t accepted = 0;
    for (std::size_t index = 0;
         index < kWin32GlideDrawBatchVertexCapacity; ++index)
    {
        if (!QueueOne(full.get(), static_cast<float>(index),
                      Win32GlideBatchPrimitive::kTriangles, 3U,
                      &flush_required))
        {
            capacity_flush_requested = flush_required;
            break;
        }
        ++accepted;
    }
    const bool capacity =
        capacity_flush_requested &&
        full->vertices.size() + 3U > kWin32GlideDrawBatchVertexCapacity &&
        full->vertices.size() % 3U == 0U &&
        accepted * 3U == full->vertices.size();

    // A backend failure is counted and the vertices are dropped rather than
    // retried later, because a retry would draw them after whatever came next.
    auto failing = std::make_unique<Win32GlideDrawBatch>();
    QueueOne(failing.get(), 1.0F, Win32GlideBatchPrimitive::kTriangles, 3U,
             &flush_required);
    FlushRecorder failing_recorder;
    failing_recorder.result = false;
    const bool failure_counted =
        !FlushGlideDrawBatch(failing.get(),
                             Win32GlideDrawBatchFlushReason::kNonDrawGate,
                             std::ref(failing_recorder)) &&
        failing->vertices.empty() &&
        SnapshotGlideDrawBatch(*failing).failure_count == 1U &&
        SnapshotGlideDrawBatch(*failing).drawn_vertex_count == 0U &&
        SnapshotGlideDrawBatch(*failing).drawn_primitive_count == 0U &&
        SnapshotGlideDrawBatch(*failing).queued_vertex_count == 3U &&
        SnapshotGlideDrawBatch(*failing).queued_primitive_count == 1U &&
        SnapshotGlideDrawBatch(*failing).pending_primitive_count == 0U;

    const bool inert =
        !QueueGlideDrawPrimitive(nullptr, nullptr, 0U,
                                 Win32GlideBatchPrimitive::kTriangles,
                                 nullptr) &&
        !SnapshotGlideDrawBatch(Win32GlideDrawBatch{}).enabled;

    const bool all = policy && membership && queued && flushed &&
        empty_flush && primitive_change && capacity && failure_counted &&
        inert;

    std::cout << "glide_draw_batch_policy=" << (policy ? "true" : "false")
              << "\nglide_draw_batch_membership="
              << (membership ? "true" : "false")
              << "\nglide_draw_batch_queue=" << (queued ? "true" : "false")
              << "\nglide_draw_batch_order=" << (flushed ? "true" : "false")
              << "\nglide_draw_batch_empty_flush="
              << (empty_flush ? "true" : "false")
              << "\nglide_draw_batch_primitive_change="
              << (primitive_change ? "true" : "false")
              << "\nglide_draw_batch_capacity="
              << (capacity ? "true" : "false")
              << "\nglide_draw_batch_failure="
              << (failure_counted ? "true" : "false")
              << "\nglide_draw_batch_inert=" << (inert ? "true" : "false")
              << "\nglide_draw_batch_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
