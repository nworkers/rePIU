#ifndef REPIU_INPUT_HOST_PAD_STATE_H_
#define REPIU_INPUT_HOST_PAD_STATE_H_

#include "repiu/input/host_pad_binding.h"

#include <cstdint>

namespace repiu::input
{

// Issue #34. What every connected pad is holding, by device number. Plain data
// written by the SDL adapter on the host thread and read when the pressed mask
// is recomputed, so the mapping from controls to inputs can be tested with no
// device attached.
struct HostPadState
{
    // Bit n is SDL_GamepadButton n.
    std::uint32_t gamepad_buttons[kMaxGamepadSlots] = {};
    // Bit 0 the left trigger, bit 1 the right one, each pulled past half.
    std::uint8_t gamepad_triggers[kMaxGamepadSlots] = {};
    // Bit n is joystick button n (zero-based).
    std::uint32_t joystick_buttons[kMaxJoystickSlots] = {};
    // SDL_HAT_* bits; a diagonal holds two.
    std::uint8_t joystick_hats[kMaxJoystickSlots][kMaxJoystickHats] = {};

    void ClearGamepad(std::uint32_t slot);
    void ClearJoystick(std::uint32_t slot);
    void ClearAll();
};

bool IsHostPadAliasDown(const HostPadAlias& alias, const HostPadState& state);

// Device numbers in connection order. A device takes the lowest free number
// and keeps it until it is removed, so unplugging Pad1 does not turn Pad2 into
// Pad1 halfway through a song. `id` is the SDL instance id, never zero.
template <std::uint32_t Capacity>
class HostPadSlotTable
{
public:
    // Returns the slot, or -1 when every slot is taken or the id is zero.
    int Assign(std::uint32_t id)
    {
        const int existing = Find(id);
        if (existing >= 0 || id == 0U)
        {
            return existing;
        }
        for (std::uint32_t slot = 0; slot < Capacity; ++slot)
        {
            if (ids_[slot] == 0U)
            {
                ids_[slot] = id;
                return static_cast<int>(slot);
            }
        }
        return -1;
    }

    // Returns the slot the id held, or -1 when it held none.
    int Release(std::uint32_t id)
    {
        const int slot = Find(id);
        if (slot >= 0)
        {
            ids_[slot] = 0U;
        }
        return slot;
    }

    int Find(std::uint32_t id) const
    {
        if (id == 0U)
        {
            return -1;
        }
        for (std::uint32_t slot = 0; slot < Capacity; ++slot)
        {
            if (ids_[slot] == id)
            {
                return static_cast<int>(slot);
            }
        }
        return -1;
    }

    std::uint32_t IdAt(std::uint32_t slot) const
    {
        return slot < Capacity ? ids_[slot] : 0U;
    }

private:
    std::uint32_t ids_[Capacity] = {};
};

}  // namespace repiu::input

#endif  // REPIU_INPUT_HOST_PAD_STATE_H_
