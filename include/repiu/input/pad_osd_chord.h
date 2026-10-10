#ifndef REPIU_INPUT_PAD_OSD_CHORD_H_
#define REPIU_INPUT_PAD_OSD_CHORD_H_

#include "repiu/input/host_pad_state.h"

#include <cstdint>

namespace repiu::input
{

// Issue #55. LT+RT+Y on one pad opens and closes the in-game OSD, for a Steam
// Deck with no Tab key. The triggers and Y are unbound by default and share
// nothing with the exit chord (LT+RT+L3+R3).
[[nodiscard]] bool IsPadOsdChordDown(const HostPadState& state);

// True once, on the update at which `down` turns true.
class PadChordEdge
{
public:
    bool Update(bool down)
    {
        const bool rising = down && !down_;
        down_ = down;
        return rising;
    }

private:
    bool down_ = false;
};

// Issue #55. What the game is allowed to see of the pad mask the bindings
// compute. While the OSD is open the pad drives the OSD, so the game sees none
// of it; when it closes, a button still held -- the B that closed it is the
// down-right panel by default -- stays hidden until it is released, so closing
// the OSD never presses a panel.
class PadGameGate
{
public:
    // `raw` is the bindings' mask for what the pads hold now.
    void SetSuppressed(bool suppressed, std::uint16_t raw);
    [[nodiscard]] bool suppressed() const
    {
        return suppressed_;
    }
    // The mask the game sees for `raw`.
    std::uint16_t Apply(std::uint16_t raw);

private:
    bool suppressed_ = false;
    std::uint16_t latched_ = 0U;
};

}  // namespace repiu::input

#endif  // REPIU_INPUT_PAD_OSD_CHORD_H_
