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
//
// Task 766: an unset variable no longer leaves the driver's default in place.
// That default differs by host -- SDL's Wayland backend keeps the interval at 0
// unless asked -- so a run requests kDefaultGlideSwapInterval everywhere, and
// the variable (or the launcher setting published into it) turns vsync off.
constexpr std::int32_t kMinGlideSwapInterval = -1;   // adaptive vsync
constexpr std::int32_t kMaxGlideSwapInterval = 4;
constexpr std::int32_t kDefaultGlideSwapInterval = 1;

// Accepts -1 through 4 exactly. Trailing spaces and non-numeric text are rejected
// rather than coerced, so a mistyped variable fails visibly instead of silently
// selecting a different measurement.
bool ResolveGlideSwapIntervalOverride(std::string_view setting,
                                      std::int32_t* interval);

// Task 766. Always fills `interval`: the variable's value when it is a valid
// override, kDefaultGlideSwapInterval when it is absent (`value` null) or
// malformed. Returns whether the value was an override.
bool ResolveGlideSwapInterval(const char* value, std::int32_t* interval);

// ResolveGlideSwapInterval applied to REPIU_GLIDE_SWAP_INTERVAL.
bool ReadGlideSwapInterval(std::int32_t* interval);

// Task 745. The period, in microseconds, at which the backend paces its swaps
// when the driver refuses the requested interval: `interval` frames of the
// display's refresh rate (60 Hz when the rate is unknown), with the adaptive
// request -1 counted as 1. Zero when no pacing is wanted (interval 0).
std::uint32_t ResolveGlideSwapPacingPeriodMicroseconds(
    std::int32_t interval, double refresh_rate_hz);

struct GlideSwapIntervalPolicySnapshot
{
    // Task 766: an interval is requested whenever a GL window exists; false
    // only when the backend never created one (dummy mode).
    bool interval_requested = false;
    bool override_requested = false;
    std::int32_t requested_interval = 0;
    bool applied = false;
    // Read back from the driver rather than assumed: a driver may refuse or clamp
    // the request, and a refusal that stayed invisible would invalidate the A/B.
    bool effective_valid = false;
    std::int32_t effective_interval = 0;
    // Task 745. Why the driver refused (SDL's error text), the display's
    // refresh rate, and the engine's own pacing that stands in for a refused
    // vsync: whether it is on, its period, and how many swaps it paced for
    // how long in total.
    char failure[96] = {};
    // Task 752. What draws: the GL renderer the context reports, and whether
    // the engine chose Mesa's D3D12 driver for it (WSL offers the GPU only
    // through that driver, and Mesa does not pick it by itself).
    char gl_renderer[96] = {};
    bool wsl_d3d12_selected = false;
    double refresh_rate_hz = 0.0;
    bool pacing_active = false;
    std::uint32_t pacing_period_us = 0;
    std::uint64_t paced_swaps = 0;
    std::uint64_t paced_sleep_us = 0;
    // Task 748. Where the paced frames' jitter comes from: swaps that reached
    // the pacer already past their deadline (the frame itself was long),
    // deadlines resynchronised after a stall, and how late `sleep_until`
    // woke (WSL2's timer slack can be milliseconds).
    std::uint64_t late_swaps = 0;
    std::uint64_t late_max_us = 0;
    std::uint64_t resyncs = 0;
    std::uint64_t oversleep_over_1ms = 0;
    std::uint64_t oversleep_max_us = 0;
    // Task 750: swaps the guest waited out with the timer ISR running, and
    // the ticks injected inside those waits.
    std::uint64_t wait_tick_swaps = 0;
    std::uint64_t wait_tick_injections = 0;
    // Task 754: how far SDL's clock ran from the steady clock, a second at a
    // time, and which of the two the timer ticks followed.
    bool event_clock_steady = true;
    std::uint64_t clock_windows = 0;
    std::uint64_t clock_windows_over_limit = 0;
    std::int64_t clock_total_ppm = 0;
    std::int64_t clock_worst_window_ppm = 0;
};

}  // namespace repiu::engine
