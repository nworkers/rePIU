#include "repiu/input/pad_osd_chord.h"

#include <SDL3/SDL_gamepad.h>

namespace repiu::input
{

bool IsPadOsdChordDown(const HostPadState& state)
{
    constexpr std::uint32_t kY = 1U << SDL_GAMEPAD_BUTTON_NORTH;
    // Bit 0 the left trigger, bit 1 the right one (HostPadState).
    constexpr std::uint8_t kTriggers = 0x03U;
    for (std::uint32_t slot = 0; slot < kMaxGamepadSlots; ++slot)
    {
        if ((state.gamepad_buttons[slot] & kY) == kY &&
            (state.gamepad_triggers[slot] & kTriggers) == kTriggers)
        {
            return true;
        }
    }
    return false;
}

void PadGameGate::SetSuppressed(bool suppressed, std::uint16_t raw)
{
    if (suppressed_ && !suppressed)
    {
        // Whatever is held as the gate opens stays hidden until let go.
        latched_ = raw;
    }
    suppressed_ = suppressed;
}

std::uint16_t PadGameGate::Apply(std::uint16_t raw)
{
    if (suppressed_)
    {
        return 0U;
    }
    // A latched button that has been released is free again.
    latched_ = static_cast<std::uint16_t>(latched_ & raw);
    return static_cast<std::uint16_t>(raw & ~latched_);
}

}  // namespace repiu::input
