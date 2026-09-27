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

    // Task 753. A tick delivered at a stack level the guest never returns to:
    // every later read and interrupt is below it, so the stack test cannot
    // retire its frame.
    const std::uint16_t service = JammaInputKeyMask(JammaInputKey::kService);
    // (1) The handler's return ends the frame, and reads outside a handler
    // stop being answered by it.
    JammaInputTimeline return_timeline;
    return_timeline.Reset(0U, 0U);
    return_timeline.RecordKeyEdge(100U, JammaInputKey::kService, true);
    return_timeline.EnqueueTimerTick(150U);
    return_timeline.RecordKeyEdge(200U, JammaInputKey::kService, false);
    std::uint16_t in_handler = 0U;
    std::uint16_t after_return = 0xffffU;
    const bool return_case =
        return_timeline.BeginTimerInterrupt(5000U, 4988U, 0x1234U) &&
        return_timeline.TryReplayPressedMask(4900U, &in_handler) &&
        in_handler == service &&
        // A chained handler's `iret` pops a frame no interrupt pushed.
        !return_timeline.EndTimerInterrupt(4976U) &&
        return_timeline.TryReplayPressedMask(4900U, &in_handler) &&
        return_timeline.EndTimerInterrupt(4988U) &&
        !return_timeline.TryReplayPressedMask(3000U, &after_return);
    const auto return_snapshot = return_timeline.Snapshot();

    // (2) A nested frame goes with the outer one when only the outer return
    // is seen.
    JammaInputTimeline nested_timeline;
    nested_timeline.Reset(0U, 0U);
    nested_timeline.EnqueueTimerTick(10U);
    nested_timeline.EnqueueTimerTick(20U);
    const bool nested_case =
        nested_timeline.BeginTimerInterrupt(5000U, 4988U) &&
        nested_timeline.BeginTimerInterrupt(4900U, 4888U) &&
        nested_timeline.EndTimerInterrupt(4988U) &&
        nested_timeline.Snapshot().replay_frame_depth == 0U &&
        nested_timeline.Snapshot().replay_frame_end_count == 2U;

    // (3) Without the return (Win32 runs the guest's `iret` natively) the age
    // rule retires the frame: it answers while it is recent, and not after
    // more than kReplayFrameStaleBegins later interrupts have begun.
    JammaInputTimeline stale_timeline;
    stale_timeline.Reset(0U, 0U);
    stale_timeline.RecordKeyEdge(100U, JammaInputKey::kService, true);
    stale_timeline.EnqueueTimerTick(150U);
    stale_timeline.RecordKeyEdge(200U, JammaInputKey::kService, false);
    bool stale_case = stale_timeline.BeginTimerInterrupt(5000U, 4988U);
    std::uint16_t outside_handler = 0U;
    bool answered_while_recent = true;
    for (std::uint32_t index = 0;
         index < JammaInputTimeline::kReplayFrameStaleBegins; ++index)
    {
        stale_case = stale_case &&
            stale_timeline.EnqueueTimerTick(300U + index) &&
            stale_timeline.BeginTimerInterrupt(3000U, 2988U);
        // Above the newer frame and below the stranded one.
        answered_while_recent = answered_while_recent &&
            stale_timeline.TryReplayPressedMask(3000U, &outside_handler) &&
            outside_handler == service;
    }
    stale_case = stale_case &&
        stale_timeline.EnqueueTimerTick(1000U) &&
        stale_timeline.BeginTimerInterrupt(3000U, 2988U) &&
        !stale_timeline.TryReplayPressedMask(3000U, &outside_handler);
    const auto stale_snapshot = stale_timeline.Snapshot();

    // (4) The latest recorded state answers only when a scripted keyboard
    // asked for it.
    JammaInputTimeline latest_timeline;
    latest_timeline.Reset(0U, 0U);
    latest_timeline.RecordKeyEdge(100U, JammaInputKey::kService, true);
    std::uint16_t latest_state = 0U;
    const bool latest_off =
        !latest_timeline.TryLatestPressedMask(&latest_state);
    latest_timeline.ServeLiveReadsFromLatestState(true);
    const bool latest_case = latest_off &&
        latest_timeline.TryLatestPressedMask(&latest_state) &&
        latest_state == service &&
        latest_timeline.Snapshot().latest_state_read_count == 1U;

    const bool frame_end_switch =
        engine::ResolveJammaReplayFrameEnd(nullptr) &&
        engine::ResolveJammaReplayFrameEnd("1") &&
        !engine::ResolveJammaReplayFrameEnd("0");
    const bool task753 = return_case &&
        return_snapshot.replay_frame_end_count == 1U &&
        return_snapshot.replay_frame_depth == 0U &&
        return_snapshot.history_size == 1U &&
        nested_case &&
        stale_case && answered_while_recent &&
        stale_snapshot.replay_frame_stale_count == 1U &&
        stale_snapshot.replay_frame_depth == 0U &&
        stale_snapshot.history_size == 0U &&
        latest_case && frame_end_switch;
    const bool valid = queued && replayed && frame_retired && missing_due &&
        task753 &&
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
              << ",missing=" << snapshot.replay_missing_due_count
              << ",frame_return=" << (return_case ? "true" : "false")
              << ",frame_nested=" << (nested_case ? "true" : "false")
              << ",frame_stale=" << (stale_case && answered_while_recent
                                         ? "true" : "false")
              << ",stale_retired=" << stale_snapshot.replay_frame_stale_count
              << ",latest_state=" << (latest_case ? "true" : "false")
              << "\n";
    return valid;
}

}  // namespace repiu::tools
