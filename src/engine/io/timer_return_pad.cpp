#include "repiu/engine/timer_return_pad.h"

#include "repiu/runtime/env_toggle.h"

#include <cstdlib>

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kInterruptFrameBytes = 12U;

void RemoveRecord(TimerReturnPad* const pad, const std::uint32_t index)
{
    for (std::uint32_t next = index + 1U; next < pad->depth; ++next)
    {
        pad->records[next - 1U] = pad->records[next];
    }
    --pad->depth;
}

}  // namespace

bool ResolveTimerReturnPadEnabled(const char* const value)
{
    return runtime::ResolvePromotedToggle(value);
}

bool TimerReturnPadEnabled()
{
    static const bool enabled = ResolveTimerReturnPadEnabled(
        std::getenv("REPIU_TIMER_RETURN_PAD"));
    return enabled;
}

bool PushTimerReturn(TimerReturnPad* const pad,
                     const TimerReturnRecord& record,
                     const std::uint32_t current_esp)
{
    if (pad == nullptr)
    {
        return false;
    }
    if (pad->depth >= TimerReturnPad::kCapacity)
    {
        // A handler that is still running has the stack at or below its
        // frame. Only looked at when the slots run out, so a handler that
        // moved to a stack of its own is not mistaken for one that is gone.
        std::uint32_t index = 0U;
        while (index < pad->depth)
        {
            if (current_esp > pad->records[index].frame_esp)
            {
                RemoveRecord(pad, index);
                ++pad->abandoned_total;
            }
            else
            {
                ++index;
            }
        }
    }
    if (pad->depth >= TimerReturnPad::kCapacity)
    {
        ++pad->overflow_total;
        return false;
    }
    pad->records[pad->depth] = record;
    ++pad->depth;
    ++pad->pushed_total;
    if (pad->depth > pad->depth_max)
    {
        pad->depth_max = pad->depth;
    }
    return true;
}

bool PopTimerReturn(TimerReturnPad* const pad,
                    const std::uint32_t esp_after_return,
                    TimerReturnRecord* const record)
{
    if (pad == nullptr || record == nullptr || pad->depth == 0U)
    {
        return false;
    }
    std::uint32_t found = pad->depth;
    for (std::uint32_t index = pad->depth; index != 0U; --index)
    {
        if (pad->records[index - 1U].frame_esp + kInterruptFrameBytes ==
            esp_after_return)
        {
            found = index - 1U;
            break;
        }
    }
    if (found == pad->depth)
    {
        ++pad->unmatched_total;
        found = pad->depth - 1U;
    }
    *record = pad->records[found];
    pad->abandoned_total += pad->depth - 1U - found;
    pad->depth = found;
    ++pad->returned_total;
    return true;
}

}  // namespace repiu::engine
