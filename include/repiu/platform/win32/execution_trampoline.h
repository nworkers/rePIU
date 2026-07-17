#ifndef REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_
#define REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_

#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/guest_context.h"
#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/hle/dos_file_system.h"
#include "repiu/exe/dos16m_bound_module.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32PortIoTraceCapacity = 16;
constexpr std::uint32_t kWin32DosPathTraceCapacity = 16;
constexpr std::uint32_t kWin32DosFileIoTraceCapacity = 64;
constexpr std::uint32_t kWin32DosFileIoPrefixCapacity = 16;
constexpr std::uint32_t kWin32DosTerminationStackCapacity = 128;
constexpr std::uint32_t kWin32ExceptionStackDwordCapacity = 96;
constexpr std::uint32_t kWin32AllocatorProbeTraceCapacity = 16;
constexpr std::uint32_t kWin32AllocatorControlFlowTraceCapacity = 32;
constexpr std::uint32_t kWin32SegmentLoadTraceCapacity = 16;
constexpr std::uint32_t kWin32DeferredPortIoLimit = 65536;
constexpr std::uint32_t kWin32ExecutionTraceCapacity = 64;

// One capture per single-stepped instruction inside a guest code range
// (see RecordExecutionTrace). `value_at_esp_offset` is read relative to the
// live ESP at capture time, not a hardcoded absolute address, so it stays
// correct across stack reuse between calls to the traced function.
struct Win32ExecutionTraceEntry
{
    std::uint32_t sequence = 0;
    std::uint32_t eip = 0;
    std::uint32_t esp = 0;
    std::uint32_t value_at_esp_offset = 0;
};

struct X86ExecutionSnapshot
{
    bool captured = false;
    std::uint32_t eip = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t esp = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
    std::uint16_t cs = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    std::uint16_t ss = 0;
    std::uint16_t fs = 0;
    std::uint16_t gs = 0;
};

struct Win32PortIoTraceEntry
{
    bool valid = false;
    std::uint32_t sequence = 0;
    std::uint32_t address = 0;
    std::uint32_t opcode = 0;
    std::uint32_t port = 0;
    std::uint32_t width = 0;
    std::uint32_t value = 0;
    bool is_input = false;
    bool handled = false;
};

struct Win32PortIoObservation
{
    std::uint32_t observed_count = 0;
    std::uint32_t last_address = 0;
    std::uint32_t last_opcode = 0;
    std::uint32_t last_port = 0;
    std::uint32_t last_width = 0;
    std::uint32_t last_value = 0;
    bool last_is_input = false;
    bool last_handled = false;
    std::string last_result;
    std::uint32_t trace_stored_count = 0;
    bool trace_limit_reached = false;
    Win32PortIoTraceEntry trace[kWin32PortIoTraceCapacity];
};

struct Win32DosPathTraceEntry
{
    bool valid = false;
    std::uint32_t sequence = 0;
    std::string service;
    std::string guest_path;
    std::string virtual_path;
    std::string host_path;
    std::string result;
    std::uint16_t dos_error = 0;
    std::uint8_t drive = 0;
    std::uint8_t access_mode = 0;
};

struct Win32DosPathObservation
{
    std::uint32_t observed_count = 0;
    std::uint32_t trace_stored_count = 0;
    bool trace_limit_reached = false;
    Win32DosPathTraceEntry trace[kWin32DosPathTraceCapacity];
};

struct Win32DosFileIoTraceEntry
{
    bool valid = false;
    std::uint32_t sequence = 0;
    std::string operation;
    std::string host_path;
    std::uint16_t handle = 0;
    std::uint8_t origin = 0;
    std::int32_t seek_offset = 0;
    std::uint32_t position_before = 0;
    std::uint32_t position_after = 0;
    std::uint32_t requested_bytes = 0;
    std::uint32_t actual_bytes = 0;
    std::uint16_t dos_error = 0;
    std::uint8_t prefix[kWin32DosFileIoPrefixCapacity] = {};
    std::uint32_t prefix_size = 0;
    std::uint32_t guest_eip = 0;
    std::uint32_t guest_esp = 0;
    std::uint32_t guest_stack[8] = {};
};

struct Win32DosFileIoObservation
{
    std::uint32_t observed_count = 0;
    std::uint32_t trace_stored_count = 0;
    bool trace_wrapped = false;
    Win32DosFileIoTraceEntry trace[kWin32DosFileIoTraceCapacity];
};

struct Win32AllocatorProbeTraceEntry
{
    bool valid = false;
    std::uint32_t sequence = 0;
    std::uint32_t eax = 0;
    std::uint32_t esi = 0;
    std::uint32_t source = 0;
    std::uint16_t ds = 0;
    bool pending_before = false;
    std::uint32_t pending_size_before = 0;
    bool pending_after = false;
    std::uint32_t pending_size_after = 0;
    std::string result;
};

struct Win32AllocatorProbeObservation
{
    std::uint32_t observed_count = 0;
    std::uint32_t trace_stored_count = 0;
    bool trace_wrapped = false;
    Win32AllocatorProbeTraceEntry
        trace[kWin32AllocatorProbeTraceCapacity];
};

struct Win32AllocatorControlFlowTraceEntry
{
    bool valid = false;
    std::uint32_t sequence = 0;
    std::uint32_t eip_offset = 0;
    std::uint32_t seh_code = 0;
    std::uint8_t opcode[4] = {};
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t eflags = 0;
    bool pending_valid = false;
    std::uint32_t pending_size = 0;
    bool read_valid = false;
    std::uint32_t read_address = 0;
    std::uint32_t read_value = 0;
    bool read_explicit_shadow = false;
    bool read_zero_backed = false;
    bool writer_valid = false;
    std::uint32_t writer_sequence = 0;
    std::uint32_t writer_eip_offset = 0;
    std::uint32_t writer_opcode = 0;
    std::uint32_t writer_destination = 0;
    std::uint32_t writer_value = 0;
    std::uint32_t writer_width = 0;
};

struct Win32AllocatorControlFlowObservation
{
    std::uint32_t observed_count = 0;
    std::uint32_t trace_stored_count = 0;
    bool trace_wrapped = false;
    bool null_link_transition_valid = false;
    Win32AllocatorControlFlowTraceEntry null_link_transition;
    bool poison_link_transition_valid = false;
    Win32AllocatorControlFlowTraceEntry poison_link_transition;
    bool root_transition_valid = false;
    Win32AllocatorControlFlowTraceEntry root_transition;
    Win32AllocatorControlFlowTraceEntry
        trace[kWin32AllocatorControlFlowTraceCapacity];
};

struct Win32SegmentLoadTraceEntry
{
    bool valid = false;
    std::uint32_t sequence = 0;
    std::uint32_t eip_offset = 0;
    std::uint8_t segment_register = 0;
    std::uint16_t selector = 0;
    std::uint32_t source = 0;
};

struct Win32SegmentLoadObservation
{
    std::uint32_t observed_count = 0;
    std::uint32_t trace_stored_count = 0;
    bool trace_wrapped = false;
    Win32SegmentLoadTraceEntry trace[kWin32SegmentLoadTraceCapacity];
};

struct Win32AotReturnTraceEntry
{
    std::uint32_t source = 0;
    std::uint32_t actual_target = 0;
    std::uint32_t expected_target = 0;
    std::uint32_t esp = 0;
    bool matches = false;
};

constexpr std::uint32_t kWin32AotReturnTraceCapacity = 16;

struct Win32AotTransferTraceEntry
{
    std::uint32_t source = 0;
    std::uint32_t target = 0;
    bool is_call = false;
};

constexpr std::uint32_t kWin32AotTransferTraceCapacity = 32;

struct Win32MinimalExecutionAttempt
{
    bool valid = false;
    bool supported = false;
    bool attempted = false;
    bool returned = false;
    bool exception_caught = false;
    bool timed_out = false;
    bool guest_stack_switch_supported = false;
    bool guest_stack_switch_attempted = false;
    std::uint32_t entry_address = 0;
    std::uint32_t guest_stack_initial_esp = 0;
    std::uint32_t guest_stack_return_esp = 0;
    std::uint32_t seh_exception_code = 0;
    std::uint32_t seh_exception_address = 0;
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
    bool fatal_halt_reached = false;
    X86ExecutionSnapshot timeout_snapshot;
    X86ExecutionSnapshot last_single_step_snapshot;
    std::uint32_t single_step_trace_count = 0;
    std::uint32_t native_fast_path_entry_count = 0;
    std::uint32_t native_fast_path_return_count = 0;
    std::uint32_t native_fast_path_cancel_count = 0;
    std::uint32_t native_fast_path_last_entry = 0;
    std::uint32_t native_fast_path_last_return = 0;
    bool aot_backend_active = false;
    std::uint32_t aot_cache_entry_count = 0;
    std::uint32_t aot_boundary_count = 0;
    std::uint32_t aot_reentry_count = 0;
    std::uint32_t aot_legacy_fallback_count = 0;
    std::uint32_t aot_last_fallback_address = 0;
    std::uint32_t aot_dynamic_attempt_count = 0;
    std::uint32_t aot_dynamic_success_count = 0;
    std::uint32_t aot_dynamic_added_bytes = 0;
    std::uint32_t aot_indirect_dispatch_count = 0;
    std::uint32_t aot_inline_cache_patch_attempt_count = 0;
    std::uint32_t aot_inline_cache_patch_success_count = 0;
    std::uint32_t aot_inline_cache_site_count = 0;
    std::uint32_t aot_last_reentry_cache_address = 0;
    std::uint32_t aot_code_write_count = 0;
    std::uint32_t aot_page_retire_attempt_count = 0;
    std::uint32_t aot_page_retire_success_count = 0;
    std::uint32_t aot_generation_publish_count = 0;
    std::uint32_t aot_generation_failure_count = 0;
    std::uint32_t aot_generation_relinked_entry_count = 0;
    std::uint32_t aot_retired_entry_trap_count = 0;
    std::uint32_t aot_quarantine_count = 0;
    std::uint32_t aot_last_code_write_source = 0;
    std::uint32_t aot_last_code_write_destination = 0;
    std::uint32_t aot_last_retired_page = 0;
    std::uint32_t aot_last_published_generation = 0;
    bool aot_exception_mapping_valid = false;
    std::uint32_t aot_exception_cache_address = 0;
    std::uint32_t aot_exception_guest_address = 0;
    std::uint8_t aot_exception_cache_bytes[16] = {};
    std::uint8_t aot_exception_guest_bytes[16] = {};
    std::uint32_t aot_last_indirect_source = 0;
    std::uint32_t aot_last_indirect_target = 0;
    std::uint32_t aot_return_dispatch_count = 0;
    std::uint32_t aot_last_return_target = 0;
    std::uint32_t aot_last_return_source = 0;
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
    Win32ExecutionTraceEntry execution_trace[kWin32ExecutionTraceCapacity];
    std::uint32_t aot_call_depth = 0;
    bool aot_last_return_matches_call = false;
    std::uint32_t aot_last_expected_return = 0;
    std::uint32_t aot_last_call_source = 0;
    std::uint32_t aot_last_call_target = 0;
    std::uint32_t aot_last_expected_call_source = 0;
    std::uint32_t aot_last_expected_call_target = 0;
    std::uint32_t aot_return_trace_count = 0;
    Win32AotReturnTraceEntry
        aot_return_trace[kWin32AotReturnTraceCapacity];
    std::uint32_t aot_transfer_trace_count = 0;
    Win32AotTransferTraceEntry
        aot_transfer_trace[kWin32AotTransferTraceCapacity];
    std::uint32_t diagnostic_poll_iteration_count = 0;
    std::uint32_t diagnostic_progress_count = 0;
    std::uint32_t diagnostic_quiet_iteration_count = 0;
    std::uint32_t exception_dispatch_entry_count = 0;
    std::uint32_t exception_dispatch_exit_count = 0;
    std::uint32_t exception_dispatch_last_eip = 0;
    bool selector_table_valid = false;
    std::uint32_t selector_descriptor_count = 0;
    bool linexe_environment_active = false;
    std::uint16_t linexe_saved_client_gs = 0;
    bool linexe_client_descriptor_valid = false;
    std::uint32_t linexe_client_descriptor_base = 0;
    std::uint32_t linexe_client_descriptor_limit = 0;
    std::uint16_t linexe_root_offset = 0;
    std::uint16_t linexe_root_selector = 0;
    bool linexe_data_descriptor_valid = false;
    std::uint32_t linexe_data_descriptor_base = 0;
    std::uint16_t linexe_module_name_offset = 0;
    std::uint16_t linexe_module_name_selector = 0;
    std::uint16_t linexe_direct_export_count = 0;
    std::uint16_t linexe_direct_export_table_offset = 0;
    std::uint16_t linexe_direct_export_table_selector = 0;
    std::string linexe_direct_module_name;
    std::uint32_t linexe_gs_byte_load_count = 0;
    std::uint32_t linexe_first_gs_byte_offset = 0;
    std::uint32_t linexe_first_gs_byte_value = 0;
    std::uint16_t linexe_selector_words[4] = {};
    std::uint32_t linexe_resolved_exports[8] = {};
    std::uint32_t linexe_resolved_export_count = 0;
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
    std::uint16_t linexe_direct_first_export_name_offset = 0;
    std::uint16_t linexe_direct_first_export_name_selector = 0;
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
    struct GlideCallObservation
    {
        std::uint16_t ordinal = 0;
        std::uint32_t count = 0;
        std::uint32_t first_stack[8] = {};
        std::string name;
    };
    std::vector<GlideCallObservation> glide_calls;
    bool mscdex_available = false;
    bool cd_audio_available = false;
    std::uint32_t mscdex_track_count = 0;
    std::uint32_t mscdex_request_count = 0;
    std::uint16_t mscdex_frame_es = 0;
    std::uint32_t mscdex_decline_count = 0;
    std::uint32_t mscdex_last_decline_reason = 0;
    std::uint32_t mscdex_last_resolve_kind = 0;
    std::uint32_t mscdex_last_header_bytes = 0;
    std::uint32_t cd_audio_current_lba = 0;
    std::uint32_t glide_window_open_count = 0;
    std::uint32_t glide_logical_width = 0;
    std::uint32_t glide_logical_height = 0;
    std::string glide_backend_message;
    std::uint32_t glide_texture_memory_bytes = 0;
    std::uint32_t glide_texture_max_address = 0;
    std::uint32_t linexe_scan_return_eax = 0;
    std::uint32_t linexe_scan_return_ebp = 0;
    std::uint32_t linexe_scan_caller_eax = 0;
    std::uint32_t linexe_selector_init_results[3] = {};
    std::uint32_t dpmi_allocate_call_count = 0;
    std::uint16_t dpmi_last_allocate_requested_count = 0;
    std::uint16_t dpmi_last_allocated_selector = 0;
    bool dos_low_memory_valid = false;
    std::uint32_t dos_low_memory_size = 0;
    std::uint32_t dos_environment_block_size = 0;
    bool last_dos_environment_access_valid = false;
    std::uint32_t last_dos_environment_access_offset = 0;
    std::uint32_t last_dos_environment_entry_offset = 0;
    std::uint32_t last_dos_environment_value_length = 0;
    std::string last_dos_environment_entry_name;
    std::uint32_t handled_hle_trap_count = 0;
    std::uint32_t last_hle_trap_address = 0;
    std::uint32_t last_hle_trap_opcode = 0;
    Win32PortIoObservation port_io;
    Win32DosPathObservation dos_path;
    Win32DosFileIoObservation dos_file_io;
    Win32AllocatorProbeObservation allocator_probe;
    Win32AllocatorControlFlowObservation allocator_control_flow;
    std::uint32_t handled_dos_interrupt_count = 0;
    std::uint32_t last_dos_interrupt_vector = 0;
    std::uint32_t last_dos_interrupt_ah = 0;
    std::uint32_t last_dos_interrupt_ax = 0;
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
    std::uint32_t debug_emulate_stage = 0;
    std::uint32_t debug_emulate_decode_result = 0;
    std::uint32_t debug_emulate_calculated_address = 0;
    std::uint32_t rep_movs_copy_failure_count = 0;
    std::uint32_t last_rep_movs_copy_failure_stage = 0;
    std::uint32_t last_rep_movs_copy_error = 0;
    std::uint32_t last_rep_movs_copy_source = 0;
    std::uint32_t last_rep_movs_copy_destination = 0;
    std::uint32_t last_rep_movs_copy_bytes = 0;
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
    std::uint32_t shadow_memory_byte_count = 0;
    bool shadow_memory_range_valid = false;
    std::uint32_t shadow_memory_min_address = 0;
    std::uint32_t shadow_memory_max_address = 0;
    std::uint32_t thread_exit_code = 0;
    bool dos_termination_captured = false;
    std::uint32_t dos_termination_ax = 0;
    std::uint32_t dos_termination_eip = 0;
    std::uint32_t dos_termination_esp = 0;
    std::uint32_t dos_termination_stack[kWin32DosTerminationStackCapacity] = {};
    std::string hle_stdout_output;
    std::string hle_stderr_output;
    std::string message;
};

bool AttemptWin32MinimalExecution(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackTrapExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackAotExecution(
    const Win32RelocatedImagePlacement& placement,
    Win32AotCodeCachePlacement& aot_placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    bool enable_dynamic_translation,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackHleExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_
