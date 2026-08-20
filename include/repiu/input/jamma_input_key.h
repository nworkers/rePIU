#ifndef REPIU_INPUT_JAMMA_INPUT_KEY_H_
#define REPIU_INPUT_JAMMA_INPUT_KEY_H_

#include <cstdint>

namespace repiu::input
{

// The logical PIUIO/JAMMA inputs the emulator can deliver to the guest.
//
// Names describe the physical stage panel, matching the port bit tables in
// docs/analysis/piu-io-port-specification.md. The enumerator ORDER is load
// bearing: JammaInputKeyMask turns it into a bit position and the timeline in
// Win32JammaInputTimeline stores those masks, so entries may be renamed but
// never reordered.
enum class JammaInputKey : std::uint8_t
{
    kP1UpLeft,
    kP1UpRight,
    kP1DownLeft,
    kP1DownRight,
    kP1Center,
    kCoin1,
    kTest,
    kService,
    kClear,
    kP2UpLeft,
    kP2UpRight,
    kP2DownLeft,
    kP2DownRight,
    kP2Center,
    kCount,
};

constexpr std::uint16_t JammaInputKeyMask(JammaInputKey key)
{
    return static_cast<std::uint16_t>(
        1U << static_cast<std::uint8_t>(key));
}

constexpr std::uint32_t kJammaInputKeyCount =
    static_cast<std::uint32_t>(JammaInputKey::kCount);

// The configuration name of each input, used as the `[Input]` key in a ROM-set
// config file. Returns an empty string for kCount.
const char* JammaInputKeyConfigName(JammaInputKey key);

// The inverse lookup. Comparison ignores case and underscores, so
// "P1_UP_LEFT", "p1upleft", and "P1UpLeft" all resolve to the same input.
// Returns false when the name matches no input.
bool FindJammaInputKeyByConfigName(const char* name, JammaInputKey* key);

}  // namespace repiu::input

#endif  // REPIU_INPUT_JAMMA_INPUT_KEY_H_
