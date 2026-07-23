#pragma once

#include "repiu/platform/win32/execution_trampoline.h"
#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/platform/win32/live_telemetry.h"
#include "repiu/platform/win32/cd_audio_wave_out.h"
#include "repiu/platform/win32/glide_opengl_backend.h"
#include "repiu/hle/linexe_call_gate.h"
#include "repiu/hle/glide_hle.h"
#include "repiu/hle/glide_lfb.h"
#include "repiu/media/chd_cd_image.h"
#include "repiu/runtime/dos_low_memory.h"
#include "repiu/runtime/selector_table.h"
#include "native_fast_path.h"

#include <cstdint>
#include <atomic>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repiu::platform::win32
{

struct ThreadContext;

extern "C" ThreadContext* g_repiu_active_thread_context;
extern "C" std::uint32_t g_repiu_dbt_host_esp;
extern "C" std::uint32_t g_repiu_dbt_host_stack_base;
extern "C" std::uint32_t g_repiu_dbt_host_stack_limit;
extern "C" std::uint32_t g_repiu_dbt_guest_stack_base;
extern "C" std::uint32_t g_repiu_dbt_guest_stack_limit;

struct StackSwitchCallState
{
    std::uint32_t entry_address = 0;
    std::uint32_t initial_esp = 0;
    std::uint32_t host_esp = 0;
    std::uint32_t guest_return_esp = 0;
    std::uint32_t result_code = 0;
    std::uint32_t enable_single_step_trace = 0;
    std::uint32_t host_fs = 0;
    std::uint32_t host_ds = 0;
    std::uint32_t host_es = 0;
    std::uint32_t host_gs = 0;
    std::uint32_t host_ss = 0;
    std::uint32_t guest_stack_base = 0;
    std::uint32_t guest_stack_limit = 0;
    std::uint32_t host_stack_base = 0;
    std::uint32_t host_stack_limit = 0;
};

struct DosInterruptVectorShadow
{
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    bool valid = false;
};

struct DpmiInterruptVectorShadow
{
    std::uint16_t selector = 0;
    std::uint32_t offset = 0;
    bool valid = false;
};

struct ShadowWriteProvenance
{
    std::uint32_t sequence = 0;
    std::uint32_t eip = 0;
    std::uint32_t opcode = 0;
    std::uint32_t destination = 0;
    std::uint32_t value = 0;
    std::uint32_t width = 0;
};

constexpr std::uint32_t kShadowWriteProvenanceCapacity = 256;

enum class AotWorkerOperation : std::uint32_t
{
    kTranslate = 0,
    kPatchInlineCache = 1,
    kRetireGuestPage = 2,
};

// DOS/32A is used only as an independent behavioral reference for this
// DOS/4G-compatible identification call; no DOS/32A source code is included.
// Reference: https://github.com/amindlost/dos32a/blob/master/src/dos32a/text/client/int21h.asm
constexpr std::uint16_t kDos4gIdentificationAx = 0xFF00U;
constexpr std::uint16_t kDos4gIdentificationDx = 0x0078U;
constexpr std::uint32_t kDos32aIdentificationSignature = 0xFFFF3447U;

// Static replay of the original DOS4GW resident handler establishes a
// different implementation contract: low AX=FFFFh, carry set, and the
// already-installed client-data GS preserved. Keep this disabled until the
// corresponding GS:0042h private environment is available atomically.
constexpr std::uint16_t kDos4gwIdentificationAxResult = 0xFFFFU;
constexpr bool kDos4gwIdentificationCarry = true;

// Original DOS4GW/LINEXE private-environment topology recovered by static
// replay. These are evidence constants, not active descriptors: the 0080h
// exports are 16-bit code and require HLE call gates before exposure.
constexpr std::uint16_t kDos4gwClientDataSelector = 0x0020U;
constexpr std::uint16_t kDos4gwPrivateRootOffset = 0x0042U;
constexpr std::uint16_t kDos4gwLinexeDataSelector = 0x0090U;
constexpr std::uint16_t kDos4gwLinexeLoaderOffset = 0x059AU;
constexpr std::uint16_t kDos4gwLinexeCodeSelector = 0x0080U;
constexpr std::uint16_t kGlideFirstGateOffset = 0x9200U;
constexpr std::uint32_t kGlideGateStride = 8U;

struct ThreadContext
{
    struct AotCallFrame
    {
        std::uint32_t source = 0;
        std::uint32_t target = 0;
        std::uint32_t fallthrough = 0;
    };
    static constexpr std::uint32_t kAotCallFrameCapacity = 1024;
    std::uint32_t entry_address = 0;
    std::uint32_t runtime_base = 0;
    std::uint32_t runtime_size = 0;
    std::uint32_t guest_initial_esp = 0;
    std::uint32_t host_esp = 0;
    std::uint32_t guest_return_esp = 0;
    std::uint32_t guest_thread_id = 0;
    StackSwitchCallState* active_call_state = nullptr;
    bool use_guest_stack = false;
    bool enable_privileged_trap_hle = false;
    bool enable_traced_dos_hle = false;
    bool enable_segment_load_hle = false;
    bool enable_dos_hle = false;
    bool enable_single_step_trace = false;
    Win32AotCodeCachePlacement* aot_placement = nullptr;
    bool aot_reentry_pending = false;
    bool aot_legacy_fallback = false;
    runtime::ExecutionBackend execution_backend =
        runtime::ExecutionBackend::kLegacy;
    HANDLE aot_translation_thread = nullptr;
    HANDLE aot_translation_request_event = nullptr;
    HANDLE aot_translation_complete_event = nullptr;
    std::atomic<bool> aot_translation_shutdown{false};
    std::atomic<std::uint32_t> aot_translation_target{0};
    std::atomic<std::uint32_t> aot_worker_operation{
        static_cast<std::uint32_t>(AotWorkerOperation::kTranslate)};
    std::atomic<std::uint32_t> aot_dbt_return_attempt_count{0};
    std::atomic<std::uint32_t> aot_dbt_return_success_count{0};
    std::atomic<std::uint32_t> aot_dbt_return_fallback_count{0};
    std::atomic<std::uint32_t> aot_dbt_return_fallback_reason_counts[
        kAotDbtReturnFallbackReasonCount] = {};
    std::atomic<std::uint32_t> aot_patch_cache_miss_address{0};
    std::atomic<std::uint32_t> aot_patch_guest_target{0};
    std::atomic<std::uint32_t> aot_patch_cache_target{0};
    std::atomic<std::uint32_t> aot_retire_guest_page{0};
    std::atomic<bool> aot_retire_quarantine{false};
    Win32AotDynamicAppendResult aot_translation_result;
    std::vector<runtime::AotExcludedGuestRange> aot_excluded_guest_ranges;
    Win32AotInlineCachePatchResult aot_inline_cache_patch_result;
    Win32AotGuestPageRetireResult aot_guest_page_retire_result;
    Win32AotPageWriteWatchSet aot_page_write_watch;
    std::atomic<bool> aot_terminal_failure{false};
    std::uint32_t aot_reentry_cache_address = 0;
    std::atomic<std::uint32_t> aot_cache_entry_count{0};
    std::atomic<std::uint32_t> aot_boundary_count{0};
    // Per-reason breakdown of aot_boundary_count (Task 262): the boundary guest
    // instruction that forced each single-step exit. The five sum to
    // aot_boundary_count.
    std::atomic<std::uint32_t> aot_boundary_return_count{0};
    std::atomic<std::uint32_t> aot_boundary_indirect_count{0};
    std::atomic<std::uint32_t> aot_boundary_direct_count{0};
    std::atomic<std::uint32_t> aot_boundary_conditional_count{0};
    std::atomic<std::uint32_t> aot_boundary_other_count{0};
    // Task 263(a): characterize the dominant `other` boundary bucket. Lead-opcode
    // histogram of the boundary guest instruction (guest-thread only) plus the
    // most recent `other` boundary EIP and its first four bytes.
    std::uint32_t aot_other_opcode_histogram[256] = {};
    std::atomic<std::uint32_t> aot_last_other_boundary_eip{0};
    std::atomic<std::uint32_t> aot_last_other_boundary_bytes{0};
    // Task 263(b): AOT residency proxy. Straight-line guest instruction count from
    // each real cache entry to its first control transfer, accumulated. Coverage
    // estimate = total / (total + single_step_trace_count).
    std::atomic<std::uint32_t> aot_residency_instruction_total{0};
    std::atomic<std::uint32_t> aot_residency_sample_count{0};
    std::atomic<std::uint32_t> aot_residency_max{0};
    // Task 264 Phase 3a: the segment selectors last folded into segment-override
    // sites, so re-resolution runs only when a segment register actually changes
    // (0xFFFF forces the first resolution). Indexed by segment register.
    std::uint16_t aot_resolved_segment_selectors[6] = {
        0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU};
    std::atomic<std::uint32_t> aot_reentry_count{0};
    std::atomic<std::uint32_t> aot_legacy_fallback_count{0};
    std::atomic<std::uint32_t> aot_last_fallback_address{0};
    std::atomic<std::uint32_t> aot_dynamic_attempt_count{0};
    std::atomic<std::uint32_t> aot_dynamic_success_count{0};
    std::atomic<std::uint32_t> aot_dynamic_added_bytes{0};
    std::atomic<std::uint32_t> aot_dbt_hle_reentry_attempt_count{0};
    std::atomic<std::uint32_t> aot_dbt_hle_reentry_success_count{0};
    std::atomic<std::uint32_t> aot_indirect_dispatch_count{0};
    std::atomic<std::uint32_t> aot_inline_cache_patch_attempt_count{0};
    std::atomic<std::uint32_t> aot_inline_cache_patch_success_count{0};
    std::atomic<std::uint32_t> aot_code_write_count{0};
    std::atomic<std::uint32_t> aot_page_retire_attempt_count{0};
    std::atomic<std::uint32_t> aot_page_retire_success_count{0};
    std::atomic<std::uint32_t> aot_generation_publish_count{0};
    std::atomic<std::uint32_t> aot_generation_failure_count{0};
    std::atomic<std::uint32_t> aot_generation_relinked_entry_count{0};
    std::atomic<std::uint32_t> aot_retired_entry_trap_count{0};
    std::atomic<std::uint32_t> aot_quarantine_count{0};
    std::atomic<std::uint32_t> aot_inline_cache_guard_reset_count{0};
    std::atomic<std::uint32_t> aot_last_code_write_source{0};
    std::atomic<std::uint32_t> aot_last_code_write_destination{0};
    std::atomic<std::uint32_t> aot_last_retired_page{0};
    std::atomic<std::uint32_t> aot_last_published_generation{0};
    bool aot_exception_mapping_valid = false;
    std::uint32_t aot_exception_cache_address = 0;
    std::uint32_t aot_exception_guest_address = 0;
    std::uint8_t aot_exception_cache_bytes[16] = {};
    std::uint8_t aot_exception_guest_bytes[16] = {};
    std::atomic<std::uint32_t> aot_last_indirect_source{0};
    std::atomic<std::uint32_t> aot_last_indirect_target{0};
    std::atomic<std::uint32_t> aot_return_dispatch_count{0};
    std::atomic<std::uint32_t> aot_last_return_target{0};
    std::atomic<std::uint32_t> aot_last_return_source{0};
    std::uint32_t aot_last_return_stack[4] = {};
    bool execution_probe_configured = false;
    bool execution_probe_hit = false;
    std::uint32_t execution_probe_offset = 0;
    X86ExecutionSnapshot execution_probe_snapshot;
    std::uint32_t execution_probe_stack[8] = {};
    bool execution_trace_configured = false;
    std::uint32_t execution_trace_start_offset = 0;
    std::uint32_t execution_trace_end_offset = 0;
    std::uint32_t execution_trace_esp_offset = 0;
    std::uint32_t execution_trace_hit_count = 0;
    bool execution_trace_sentinel2_configured = false;
    std::uint32_t execution_trace_sentinel2_offset = 0;
    std::uint32_t execution_trace_sentinel_rearm_count = 0;
    Win32ExecutionTraceEntry
        execution_trace[kWin32ExecutionTraceCapacity] = {};
    std::uint32_t aot_call_depth = 0;
    AotCallFrame aot_call_frames[kAotCallFrameCapacity] = {};
    bool aot_last_return_matches_call = false;
    std::uint32_t aot_last_expected_return = 0;
    std::uint32_t aot_last_call_source = 0;
    std::uint32_t aot_last_call_target = 0;
    std::uint32_t aot_last_expected_call_source = 0;
    std::uint32_t aot_last_expected_call_target = 0;
    std::uint32_t aot_return_trace_count = 0;
    Win32AotReturnTraceEntry
        aot_return_trace[kWin32AotReturnTraceCapacity] = {};
    std::uint32_t aot_transfer_trace_count = 0;
    Win32AotTransferTraceEntry
        aot_transfer_trace[kWin32AotTransferTraceCapacity] = {};
    detail::NativeFastPathState native_fast_path;
    bool returned = false;
    bool process_exit = false;
    bool dos_termination_captured = false;
    std::uint32_t dos_termination_ax = 0;
    std::uint32_t dos_termination_eip = 0;
    std::uint32_t dos_termination_esp = 0;
    std::uint32_t dos_termination_stack[kWin32DosTerminationStackCapacity] = {};
    bool exception_caught = false;
    std::uint32_t exception_code = 0;
    std::uint32_t exception_address = 0;
    std::uint32_t exception_eax = 0;
    std::uint32_t exception_ebx = 0;
    std::uint32_t exception_ecx = 0;
    std::uint32_t exception_edx = 0;
    std::uint32_t exception_esi = 0;
    std::uint32_t exception_edi = 0;
    X86ExecutionSnapshot exception_snapshot;
    std::uint32_t exception_access_kind = 0xFFFFFFFFU;
    std::uint32_t exception_fault_va = 0;
    std::uint32_t exception_fault_region_base = 0;
    std::uint32_t exception_fault_alloc_base = 0;
    std::uint32_t exception_fault_state = 0;
    std::uint32_t exception_fault_protect = 0;
    std::uint32_t exception_fault_region_size = 0;
    std::uint32_t exception_esi_dwords[8] = {};
    std::uint32_t exception_esi_dword_valid_mask = 0;
    std::uint8_t exception_register_strings[6][32] = {};
    std::uint32_t exception_register_string_valid_mask = 0;
    std::uint32_t exception_stack_base = 0;
    std::uint32_t exception_stack_dwords[kWin32ExceptionStackDwordCapacity] = {};
    std::uint32_t exception_stack_dword_count = 0;
    std::uint32_t aot_probe_guest_address = 0;
    std::uint32_t aot_probe_cache_address = 0;
    std::uint32_t aot_probe_cache_valid = 0;
    std::uint8_t aot_probe_cache_bytes[32] = {};
    std::uint32_t handled_fatal_breakpoint_count = 0;
    std::uint32_t last_fatal_breakpoint_address = 0;
    std::uint32_t last_fatal_message_address = 0;
    std::string last_fatal_message;
    bool fatal_breakpoint_continued = false;
    bool fatal_halt_reached = false;
    std::uint32_t handled_hle_trap_count = 0;
    std::uint32_t last_hle_trap_address = 0;
    std::uint32_t last_hle_trap_opcode = 0;
    repiu::hle::LinexeCallGatePlan linexe_gate_plan;
    repiu::hle::LinexeArenaLayout linexe_arena_layout;
    bool linexe_environment_active = false;
    std::uint32_t linexe_gs_byte_load_count = 0;
    std::uint32_t linexe_first_gs_byte_offset = 0;
    std::uint32_t linexe_first_gs_byte_value = 0;
    std::uint32_t linexe_scan_entry_count = 0;
    std::uint32_t linexe_module_candidate_count = 0;
    std::uint32_t linexe_module_match_count = 0;
    std::uint32_t linexe_name_pointer_valid_count = 0;
    std::uint32_t linexe_name_byte_instruction_count = 0;
    std::uint32_t linexe_data_gs_load_count = 0;
    std::uint16_t linexe_module_selector_stack_value = 0;
    std::uint32_t linexe_module_offset_stack_value = 0;
    std::uint32_t linexe_export_offset_stack_value = 0;
    std::uint16_t linexe_export_selector_stack_value = 0;
    std::uint32_t linexe_export_jump_source_esp = 0;
    std::uint32_t linexe_export_jump_source_module_offset = 0;
    std::uint16_t linexe_export_jump_source_module_selector = 0;
    std::uint32_t linexe_export_jump_target_esp = 0;
    std::uint32_t linexe_export_jump_target_module_offset = 0;
    std::uint16_t linexe_export_jump_target_module_selector = 0;
    std::uint32_t linexe_export_name_compare_count = 0;
    std::uint16_t linexe_export_name_compare_gs = 0;
    std::uint32_t linexe_export_name_compare_edi = 0;
    std::uint32_t linexe_export_name_compare_esi = 0;
    std::uint8_t linexe_export_name_actual_byte = 0;
    std::uint8_t linexe_export_name_expected_byte = 0;
    std::uint32_t linexe_export_name_stage_mask = 0;
    std::uint32_t linexe_export_entry_name_offset_value = 0;
    std::uint32_t linexe_export_entry_name_selector_value = 0;
    std::uint32_t linexe_export_result_store_destination = 0;
    std::uint32_t linexe_export_result_store_value = 0;
    std::uint32_t linexe_export_result_store_count = 0;
    std::uint16_t linexe_export_value_load_selector = 0;
    std::uint32_t linexe_export_value_load_offset = 0;
    std::uint32_t linexe_export_value_load_value = 0;
    std::uint32_t linexe_root_selector_eax = 0;
    std::uint16_t linexe_root_read_gs = 0;
    std::uint32_t linexe_shared_load_entry_count = 0;
    std::uint32_t linexe_shared_load_read_count = 0;
    std::uint16_t linexe_shared_load_selector = 0;
    std::uint32_t linexe_shared_load_offset = 0;
    std::uint32_t linexe_shared_load_value = 0;
    std::uint32_t linexe_root_offset_load_value = 0;
    std::uint32_t linexe_root_selector_load_value = 0;
    std::uint32_t linexe_root_offset_load_success = 0;
    std::uint32_t linexe_root_selector_load_success = 0;
    std::uint32_t linexe_export_match_count = 0;
    std::uint32_t linexe_export_entry_loop_count = 0;
    std::uint32_t linexe_export_compare_count = 0;
    std::uint32_t linexe_export_compare_eax = 0;
    std::uint32_t linexe_export_compare_ecx = 0;
    std::uint32_t linexe_export_compare_eflags = 0;
    std::uint32_t linexe_export_count_load_edx = 0;
    std::uint16_t linexe_export_count_load_gs = 0;
    std::uint32_t linexe_scan_return_count = 0;
    std::uint32_t linexe_indirect_far_call_count = 0;
    std::uint32_t linexe_indirect_far_call_source = 0;
    std::uint32_t linexe_indirect_far_call_pointer = 0;
    std::uint32_t linexe_indirect_far_call_offset = 0;
    std::uint16_t linexe_indirect_far_call_selector = 0;
    bool linexe_indirect_far_call_known_export = false;
    std::uint32_t linexe_bridge_entry_count = 0;
    bool linexe_bridge_gate_valid = false;
    std::uint16_t linexe_bridge_selector = 0;
    std::uint32_t linexe_bridge_offset = 0;
    std::uint32_t linexe_bridge_service = 0;
    std::uint32_t linexe_bridge_esp = 0;
    std::uint32_t linexe_bridge_ebp = 0;
    std::uint32_t linexe_bridge_stack[20] = {};
    char linexe_bridge_argument_text[128] = {};
    char linexe_bridge_stack_text[20][64] = {};
    std::uint32_t linexe_virtual_module_load_count = 0;
    std::uint32_t linexe_virtual_module_handle = 0;
    std::uint32_t linexe_get_proc_count = 0;
    std::uint32_t linexe_get_proc_result_pointer = 0;
    char linexe_get_proc_name[64] = {};
    std::uint32_t glide_gate_entry_count = 0;
    std::uint32_t glide_gate_handled_count = 0;
    std::uint32_t glide_gate_esp = 0;
    std::uint32_t glide_gate_stack[8] = {};
    std::uint16_t glide_gate_ordinal = 0;
    std::uint32_t glide_gate_argument_bytes = 0;
    char glide_gate_name[64] = {};
    std::uint32_t glide_texture_gate_trace_count = 0;
    bool glide_texture_gate_trace_wrapped = false;
    Win32GlideTextureGateTraceEntry glide_texture_gate_trace[kWin32GlideTextureGateTraceCapacity] = {};
    Win32GlideTriangleObservation glide_first_triangle;
    std::uint32_t glide_triangle_trace_count = 0;
    bool glide_triangle_trace_wrapped = false;
    Win32GlideTriangleTraceEntry glide_triangle_trace[kWin32GlideTriangleTraceCapacity] = {};
    std::array<std::uint32_t, 256> glide_call_counts = {};
    std::array<std::array<std::uint32_t, 8>, 256> glide_first_stacks = {};
    std::array<std::string, 256> glide_call_names = {};
    std::uint32_t glide_window_open_count = 0;
    std::uint32_t glide_logical_width = 0;
    std::uint32_t glide_logical_height = 0;
    std::string glide_backend_message;
    std::vector<exe::LeResidentName> glide_exports;
    repiu::hle::GlideGatePlan glide_gate_plan;
    repiu::media::ChdCdImage cd_image;
    CdAudioWaveOut cd_audio;
    bool mscdex_available = false;
    bool cd_audio_available = false;
    std::uint8_t mscdex_drive = 3;
    std::uint32_t mscdex_request_count = 0;
    std::uint16_t mscdex_frame_es = 0;
    std::uint32_t mscdex_decline_count = 0;
    std::uint32_t mscdex_last_decline_reason = 0;
    std::uint32_t mscdex_last_resolve_kind = 0;
    std::uint32_t mscdex_last_header_bytes = 0;
    repiu::hle::GlideLogicalState glide_state;
    GlideOpenGlBackend glide_backend;
    // R4 LFB staging surface handed to the guest by grLfbLock. Host-owned (see
    // design 257 3.1): the guest writes it with native instructions under the
    // flat DS, so it does not need to live inside the runtime arena.
    repiu::hle::GlideLfbSurface glide_lfb_surface;
    std::uint32_t glide_lfb_lock_count = 0;
    std::uint32_t glide_lfb_present_count = 0;
    std::uint32_t linexe_scan_return_eax = 0;
    std::uint32_t linexe_scan_return_ebp = 0;
    std::uint32_t linexe_scan_caller_eax = 0;
    std::uint32_t linexe_selector_init_results[3] = {};
    std::uint32_t dpmi_allocate_call_count = 0;
    std::uint16_t dpmi_last_allocate_requested_count = 0;
    std::uint16_t dpmi_last_allocated_selector = 0;
    Win32PortIoObservation port_io;
    Win32DosPathObservation dos_path;
    Win32DosFileIoObservation dos_file_io;
    Win32AllocatorProbeObservation allocator_probe;
    Win32AllocatorControlFlowObservation allocator_control_flow;
    std::uint32_t handled_dos_interrupt_count = 0;
    std::uint32_t last_dos_interrupt_vector = 0;
    std::uint32_t last_dos_interrupt_ah = 0;
    std::uint32_t last_dos_interrupt_ax = 0;
    repiu::hle::DosVirtualFileSystemState dos_file_system;
    std::uint32_t handled_dos_chdir_count = 0;
    std::string last_dos_chdir_guest_path;
    std::string last_dos_chdir_host_path;
    std::string last_dos_chdir_virtual_path;
    bool last_dos_chdir_success = false;
    std::uint16_t last_dos_chdir_error = 0;
    std::uint32_t handled_dos_getcwd_count = 0;
    std::uint8_t last_dos_getcwd_drive = 0;
    std::string last_dos_getcwd_path;
    bool last_dos_getcwd_success = false;
    std::uint16_t last_dos_getcwd_error = 0;
    std::uint32_t handled_dos_getdrive_count = 0;
    std::uint8_t last_dos_getdrive_value = 0;
    std::uint32_t handled_dos_open_count = 0;
    std::string last_dos_open_guest_path;
    std::string last_dos_open_host_path;
    std::string last_dos_open_virtual_path;
    bool last_dos_open_success = false;
    std::uint16_t last_dos_open_error = 0;
    std::uint16_t last_dos_open_handle = 0;
    std::uint8_t last_dos_open_access_mode = 0;
    std::uint32_t handled_dos_read_count = 0;
    std::uint16_t last_dos_read_handle = 0;
    std::uint32_t last_dos_read_requested_bytes = 0;
    std::uint32_t last_dos_read_actual_bytes = 0;
    std::uint32_t last_dos_read_buffer = 0;
    bool last_dos_read_success = false;
    std::uint16_t last_dos_read_error = 0;
    std::uint32_t handled_dos_seek_count = 0;
    std::uint16_t last_dos_seek_handle = 0;
    std::uint8_t last_dos_seek_origin = 0;
    std::int32_t last_dos_seek_offset = 0;
    std::uint32_t last_dos_seek_position = 0;
    bool last_dos_seek_success = false;
    std::uint16_t last_dos_seek_error = 0;
    std::uint32_t handled_dos_close_count = 0;
    std::uint16_t last_dos_close_handle = 0;
    bool last_dos_close_success = false;
    std::uint16_t last_dos_close_error = 0;
    std::uint32_t handled_dos_ioctl_count = 0;
    std::uint8_t last_dos_ioctl_subfunction = 0;
    std::uint16_t last_dos_ioctl_handle = 0;
    bool last_dos_ioctl_success = false;
    std::uint16_t last_dos_ioctl_error = 0;
    std::uint16_t last_dos_ioctl_device_info = 0;
    std::uint32_t handled_dos_resize_count = 0;
    std::uint16_t last_dos_resize_selector = 0;
    std::uint16_t last_dos_resize_paragraphs = 0;
    bool last_dos_resize_success = false;
    std::uint16_t last_dos_resize_error = 0;
    std::uint32_t last_dos_resize_requested_end = 0;
    std::uint32_t last_dos_resize_allocator_end = 0;
    std::uint32_t handled_segment_load_count = 0;
    std::uint32_t last_segment_load_address = 0;
    std::uint32_t last_segment_load_opcode = 0;
    std::uint32_t last_segment_load_register = 0;
    std::uint32_t last_segment_load_selector = 0;
    std::uint32_t last_segment_load_source = 0;
    Win32SegmentLoadObservation segment_load;
    std::uint32_t handled_segment_store_count = 0;
    std::uint32_t last_segment_store_address = 0;
    std::uint32_t last_segment_store_opcode = 0;
    std::uint32_t last_segment_store_register = 0;
    std::uint32_t last_segment_store_selector = 0;
    std::uint32_t last_segment_store_destination = 0;
    std::uint32_t handled_segment_memory_load_count = 0;
    std::uint32_t last_segment_memory_load_address = 0;
    std::uint32_t last_segment_memory_load_opcode = 0;
    std::uint32_t last_segment_memory_load_register = 0;
    std::uint32_t last_segment_memory_load_selector = 0;
    std::uint32_t last_segment_memory_load_offset = 0;
    std::uint32_t last_segment_memory_load_width = 0;
    std::uint32_t last_segment_memory_load_value = 0;
    std::uint32_t handled_low_memory_access_count = 0;
    std::uint32_t last_low_memory_access_address = 0;
    std::uint32_t last_low_memory_access_opcode = 0;
    std::uint32_t last_low_memory_access_esi = 0;
    std::uint32_t last_low_memory_access_edi = 0;
    std::uint32_t last_low_memory_access_destination = 0;
    std::uint32_t last_low_memory_access_value = 0;
    std::uint32_t low_memory_read_emulate_count = 0;
    std::uint32_t last_low_memory_read_emulate_address = 0;
    std::uint32_t last_low_memory_read_emulate_eip = 0;
    std::uint32_t last_low_memory_read_emulate_value = 0;
    std::uint32_t last_low_memory_read_emulate_reg = 0;
    std::uint32_t last_low_memory_fault_eip = 0;
    std::uint32_t last_low_memory_fault_address = 0;
    std::uint32_t low_memory_fault_repeat_count = 0;
    std::uint32_t last_low_memory_fault_tick = 0;
    std::uint32_t rep_movs_copy_failure_count = 0;
    std::uint32_t last_rep_movs_copy_failure_stage = 0;
    std::uint32_t last_rep_movs_copy_error = 0;
    std::uint32_t last_rep_movs_copy_source = 0;
    std::uint32_t last_rep_movs_copy_destination = 0;
    std::uint32_t last_rep_movs_copy_bytes = 0;
    std::vector<std::uint8_t> dos_environment_block;
    bool last_dos_environment_access_valid = false;
    std::uint32_t last_dos_environment_access_offset = 0;
    std::uint32_t last_dos_environment_entry_offset = 0;
    std::uint32_t last_dos_environment_value_length = 0;
    std::string last_dos_environment_entry_name;
    std::uint32_t handled_memory_store_count = 0;
    std::uint32_t last_memory_store_address = 0;
    std::uint32_t last_memory_store_opcode = 0;
    std::uint32_t last_memory_store_destination = 0;
    std::uint32_t last_memory_store_value = 0;
    std::uint32_t last_memory_store_width = 0;
    std::string last_memory_store_source_kind;
    bool last_memory_store_applied = false;
    std::uint32_t shadow_memory_write_count = 0;
    std::uint32_t shadow_memory_read_hit_count = 0;
    bool shadow_memory_range_valid = false;
    std::uint32_t shadow_memory_min_address = 0;
    std::uint32_t shadow_memory_max_address = 0;
    std::uint32_t last_traced_fpu_m32_value = 0;
    bool has_last_traced_fpu_m32_value = false;
    std::atomic<std::uint32_t> single_step_trace_count{0};
    // Route A sizing (native region execution). Of every single-stepped guest
    // instruction, how many are HLE-sensitive (segment op / INT / IO / string /
    // privileged) and would still require a trap under selective-breakpoint
    // region execution. Native-region speedup ceiling ~=
    // single_step_trace_count / routea_sensitive_count. The segment sub-count
    // isolates how much of that is segmentation specifically.
    std::atomic<std::uint32_t> routea_sensitive_count{0};
    std::atomic<std::uint32_t> routea_segment_sensitive_count{0};
    std::atomic<std::uint32_t> single_step_eip{0};
    std::atomic<std::uint32_t> single_step_eax{0};
    std::atomic<std::uint32_t> single_step_ebx{0};
    std::atomic<std::uint32_t> single_step_ecx{0};
    std::atomic<std::uint32_t> single_step_edx{0};
    std::atomic<std::uint32_t> single_step_esi{0};
    std::atomic<std::uint32_t> single_step_edi{0};
    std::atomic<std::uint32_t> single_step_esp{0};
    std::atomic<std::uint32_t> single_step_ebp{0};
    std::atomic<std::uint32_t> single_step_eflags{0};
    std::atomic<std::uint32_t> single_step_cs{0};
    std::atomic<std::uint32_t> single_step_ds{0};
    std::atomic<std::uint32_t> single_step_es{0};
    std::atomic<std::uint32_t> single_step_ss{0};
    std::atomic<std::uint32_t> single_step_fs{0};
    std::atomic<std::uint32_t> single_step_gs{0};
    std::atomic<std::uint32_t> diagnostic_progress_count{0};
    std::uint32_t diagnostic_poll_iteration_count = 0;
    std::uint32_t diagnostic_quiet_iteration_count = 0;
    std::atomic<std::uint32_t> exception_dispatch_entry_count{0};
    std::atomic<std::uint32_t> exception_dispatch_exit_count{0};
    std::atomic<std::uint32_t> exception_dispatch_last_eip{0};
    std::atomic<std::uint32_t> live_telemetry_heartbeat{0};
    std::atomic<std::uint32_t> live_telemetry_phase{0};
    Win32SharedLiveTelemetry* shared_live_telemetry = nullptr;
    void* vectored_handler = nullptr;
    std::unordered_map<std::uint32_t, std::uint8_t> shadow_memory;
    std::array<ShadowWriteProvenance, kShadowWriteProvenanceCapacity>
        shadow_write_provenance = {};
    std::uint32_t shadow_write_provenance_count = 0;
    bool pending_shadow_allocation_valid = false;
    std::uint32_t pending_shadow_allocation_size = 0;
    bool shadow_zero_payload_valid = false;
    std::uint32_t shadow_zero_payload_begin = 0;
    std::uint32_t shadow_zero_payload_end = 0;
    bool boundary_object_chain_valid = false;
    std::uint32_t boundary_object_chain_base = 0;
    std::uint32_t boundary_object_chain_frontier = 0;
    std::uint64_t boundary_object_chain_limit = 0;
    std::uint16_t guest_es = 0;
    std::uint16_t guest_ss = 0;
    std::uint16_t guest_ds = 0;
    std::uint16_t guest_fs = 0;
    std::uint16_t guest_gs = 0;
    repiu::runtime::SelectorTable selector_table;
    repiu::runtime::SelectorAllocator dpmi_selector_allocator;
    repiu::runtime::DosLowMemory dos_low_memory;
    std::array<DosInterruptVectorShadow, 256> dos_interrupt_vectors = {};
    std::array<DpmiInterruptVectorShadow, 256> dpmi_interrupt_vectors = {};
    std::atomic<bool> timer_interrupt_pending{false};
    std::uint32_t last_timer_injection_ticks = 0;
    std::uint32_t timer_interrupt_chain_hle_count = 0;
    std::uint32_t timer_interrupt_chain_hle_source = 0;
    std::uint32_t timer_interrupt_chain_hle_pointer = 0;
    std::uint32_t timer_interrupt_chain_hle_offset = 0;
    std::uint16_t timer_interrupt_chain_hle_selector = 0;
    char hle_stdout_output[4096] = {};
    std::uint32_t hle_stdout_output_size = 0;
    char hle_stderr_output[4096] = {};
    std::uint32_t hle_stderr_output_size = 0;
    std::string hle_message;

    struct RealModeBlock
    {
        std::uint16_t selector = 0;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        bool active = false;
    };
    static constexpr std::size_t kRealModeBlockCapacity = 32;
    std::array<RealModeBlock, kRealModeBlockCapacity> dpmi_real_mode_blocks = {};
    std::uint32_t dpmi_low_memory_bump_offset = 0x1000U;
    std::uint32_t debug_emulate_stage = 0;
    std::uint32_t debug_emulate_decode_result = 0;
    std::uint32_t debug_emulate_calculated_address = 0;
};
} // namespace repiu::platform::win32
