#include "jamma_input_timeline_probe.h"

#include "repiu/engine/jamma_input_timeline.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{

bool RunJammaInputTimelineProbe()
{
    using engine::JammaInputKey;
    using engine::JammaInputKeyMask;
    using engine::JammaInputTimeline;

    JammaInputTimeline timeline;
    timeline.Reset(100U, 0U);
    timeline.RecordKeyEdge(200U, JammaInputKey::kP1UpLeft, true);
    timeline.RecordKeyEdge(400U, JammaInputKey::kP1UpLeft, false);

    const bool queued =
        timeline.EnqueueTimerTick(150U) &&
        timeline.EnqueueTimerTick(250U) &&
        timeline.EnqueueTimerTick(450U);
    std::uint16_t state_before = 0xffffU;
    std::uint16_t state_pressed = 0U;
    std::uint16_t state_outer = 0xffffU;
    std::uint16_t state_released = 0xffffU;
    const bool replayed =
        timeline.BeginTimerInterrupt(1100U, 1000U) &&
        timeline.TryReplayPressedMask(900U, &state_before) &&
        timeline.BeginTimerInterrupt(900U, 800U) &&
        timeline.TryReplayPressedMask(700U, &state_pressed) &&
        timeline.TryReplayPressedMask(900U, &state_outer) &&
        timeline.BeginTimerInterrupt(1100U, 1000U) &&
        timeline.TryReplayPressedMask(900U, &state_released);

    const bool frame_retired =
        !timeline.TryReplayPressedMask(1100U, &state_released);
    const bool missing_due = !timeline.BeginTimerInterrupt(1100U, 1000U);

    const auto snapshot = timeline.Snapshot();
    JammaInputTimeline pruning_timeline;
    pruning_timeline.Reset(0U, 0U);
    bool pruning_sequence = true;
    for (std::uint32_t index = 0; index < 300U; ++index)
    {
        const std::uint64_t timestamp = 1000U + index * 10U;
        const bool pressed = (index & 1U) == 0U;
        pruning_timeline.RecordKeyEdge(
            timestamp, JammaInputKey::kP1UpLeft, pressed);
        std::uint16_t sampled_state = 0xffffU;
        const bool queued_tick = pruning_timeline.EnqueueTimerTick(timestamp);
        const bool began = pruning_timeline.BeginTimerInterrupt(1100U, 1000U);
        const bool sampled =
            pruning_timeline.TryReplayPressedMask(900U, &sampled_state);
        const std::uint16_t expected_state = pressed
            ? JammaInputKeyMask(JammaInputKey::kP1UpLeft)
            : 0U;
        pruning_sequence = pruning_sequence && queued_tick && began && sampled &&
            sampled_state == expected_state;
        pruning_timeline.TryReplayPressedMask(1100U, &sampled_state);
    }
    const auto pruning_snapshot = pruning_timeline.Snapshot();
    const bool valid = queued && replayed && frame_retired && missing_due &&
        state_before == 0U &&
        state_pressed == JammaInputKeyMask(JammaInputKey::kP1UpLeft) &&
        state_outer == 0U &&
        state_released == 0U &&
        snapshot.edge_count == 2U &&
        snapshot.due_enqueued_count == 3U &&
        snapshot.replay_begin_count == 3U &&
        snapshot.replay_read_count == 4U &&
        snapshot.replay_missing_due_count == 1U &&
        snapshot.replay_frame_retire_count == 3U &&
        snapshot.replay_frame_overflow_count == 0U &&
        snapshot.due_size == 0U &&
        snapshot.replay_frame_depth == 0U &&
        pruning_sequence &&
        pruning_snapshot.edge_count == 300U &&
        pruning_snapshot.history_pruned_count == 300U &&
        pruning_snapshot.history_peak_size == 1U &&
        pruning_snapshot.history_overflow_count == 0U &&
        pruning_snapshot.history_coverage_miss_count == 0U &&
        pruning_snapshot.history_size == 0U;
    std::cout << "jamma_input_timeline_probe="
              << (valid ? "true" : "false")
              << ",edges=" << snapshot.edge_count
              << ",replays=" << snapshot.replay_begin_count
              << ",reads=" << snapshot.replay_read_count
              << ",retired=" << snapshot.replay_frame_retire_count
              << ",frame_overflow=" << snapshot.replay_frame_overflow_count
              << ",pruned=" << pruning_snapshot.history_pruned_count
              << ",history_peak=" << pruning_snapshot.history_peak_size
              << ",history_overflow="
              << pruning_snapshot.history_overflow_count
              << ",coverage_miss="
              << pruning_snapshot.history_coverage_miss_count
              << ",missing=" << snapshot.replay_missing_due_count << "\n";
    return valid;
}

}  // namespace repiu::tools
