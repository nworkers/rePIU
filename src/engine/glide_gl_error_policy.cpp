#include "repiu/engine/glide_gl_error_policy.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>

namespace repiu::engine
{
namespace
{

bool ReadGlideGlErrorCheckSetting()
{
    const char* value = std::getenv("REPIU_GLIDE_GL_ERROR_CHECK");
    return value != nullptr && ResolveGlideGlErrorCheckEnabled(value);
}

}  // namespace

bool ResolveGlideGlErrorCheckEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideGlErrorCheckPolicyEnabled()
{
    static const bool enabled = ReadGlideGlErrorCheckSetting();
    return enabled;
}

void RecordGlideGlErrorFrameCheck(Win32GlideGlErrorPolicyProfile* profile,
                                  std::uint32_t first_error_code,
                                  std::uint32_t drain_iterations)
{
    if (profile == nullptr)
    {
        return;
    }
    ++profile->frame_check_count;
    profile->drain_iteration_count += drain_iterations;
    if (first_error_code == 0U)
    {
        return;
    }
    ++profile->frame_error_count;
    if (profile->first_error_code == 0U)
    {
        profile->first_error_code = first_error_code;
    }
}

bool ResolveGlideGlErrorFrameInterval(std::string_view setting,
                                      std::uint32_t* interval)
{
    if (interval == nullptr || setting.empty())
    {
        return false;
    }
    std::uint32_t value = 0;
    const char* begin = setting.data();
    const char* end = begin + setting.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return false;
    }
    *interval = value;
    return true;
}

bool TryReadGlideGlErrorFrameInterval(std::uint32_t* interval)
{
    const char* value = std::getenv("REPIU_GLIDE_GL_ERROR_FRAME_INTERVAL");
    return value != nullptr &&
        ResolveGlideGlErrorFrameInterval(value, interval);
}

void RecordGlideGlDebugMessage(Win32GlideGlErrorPolicyProfile* profile,
                               std::uint32_t id,
                               bool is_error,
                               const char* message,
                               std::size_t length)
{
    if (profile == nullptr)
    {
        return;
    }
    ++profile->debug_message_count;
    if (!is_error)
    {
        return;
    }
    ++profile->debug_error_count;
    if (profile->first_debug_message_id != 0U ||
        profile->first_debug_message[0] != '\0')
    {
        return;
    }
    profile->first_debug_message_id = id;
    if (message == nullptr)
    {
        return;
    }
    // Copy rather than retain the pointer: the driver owns the buffer only for
    // the duration of the callback.
    const std::size_t copied =
        std::min(length, kGlideGlDebugMessageCapacity - 1U);
    std::copy_n(message, copied, profile->first_debug_message.begin());
    profile->first_debug_message[copied] = '\0';
}

Win32GlideGlErrorPolicySnapshot SnapshotGlideGlErrorPolicy(
    const Win32GlideGlErrorPolicyProfile& profile,
    bool per_call_check_enabled,
    std::uint32_t frame_interval)
{
    Win32GlideGlErrorPolicySnapshot snapshot;
    snapshot.per_call_check_enabled = per_call_check_enabled;
    snapshot.frame_interval = frame_interval;
    snapshot.frame_check_count = profile.frame_check_count;
    snapshot.frame_error_count = profile.frame_error_count;
    snapshot.first_error_code = profile.first_error_code;
    snapshot.drain_iteration_count = profile.drain_iteration_count;
    snapshot.debug_output_installed = profile.debug_output_installed;
    snapshot.debug_message_count = profile.debug_message_count;
    snapshot.debug_error_count = profile.debug_error_count;
    snapshot.first_debug_message_id = profile.first_debug_message_id;
    snapshot.first_debug_message = profile.first_debug_message;
    return snapshot;
}

}  // namespace repiu::engine
