#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace repiu::engine
{

// Task 369: `glGetError` is a pipeline synchronisation point, not a cheap state
// query. Task 364's phase instrument measured it at 99.81% of `grDepthMask`
// cost (491,356 of 492,266 cycles per call) across 24,774 gameplay calls that
// never once reported an error, which is 6.24% of wall time spent proving
// nothing. The per-call check therefore moves behind this policy and a single
// post-present check per frame takes over as the safety net.
//
// The check is also imprecise, which is the second reason to move it: the flag
// is context-wide, so a nonzero result names whichever earlier command failed,
// not the setter that happened to observe it.
bool ResolveGlideGlErrorCheckEnabled(std::string_view setting);

// Resolved once and cached: the setters are a hot path and `getenv` is not.
bool GlideGlErrorCheckPolicyEnabled();

// Task 370: Task 369's per-frame check turned out to be the only synchronisation
// point in the frame -- `SDL_GL_SwapWindow` queues the flip in 44 microseconds
// without draining -- so the single call cost 3.64 ms, 10.71% of wall. The frame
// check therefore becomes periodic, and zero when debug output takes over.
constexpr std::uint32_t kDefaultGlideGlErrorFrameInterval = 64U;

// Parses a frame count. Zero is valid and disables the frame check entirely.
bool ResolveGlideGlErrorFrameInterval(std::string_view setting,
                                      std::uint32_t* interval);

// Reports whether REPIU_GLIDE_GL_ERROR_FRAME_INTERVAL was set at all, so an
// explicit value can override the debug-output default in either direction.
bool TryReadGlideGlErrorFrameInterval(std::uint32_t* interval);

// Sized so a driver message fits without allocating. The callback runs on the
// host thread inside arbitrary GL calls, where allocation is not acceptable.
constexpr std::size_t kGlideGlDebugMessageCapacity = 192U;

struct Win32GlideGlErrorPolicyProfile
{
    std::uint32_t frame_check_count = 0;
    std::uint32_t frame_error_count = 0;
    // First code observed, kept rather than overwritten so a single early fault
    // is not buried by later ones.
    std::uint32_t first_error_code = 0;
    std::uint32_t drain_iteration_count = 0;
    // Task 370: asynchronous reporting through GL_KHR_debug.
    bool debug_output_installed = false;
    std::uint32_t debug_message_count = 0;
    std::uint32_t debug_error_count = 0;
    std::uint32_t first_debug_message_id = 0;
    std::array<char, kGlideGlDebugMessageCapacity> first_debug_message{};
};

struct Win32GlideGlErrorPolicySnapshot
{
    bool per_call_check_enabled = false;
    std::uint32_t frame_interval = 0;
    std::uint32_t frame_check_count = 0;
    std::uint32_t frame_error_count = 0;
    std::uint32_t first_error_code = 0;
    std::uint32_t drain_iteration_count = 0;
    bool debug_output_installed = false;
    std::uint32_t debug_message_count = 0;
    std::uint32_t debug_error_count = 0;
    std::uint32_t first_debug_message_id = 0;
    std::array<char, kGlideGlDebugMessageCapacity> first_debug_message{};
};

// `first_error_code` is GL_NO_ERROR (0) when the frame was clean. `drain_iterations`
// counts how many codes the frame check had to pop, which is how a burst of
// distinct errors stays visible even though only the first code is retained.
void RecordGlideGlErrorFrameCheck(Win32GlideGlErrorPolicyProfile* profile,
                                  std::uint32_t first_error_code,
                                  std::uint32_t drain_iterations);

// Called from the GL debug callback. Allocation-free and lock-free by contract:
// the driver invokes it on the host thread from inside an arbitrary GL call.
// `is_error` separates real errors from the performance and notification chatter
// drivers emit, and only an error supplies the retained first message.
void RecordGlideGlDebugMessage(Win32GlideGlErrorPolicyProfile* profile,
                               std::uint32_t id,
                               bool is_error,
                               const char* message,
                               std::size_t length);

Win32GlideGlErrorPolicySnapshot SnapshotGlideGlErrorPolicy(
    const Win32GlideGlErrorPolicyProfile& profile,
    bool per_call_check_enabled,
    std::uint32_t frame_interval);

}  // namespace repiu::engine
