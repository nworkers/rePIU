#include "repiu/input/host_pad_state.h"

namespace repiu::input
{

void HostPadState::ClearGamepad(std::uint32_t slot)
{
    if (slot < kMaxGamepadSlots)
    {
        gamepad_buttons[slot] = 0U;
        gamepad_triggers[slot] = 0U;
    }
}

void HostPadState::ClearJoystick(std::uint32_t slot)
{
    if (slot < kMaxJoystickSlots)
    {
        joystick_buttons[slot] = 0U;
        for (std::uint8_t& hat : joystick_hats[slot])
        {
            hat = 0U;
        }
    }
}

void HostPadState::ClearAll()
{
    *this = HostPadState{};
}

bool IsHostPadAliasDown(const HostPadAlias& alias, const HostPadState& state)
{
    switch (alias.kind)
    {
        case HostPadControlKind::kGamepadButton:
            return alias.slot < kMaxGamepadSlots && alias.control < 32U &&
                   (state.gamepad_buttons[alias.slot] &
                    (1U << alias.control)) != 0U;
        case HostPadControlKind::kGamepadTrigger:
            return alias.slot < kMaxGamepadSlots && alias.control < 2U &&
                   (state.gamepad_triggers[alias.slot] &
                    (1U << alias.control)) != 0U;
        case HostPadControlKind::kJoystickButton:
            return alias.slot < kMaxJoystickSlots &&
                   alias.control < kMaxJoystickButtons &&
                   (state.joystick_buttons[alias.slot] &
                    (1U << alias.control)) != 0U;
        case HostPadControlKind::kJoystickHat:
            return alias.slot < kMaxJoystickSlots &&
                   alias.control < kMaxJoystickHats &&
                   (state.joystick_hats[alias.slot][alias.control] &
                    alias.hat_direction) != 0U;
        case HostPadControlKind::kNone:
            break;
    }
    return false;
}

}  // namespace repiu::input
