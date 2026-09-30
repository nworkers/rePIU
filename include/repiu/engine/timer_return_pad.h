#pragma once

#include <cstdint>

namespace repiu::engine
{

// Task 762. Where an injected timer frame returns to on the direct model, and
// what the engine has to remember to take the return.
//
// On the direct model the guest's `iret` runs natively, so the engine never
// saw a timer handler return. Two things followed. The rules that need the
// return -- no nesting inside a handler, the interrupted code's turn, the
// bounded chain after a return (Task 751) -- stayed off. And the code a
// handler returned to ran on unsupervised: in place, natively, with nothing
// to trap on. pumpit8 waits for the tick counter in such a loop after a song
// has loaded, no further tick could be injected, and the wait never ended.
//
// So the frame's return address is the return pad, an address that faults
// when executed, and the real return address waits here with the frame it
// belongs to. The fault is the handler's return, seen.
//
// Guest-thread only: injection and the fault both run there.
//
// On by default. `REPIU_TIMER_RETURN_PAD=0|off|false` writes the real return
// address into the frame as before.
[[nodiscard]] bool ResolveTimerReturnPadEnabled(const char* value);
[[nodiscard]] bool TimerReturnPadEnabled();

struct TimerReturnRecord
{
    // Where the interrupted code goes on: a guest address in place or an
    // address in the code cache.
    std::uint32_t return_eip = 0;
    // ESP of the injected frame, which is ESP at the handler's `iret`.
    std::uint32_t frame_esp = 0;
    // The engine's execution state for the interrupted code. The handler runs
    // without it and the return puts it back.
    bool single_step_trace = false;
    bool legacy_fallback = false;
    bool reentry_pending = false;
};

struct TimerReturnPad
{
    static constexpr std::uint32_t kCapacity = 8;
    // Injection order, newest last.
    TimerReturnRecord records[kCapacity] = {};
    std::uint32_t depth = 0;
    std::uint32_t depth_max = 0;
    std::uint32_t pushed_total = 0;
    std::uint32_t returned_total = 0;
    // Returns into guest code in place, which go on under the trace.
    std::uint32_t supervised_total = 0;
    // Injections made without the pad because no record could be kept.
    std::uint32_t overflow_total = 0;
    // Returns whose stack matched no record; the newest was used.
    std::uint32_t unmatched_total = 0;
    // Records dropped without a return: their handler never came back, or
    // their frame was abandoned.
    std::uint32_t abandoned_total = 0;
};

// Keeps `record` for the frame about to be written. When every slot is taken,
// the records whose frame the stack at `current_esp` stands above are dropped
// first: those frames have been popped without a return. False when no slot
// can be had, and the frame then has to carry the real return address.
bool PushTimerReturn(TimerReturnPad* pad, const TimerReturnRecord& record,
                     std::uint32_t current_esp);

// The return of the frame that ended at `esp_after_return`, which is twelve
// bytes above the frame's own ESP. The newest record of that frame is taken,
// and the records above it go with it: their handlers cannot return any more.
// Without a match the newest record is taken. False when there is no record
// at all.
bool PopTimerReturn(TimerReturnPad* pad, std::uint32_t esp_after_return,
                    TimerReturnRecord* record);

}  // namespace repiu::engine
