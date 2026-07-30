#pragma once

#include <cstdint>
#include <string_view>

namespace repiu::platform::win32
{

// Task 364: splits the host-thread body of the two leading state setters into
// the leading error drain, the state application, and the trailing error check.
// This is the one question in Task 364 that cannot reuse an existing timestamp
// — separating `glDepthMask` from the `glGetError` right after it requires a
// clock read between them — so it carries its own opt-in
// (`REPIU_GLIDE_SETTER_PHASE`) and its own observer verdict.
enum class Win32GlideSetterPhaseKind : std::uint8_t
{
    kDepthMask = 0,
    kAlphaBlend,
};

struct Win32GlideSetterPhaseEntry
{
    std::uint32_t call_count = 0;
    // `grDepthMask` has no leading drain, so its drain interval is length zero
    // by construction rather than by omission.
    std::uint64_t drain_cycles = 0;
    std::uint64_t apply_cycles = 0;
    std::uint64_t error_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_total_cycles = 0;
    std::uint64_t max_apply_cycles = 0;
    std::uint64_t max_error_cycles = 0;
    std::uint32_t drain_iteration_count = 0;
    std::uint32_t error_count = 0;
};

struct Win32GlideSetterPhaseProfile
{
    bool enabled = false;
    std::uint32_t clamped_sample_count = 0;
    Win32GlideSetterPhaseEntry depth_mask;
    Win32GlideSetterPhaseEntry alpha_blend;
};

struct Win32GlideSetterPhaseSnapshot
{
    bool enabled = false;
    std::uint32_t clamped_sample_count = 0;
    Win32GlideSetterPhaseEntry depth_mask;
    Win32GlideSetterPhaseEntry alpha_blend;
};

bool ResolveGlideSetterPhaseProfileEnabled(std::string_view setting);
bool GlideSetterPhaseProfileEnabled();

// The four timestamps partition the interval exactly:
// drain = entry..apply_start, apply = apply_start..error_start,
// error = error_start..finish, total = entry..finish. The identity
// `drain + apply + error == total` therefore holds by construction and is
// checked as a gate by the probe and the measurement script.
void RecordGlideSetterPhaseSample(
    Win32GlideSetterPhaseProfile* profile,
    Win32GlideSetterPhaseKind kind,
    std::uint64_t entry_cycles,
    std::uint64_t apply_start_cycles,
    std::uint64_t error_start_cycles,
    std::uint64_t finish_cycles,
    std::uint32_t drain_iterations,
    bool error_reported);

Win32GlideSetterPhaseSnapshot SnapshotGlideSetterPhaseTiming(
    const Win32GlideSetterPhaseProfile& profile);

}  // namespace repiu::platform::win32
