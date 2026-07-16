#ifndef REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_
#define REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_

#include <cstdint>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32LiveTelemetryMagic = 0x5250544CU;
constexpr std::uint32_t kWin32LiveTelemetryVersion = 15;
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
    volatile long mscdex_frame_es = 0;
    // 0 = none, 1 = buffer resolution failed, 2 = header length below 13.
    volatile long mscdex_decline_reason = 0;
    // 0 = unresolved, 1 = selector table, 2 = real-mode low memory.
    volatile long mscdex_resolve_kind = 0;
    volatile long mscdex_header = 0;
    volatile long fatal_breakpoint_count = 0;
    volatile long fatal_message_address = 0;
    // Physical/shadow selector reads that disagreed inside HLE handlers.
    volatile long seg_divergence_count = 0;
    // segment_register << 16 | physical selector of the last divergence.
    volatile long seg_divergence_reg_physical = 0;
    volatile long seg_divergence_shadow = 0;
    // Mirrored by the guest thread itself so they stay observable even if
    // the host poll loop stalls.
    volatile long aot_boundary_count = 0;
    volatile long aot_reentry_count = 0;
    // Guest address of the most recent HandleAotReentry inline-cache-miss
    // boundary (updated outside ExceptionDispatchScope, unlike last_eip).
    volatile long aot_boundary_guest_eip = 0;
    volatile long aot_legacy_fallback_count = 0;
    volatile long aot_last_fallback_address = 0;
    // Live-mirrored code-page retirement/quarantine counters (Task 217) --
    // to confirm whether a stuck aot_boundary_guest_eip reflects the same
    // page repeatedly retiring/re-resolving rather than a one-time event.
    volatile long aot_page_retire_attempt_count = 0;
    volatile long aot_page_retire_success_count = 0;
    volatile long aot_retired_entry_trap_count = 0;
    volatile long aot_quarantine_count = 0;
    // Provenance of the most recent quarantine (Task 218): which page was
    // retired and the guest write that triggered it, so the storm's cause
    // (false positive vs. DOS4GW's own thunk self-patch) can be judged live.
    volatile long aot_last_retired_page = 0;
    volatile long aot_last_code_write_source = 0;
    volatile long aot_last_code_write_destination = 0;
    // Return-transfer diagnostics (Task 219): where the most recent guest
    // RET actually goes, so a return target stuck on a quarantined page can
    // be observed while dispatch is silent.
    volatile long aot_last_return_source = 0;
    volatile long aot_last_return_target = 0;
    volatile long aot_last_expected_return = 0;
    volatile long aot_last_return_matches_call = 0;
    volatile long aot_return_dispatch_count = 0;
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
