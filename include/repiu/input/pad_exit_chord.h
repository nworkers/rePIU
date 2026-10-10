#ifndef REPIU_INPUT_PAD_EXIT_CHORD_H_
#define REPIU_INPUT_PAD_EXIT_CHORD_H_

#include "repiu/input/host_pad_state.h"

#include <cstdint>

namespace repiu::input
{

// Issue #52. The gamepad way to quit, for a Steam Deck that has no keyboard or
// close button: both triggers and both stick clicks held together on one pad.
// Four controls no song uses at once, held for a second, so it cannot happen
// by accident in play.
//
// True when any one pad holds all four; the four split across two pads do not
// count.
[[nodiscard]] bool IsPadExitChordDown(const HostPadState& state);

// Fires once when the chord has been held for `kHoldMilliseconds` without a
// break, and not again until it is released.
class PadExitChordTimer
{
public:
    static constexpr std::uint64_t kHoldMilliseconds = 1000;

    // `now_ms` is any monotonic millisecond clock. True on the one update at
    // which the hold reaches a second.
    bool Update(bool down, std::uint64_t now_ms);

private:
    bool down_ = false;
    bool fired_ = false;
    std::uint64_t since_ms_ = 0;
};

}  // namespace repiu::input

#endif  // REPIU_INPUT_PAD_EXIT_CHORD_H_
