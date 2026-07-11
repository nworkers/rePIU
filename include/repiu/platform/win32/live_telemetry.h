#ifndef REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_
#define REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_

#include <cstdint>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32LiveTelemetryMagic = 0x5250544CU;
constexpr std::uint32_t kWin32LiveTelemetryVersion = 2;
constexpr const char* kWin32LiveTelemetryEnvironment =
    "REPIU_LIVE_TELEMETRY_MAPPING";
constexpr const char* kWin32ExecutionTimeoutEnvironment =
    "REPIU_EXECUTION_TIMEOUT_MS";

struct Win32SharedLiveTelemetry
{
    std::uint32_t magic = kWin32LiveTelemetryMagic;
    std::uint32_t version = kWin32LiveTelemetryVersion;
    volatile long host_phase = 0;
    volatile long heartbeat = 0;
    volatile long dispatch_entry_count = 0;
    volatile long dispatch_exit_count = 0;
    volatile long last_eip = 0;
    volatile long last_guest_eip = 0;
    volatile long last_exception_code = 0;
    volatile long last_guest_eax = 0;
    volatile long last_guest_esp = 0;
    volatile long guest_handler_phase = 0;
};

static_assert(sizeof(long) == 4);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_
