#include "repiu/input/pad_exit_chord.h"

#include <SDL3/SDL_gamepad.h>

namespace repiu::input
{

bool IsPadExitChordDown(const HostPadState& state)
{
    constexpr std::uint32_t kSticks =
        (1U << SDL_GAMEPAD_BUTTON_LEFT_STICK) |
        (1U << SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    // Bit 0 the left trigger, bit 1 the right one (HostPadState).
    constexpr std::uint8_t kTriggers = 0x03U;
    for (std::uint32_t slot = 0; slot < kMaxGamepadSlots; ++slot)
    {
        if ((state.gamepad_buttons[slot] & kSticks) == kSticks &&
            (state.gamepad_triggers[slot] & kTriggers) == kTriggers)
        {
            return true;
        }
    }
    return false;
}

bool PadExitChordTimer::Update(bool down, std::uint64_t now_ms)
{
    if (!down)
    {
        down_ = false;
        fired_ = false;
        return false;
    }
    if (!down_)
    {
        down_ = true;
        since_ms_ = now_ms;
    }
    if (fired_ || now_ms - since_ms_ < kHoldMilliseconds)
    {
        return false;
    }
    fired_ = true;
    return true;
}

}  // namespace repiu::input
