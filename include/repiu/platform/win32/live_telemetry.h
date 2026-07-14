#ifndef REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_
#define REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_

#include <cstdint>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32LiveTelemetryMagic = 0x5250544CU;
constexpr std::uint32_t kWin32LiveTelemetryVersion = 9;
constexpr std::uint32_t kWin32NativeSampleRingCapacity = 8;
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
    volatile long last_guest_ebx = 0;
    volatile long last_guest_ecx = 0;
    volatile long last_guest_edx = 0;
    volatile long last_guest_esi = 0;
    volatile long last_guest_edi = 0;
    volatile long last_guest_esp = 0;
    volatile long recovery_host_fs = 0;
    volatile long recovery_host_ds = 0;
    volatile long recovery_host_es = 0;
    volatile long recovery_host_gs = 0;
    volatile long dpmi_frame_eax = 0;
    volatile long dpmi_frame_ebx = 0;
    volatile long dpmi_frame_ecx = 0;
    volatile long guest_handler_phase = 0;
    volatile long glide_gate_ordinal = 0;
    volatile long glide_gate_esp = 0;
    volatile long glide_gate_ebx = 0;
    volatile long glide_gate_ecx = 0;
    volatile long glide_gate_edx = 0;
    volatile long glide_gate_stack[8] = {};
    volatile long mscdex_probe_count = 0;
    volatile long mscdex_request_count = 0;
    volatile long mscdex_last_command = 0;
    volatile long mscdex_last_status = 0;
    // Mirrored by the guest thread itself so they stay observable even if
    // the host poll loop stalls.
    volatile long aot_boundary_count = 0;
    volatile long aot_reentry_count = 0;
    // Published once by the loader so the supervisor can sample child
    // threads externally when the in-process poll loop stalls.
    volatile long guest_thread_id = 0;
    volatile long host_main_thread_id = 0;
    volatile long aot_cache_base = 0;
    volatile long aot_cache_size = 0;
    volatile long native_sample_count = 0;
    volatile long native_sample_unmapped_count = 0;
    // 0 = idle, 1 = suspending, 2 = suspended, 3 = context read,
    // 4 = resumed. A stuck nonzero value locates a frozen capture step.
    volatile long native_sample_stage = 0;
    volatile long native_sample_eip = 0;
    volatile long native_sample_guest_eip = 0;
    volatile long native_sample_eax = 0;
    volatile long native_sample_ebx = 0;
    volatile long native_sample_ecx = 0;
    volatile long native_sample_edx = 0;
    volatile long native_sample_esi = 0;
    volatile long native_sample_edi = 0;
    volatile long native_sample_esp = 0;
    volatile long native_sample_ebp = 0;
    volatile long native_sample_eflags = 0;
    volatile long native_sample_indirect_source = 0;
    volatile long native_sample_indirect_target = 0;
    volatile long native_sample_ring[kWin32NativeSampleRingCapacity] = {};
    volatile long native_sample_ring_mapped_bits = 0;
    volatile long native_sample_ring_cursor = 0;
};

static_assert(sizeof(long) == 4);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_
