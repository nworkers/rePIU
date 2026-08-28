#pragma once

#include <cstdint>
#include <string_view>

namespace repiu::engine
{

// Task 371: the guest's `grBufferSwap` interval argument has never been applied --
// the backend records it and never calls `SDL_GL_SetSwapInterval` -- so the vsync
// in effect is SDL's or the driver's default. Task 370 measured the present at a
// maximum of exactly one 60 Hz refresh period, which raised the question of
// whether the run is display-limited rather than CPU-limited. This override
// exists to answer that: it forces the interval so the two cases can be compared.
//
// Applying the guest's request automatically is deliberately not done here. That
// is a behaviour change, and it waits on what the measurement says.
constexpr std::int32_t kMinGlideSwapInterval = -1;   // adaptive vsync
constexpr std::int32_t kMaxGlideSwapInterval = 4;

// Accepts -1 through 4 exactly. Trailing spaces and non-numeric text are rejected
// rather than coerced, so a mistyped variable fails visibly instead of silently
// selecting a different measurement.
bool ResolveGlideSwapIntervalOverride(std::string_view setting,
                                      std::int32_t* interval);

bool TryReadGlideSwapIntervalOverride(std::int32_t* interval);

struct Win32GlideSwapIntervalPolicySnapshot
{
    bool override_requested = false;
    std::int32_t requested_interval = 0;
    bool applied = false;
    // Read back from the driver rather than assumed: a driver may refuse or clamp
    // the request, and a refusal that stayed invisible would invalidate the A/B.
    bool effective_valid = false;
    std::int32_t effective_interval = 0;
};

}  // namespace repiu::engine
