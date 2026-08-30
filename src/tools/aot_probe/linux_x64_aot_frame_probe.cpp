#include "linux_x64_aot_frame_probe.h"

#include "repiu/platform/linux_x64_aot_frame.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

constexpr std::uint32_t kGuestSourceMarker = 0x54700001U;
constexpr std::uint32_t kGuestContinuationMarker = 0x54700002U;
constexpr std::uint32_t kStatusMarker = 0x54700003U;

struct ProbeState
{
    void* expected_context = nullptr;
    bool context_matched = false;
    bool call_stack_aligned = false;
    bool frame_edited = false;
};

ProbeState g_probe;

}  // namespace

}  // namespace repiu::tools

extern "C" void RepiuLinuxX64AotFrameProbeResolver(
    void* context,
    repiu::platform::LinuxX64AotDispatchFrame* frame,
    std::uintptr_t call_stack_pointer)
{
    auto& probe = repiu::tools::g_probe;
    probe.context_matched = context == probe.expected_context;
    probe.call_stack_aligned = (call_stack_pointer & 0x0FU) == 0U;
    if (frame == nullptr)
    {
        return;
    }
    frame->guest_source = repiu::tools::kGuestSourceMarker;
    frame->guest_continuation = repiu::tools::kGuestContinuationMarker;
    frame->status = repiu::tools::kStatusMarker;
    probe.frame_edited = true;
}

extern "C" std::uint32_t RepiuLinuxX64AotFrameAbiProbe(
    void* context,
    repiu::platform::LinuxX64AotDispatchFrame* frame);

namespace repiu::tools
{

bool RunLinuxX64AotFrameProbe()
{
    int context_marker = 0;
    g_probe = ProbeState{};
    g_probe.expected_context = &context_marker;

    platform::LinuxX64AotDispatchFrame frame;
    frame.context = reinterpret_cast<std::uintptr_t>(&context_marker);
    frame.guest.eip = 0x01020304U;
    frame.guest.esp = 0x05060708U;

    const std::uint32_t result = RepiuLinuxX64AotFrameAbiProbe(
        &context_marker, &frame);
    const bool markers_match = frame.guest_source == kGuestSourceMarker &&
        frame.guest_continuation == kGuestContinuationMarker &&
        frame.status == kStatusMarker;
    const bool guest_widths_preserved = frame.guest.eip == 0x01020304U &&
        frame.guest.esp == 0x05060708U;
    const bool all = result == 1U && g_probe.context_matched &&
        g_probe.call_stack_aligned && g_probe.frame_edited && markers_match &&
        guest_widths_preserved &&
        frame.context == reinterpret_cast<std::uintptr_t>(&context_marker);

    std::cout << "linux_x64_aot_frame_context="
              << (g_probe.context_matched ? "true" : "false")
              << "\nlinux_x64_aot_frame_stack_aligned="
              << (g_probe.call_stack_aligned ? "true" : "false")
              << "\nlinux_x64_aot_frame_edited="
              << (g_probe.frame_edited ? "true" : "false")
              << "\nlinux_x64_aot_frame_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
