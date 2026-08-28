#ifndef REPIU_PLATFORM_WIN32_JAMMA_INPUT_TIMELINE_H_
#define REPIU_PLATFORM_WIN32_JAMMA_INPUT_TIMELINE_H_

#include "repiu/input/jamma_input_key.h"

#include <cstdint>
#include <atomic>

namespace repiu::engine
{

// The enumeration moved to repiu/input/jamma_input_key.h so the configuration
// layer, which is platform neutral, can name the same inputs. These aliases
// keep the win32 call sites reading as they did.
using repiu::input::JammaInputKey;
using repiu::input::JammaInputKeyMask;

struct Win32JammaInputTimelineSnapshot
{
    std::uint64_t edge_count = 0;
    std::uint64_t history_pruned_count = 0;
    std::uint64_t history_overflow_count = 0;
    std::uint64_t history_coverage_miss_count = 0;
    std::uint64_t due_enqueued_count = 0;
    std::uint64_t due_overflow_count = 0;
    std::uint64_t replay_begin_count = 0;
    std::uint64_t replay_read_count = 0;
    std::uint64_t replay_missing_due_count = 0;
    std::uint64_t replay_frame_retire_count = 0;
    std::uint64_t replay_frame_overflow_count = 0;
    std::uint32_t history_size = 0;
    std::uint32_t history_peak_size = 0;
    std::uint32_t due_size = 0;
    std::uint32_t replay_frame_depth = 0;
};

class Win32JammaInputTimeline
{
public:
    static constexpr std::uint32_t kHistoryCapacity = 256U;
    static constexpr std::uint32_t kDueCapacity = 64U;

    void Reset(std::uint64_t timestamp_nanoseconds,
               std::uint16_t pressed_mask);
    void RecordKeyEdge(std::uint64_t timestamp_nanoseconds,
                       JammaInputKey key,
                       bool pressed);
    void RecordAllReleased(std::uint64_t timestamp_nanoseconds);

    bool EnqueueTimerTick(std::uint64_t due_timestamp_nanoseconds);
    void ClearTimerTicks();
    bool BeginTimerInterrupt(std::uint32_t pre_interrupt_esp,
                             std::uint32_t interrupt_frame_esp);
    bool TryReplayPressedMask(std::uint32_t current_esp,
                              std::uint16_t* pressed_mask);

    Win32JammaInputTimelineSnapshot Snapshot() const;

private:
    struct HistoryEntry
    {
        std::uint64_t timestamp_nanoseconds = 0;
        std::uint16_t pressed_mask = 0;
    };

    struct ReplayFrame
    {
        std::uint64_t timestamp_nanoseconds = 0;
        std::uint32_t interrupt_frame_esp = 0;
    };

    std::uint16_t StateAtLocked(std::uint64_t timestamp_nanoseconds);
    void RecordStateLocked(std::uint64_t timestamp_nanoseconds,
                           std::uint16_t pressed_mask);
    void RetireReplayFramesLocked(std::uint32_t current_esp);
    void PruneHistoryLocked();

    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    HistoryEntry history_[kHistoryCapacity] = {};
    std::uint32_t history_size_ = 0;
    std::uint64_t history_floor_timestamp_ = 0;
    std::uint16_t history_floor_pressed_mask_ = 0;
    std::uint16_t latest_pressed_mask_ = 0;
    std::uint32_t history_peak_size_ = 0;

    std::uint64_t due_timestamps_[kDueCapacity] = {};
    std::uint32_t due_head_ = 0;
    std::uint32_t due_size_ = 0;
    std::uint64_t last_due_timestamp_ = 0;
    bool has_due_timestamp_ = false;

    ReplayFrame replay_frames_[kDueCapacity] = {};
    std::uint32_t replay_frame_depth_ = 0;

    std::uint64_t edge_count_ = 0;
    std::uint64_t history_pruned_count_ = 0;
    std::uint64_t history_overflow_count_ = 0;
    std::uint64_t history_coverage_miss_count_ = 0;
    std::uint64_t due_enqueued_count_ = 0;
    std::uint64_t due_overflow_count_ = 0;
    std::uint64_t replay_begin_count_ = 0;
    std::uint64_t replay_read_count_ = 0;
    std::uint64_t replay_missing_due_count_ = 0;
    std::uint64_t replay_frame_retire_count_ = 0;
    std::uint64_t replay_frame_overflow_count_ = 0;
};

std::uint16_t CaptureCurrentJammaPressedMask();

}  // namespace repiu::engine

#endif  // REPIU_PLATFORM_WIN32_JAMMA_INPUT_TIMELINE_H_
