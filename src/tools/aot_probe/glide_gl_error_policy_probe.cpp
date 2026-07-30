#include "glide_gl_error_policy_probe.h"

#include "repiu/platform/win32/glide_gl_error_policy.h"

#include <iostream>
#include <string>

namespace repiu::tools
{

bool RunGlideGlErrorPolicyProbe()
{
    using platform::win32::RecordGlideGlDebugMessage;
    using platform::win32::RecordGlideGlErrorFrameCheck;
    using platform::win32::ResolveGlideGlErrorCheckEnabled;
    using platform::win32::SnapshotGlideGlErrorPolicy;
    using platform::win32::Win32GlideGlErrorPolicyProfile;

    // The accepted set is exact. A trailing space is the failure that actually
    // happened during Task 369 measurement -- `set VAR=1 && ...` in cmd puts the
    // space inside the value -- so it is pinned here rather than left implied.
    const bool policy =
        !ResolveGlideGlErrorCheckEnabled("") &&
        !ResolveGlideGlErrorCheckEnabled("0") &&
        !ResolveGlideGlErrorCheckEnabled("1 ") &&
        !ResolveGlideGlErrorCheckEnabled("TRUE") &&
        ResolveGlideGlErrorCheckEnabled("1") &&
        ResolveGlideGlErrorCheckEnabled("on") &&
        ResolveGlideGlErrorCheckEnabled("true");

    // Task 370: zero is a meaningful value -- it disables the frame check --
    // so it must parse rather than be rejected as empty.
    std::uint32_t interval = 0xFFFFFFFFU;
    const bool interval_policy =
        platform::win32::ResolveGlideGlErrorFrameInterval("64", &interval) &&
        interval == 64U &&
        platform::win32::ResolveGlideGlErrorFrameInterval("0", &interval) &&
        interval == 0U &&
        !platform::win32::ResolveGlideGlErrorFrameInterval("", &interval) &&
        !platform::win32::ResolveGlideGlErrorFrameInterval("64 ", &interval) &&
        !platform::win32::ResolveGlideGlErrorFrameInterval("x", &interval) &&
        !platform::win32::ResolveGlideGlErrorFrameInterval("64", nullptr);

    Win32GlideGlErrorPolicyProfile profile;
    RecordGlideGlErrorFrameCheck(&profile, 0U, 0U);
    RecordGlideGlErrorFrameCheck(&profile, 0x0501U, 1U);
    RecordGlideGlErrorFrameCheck(&profile, 0x0502U, 2U);
    RecordGlideGlErrorFrameCheck(&profile, 0U, 0U);

    const auto snapshot = SnapshotGlideGlErrorPolicy(profile, false, 64U);
    const bool accumulation =
        snapshot.frame_check_count == 4U &&
        snapshot.frame_error_count == 2U &&
        snapshot.drain_iteration_count == 3U &&
        snapshot.frame_interval == 64U;

    // The first code is retained, not overwritten: one early fault must not be
    // buried by a later, noisier one.
    const bool first_error_retained = snapshot.first_error_code == 0x0501U;

    // Task 370: non-error chatter counts but must not claim the first message,
    // and the retained message is a copy rather than the driver's pointer.
    Win32GlideGlErrorPolicyProfile debug_profile;
    RecordGlideGlDebugMessage(&debug_profile, 0x1111U, false, "chatter", 7U);
    RecordGlideGlDebugMessage(&debug_profile, 0x2222U, true, "first fault", 11U);
    RecordGlideGlDebugMessage(&debug_profile, 0x3333U, true, "second", 6U);
    const auto debug_snapshot =
        SnapshotGlideGlErrorPolicy(debug_profile, false, 0U);
    const bool debug_accounting =
        debug_snapshot.debug_message_count == 3U &&
        debug_snapshot.debug_error_count == 2U &&
        debug_snapshot.first_debug_message_id == 0x2222U &&
        std::string(debug_snapshot.first_debug_message.data()) == "first fault";

    // An oversized message is truncated, not overrun, and stays terminated.
    Win32GlideGlErrorPolicyProfile long_profile;
    const std::string oversized(
        platform::win32::kGlideGlDebugMessageCapacity + 64U, 'x');
    RecordGlideGlDebugMessage(&long_profile, 7U, true, oversized.c_str(),
                              oversized.size());
    const auto long_snapshot =
        SnapshotGlideGlErrorPolicy(long_profile, false, 0U);
    const bool truncated =
        std::string(long_snapshot.first_debug_message.data()).size() ==
            platform::win32::kGlideGlDebugMessageCapacity - 1U &&
        long_snapshot.first_debug_message.back() == '\0';

    const bool clean_run_reports_zero =
        SnapshotGlideGlErrorPolicy(Win32GlideGlErrorPolicyProfile{}, true, 0U)
                .first_error_code == 0U &&
        SnapshotGlideGlErrorPolicy(Win32GlideGlErrorPolicyProfile{}, true, 0U)
            .per_call_check_enabled;

    Win32GlideGlErrorPolicyProfile untouched;
    RecordGlideGlErrorFrameCheck(nullptr, 0x0501U, 4U);
    RecordGlideGlDebugMessage(nullptr, 1U, true, "ignored", 7U);
    const bool inert =
        SnapshotGlideGlErrorPolicy(untouched, false, 0U).frame_check_count ==
            0U &&
        SnapshotGlideGlErrorPolicy(untouched, false, 0U).debug_message_count ==
            0U &&
        !SnapshotGlideGlErrorPolicy(untouched, false, 0U).per_call_check_enabled;

    const bool all = policy && interval_policy && accumulation &&
        first_error_retained && debug_accounting && truncated &&
        clean_run_reports_zero && inert;
    std::cout << "glide_gl_error_policy_resolver="
              << (policy ? "true" : "false")
              << "\nglide_gl_error_policy_interval="
              << (interval_policy ? "true" : "false")
              << "\nglide_gl_error_policy_accumulation="
              << (accumulation ? "true" : "false")
              << "\nglide_gl_error_policy_first_error_retained="
              << (first_error_retained ? "true" : "false")
              << "\nglide_gl_error_policy_debug_accounting="
              << (debug_accounting ? "true" : "false")
              << "\nglide_gl_error_policy_message_truncated="
              << (truncated ? "true" : "false")
              << "\nglide_gl_error_policy_clean_run="
              << (clean_run_reports_zero ? "true" : "false")
              << "\nglide_gl_error_policy_inert="
              << (inert ? "true" : "false")
              << "\nglide_gl_error_policy_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
