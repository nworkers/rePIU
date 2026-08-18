#ifndef REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_
#define REPIU_PLATFORM_WIN32_LIVE_TELEMETRY_H_

#include "repiu/platform/win32/aot_boundary_provenance.h"

#include <cstdint>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32LiveTelemetryMagic = 0x5250544CU;
constexpr std::uint32_t kWin32LiveTelemetryVersion = 23;
constexpr std::uint32_t kWin32NativeSampleRingCapacity = 8;
constexpr const char* kWin32LiveTelemetryEnvironment =
    "REPIU_LIVE_TELEMETRY_MAPPING";
constexpr const char* kWin32ExecutionTimeoutEnvironment =
    "REPIU_EXECUTION_TIMEOUT_MS";
constexpr const char* kWin32StallTimeoutEnvironment =
    "REPIU_STALL_TIMEOUT_MS";

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
    // Task 274 same-binary A/B semantic milestones. Each value is published
    // once, allowing the supervisor's one-second snapshots to timestamp real
    // game progress without adding steady-state logging to draw/swap paths.
    volatile long glide_window_gate_milestone = 0;
    volatile long glide_window_open_milestone = 0;
    volatile long glide_texture_milestone = 0;
    volatile long glide_draw_milestone = 0;
    volatile long glide_swap_milestone = 0;
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
    // Low byte is the last IOCTL control block code; bit 8 is set when it was
    // handled. Identifies which position call the guest actually depends on.
    volatile long mscdex_last_ioctl_subfunction = -1;
    // One bit per rejected IOCTL control block code (codes 0..31).
    volatile long mscdex_ioctl_reject_mask = 0;
    // Transfer count the caller declared, which can legitimately be short.
    volatile long mscdex_last_ioctl_length = -1;
    // Last 84h play request as the guest phrased it: 0 = HSG, 1 = Red Book.
    volatile long mscdex_last_play_mode = -1;
    volatile long mscdex_last_play_start = 0;
    volatile long mscdex_last_play_length = 0;
    volatile long mscdex_last_seek_target = 0;
    // Position as of the most recent IOCTL, i.e. the value the guest was
    // actually handed rather than a free-running sample.
    volatile long cd_audio_reported_lba = 0;
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
    // Per-reason breakdown of aot_boundary_count (Task 262): which kind of
    // boundary guest instruction forced each single-step exit, so the ~1,400/s
    // inline-cache churn can be attributed live even if the loader's graceful
    // exit summary is suppressed by the timeout-teardown segfault (Task 235).
    volatile long aot_boundary_return_count = 0;
    volatile long aot_boundary_indirect_count = 0;
    volatile long aot_boundary_direct_count = 0;
    volatile long aot_boundary_conditional_count = 0;
    volatile long aot_boundary_other_count = 0;
    // Task 289 Stage 3a: structural cache-INT3 origin. Unlike the guest-opcode
    // reason counters, these distinguish HLE from retired/probe/fallback bytes.
    volatile long aot_breakpoint_provenance_counts[
        kAotCacheBreakpointProvenanceCount] = {};
    // Task 263(a): dominant `other` boundary opcode (running max over the
    // histogram) and the most recent `other` boundary EIP + first four bytes.
    volatile long aot_other_top_opcode = 0;
    volatile long aot_other_top_opcode_count = 0;
    volatile long aot_last_other_eip = 0;
    volatile long aot_last_other_bytes = 0;
    // Task 263(b): AOT residency proxy accumulators (straight-line guest
    // instructions per real cache entry).
    volatile long aot_residency_total = 0;
    volatile long aot_residency_samples = 0;
    volatile long aot_residency_max = 0;
    // Task 264 prerequisite probe: at a push-segment boundary, the host segment
    // register vs the shadow selector, to settle whether the guest executes with
    // its own selectors loaded (native push would be correct) or host-flat
    // (native push would push the host selector). Instrumentation only.
    volatile long aot_pushseg_count = 0;
    volatile long aot_pushseg_last_opcode = 0;
    volatile long aot_pushseg_last_host_sel = 0;
    volatile long aot_pushseg_last_shadow_sel = 0;
    volatile long aot_pushseg_match_count = 0;
    volatile long aot_pushseg_mismatch_count = 0;
    // Task 264 Phase 3 characterization: at a segment-override memory boundary,
    // the overridden segment's shadow selector and its descriptor base, so we
    // can judge whether the override is flat (base 0, translatable by stripping
    // the prefix) or needs a runtime base add. Instrumentation only.
    volatile long aot_segovr_count = 0;
    volatile long aot_segovr_last_prefix = 0;
    volatile long aot_segovr_last_selector = 0;
    volatile long aot_segovr_last_base = 0;
    volatile long aot_segovr_flat_count = 0;
    volatile long aot_segovr_nonflat_count = 0;
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
    volatile long aot_retired_span_attempt_count = 0;
    volatile long aot_retired_span_success_count = 0;
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
    // INT 21h AH=4Ah resize diagnostics (Task 221): the full 32-bit EBX of
    // the most recent request (the handler consumes only the low word) and
    // the selector/base it actually resolved, so an inert allocator ceiling
    // (base 0) is visible live.
    volatile long dos_resize_count = 0;
    volatile long dos_resize_reject_count = 0;
    volatile long dos_resize_last_ebx = 0;
    volatile long dos_resize_last_selector = 0;
    volatile long dos_resize_last_base = 0;
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
