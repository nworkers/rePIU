#ifndef REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_
#define REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_

#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/guest_context.h"
#include "repiu/hle/dos_file_system.h"
#include "repiu/exe/dos16m_bound_module.h"

#include <cstdint>
#include <string>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32PortIoTraceCapacity = 16;
constexpr std::uint32_t kWin32DosPathTraceCapacity = 16;
constexpr std::uint32_t kWin32AllocatorProbeTraceCapacity = 16;
constexpr std::uint32_t kWin32AllocatorControlFlowTraceCapacity = 32;
constexpr std::uint32_t kWin32SegmentLoadTraceCapacity = 16;
constexpr std::uint32_t kWin32DeferredPortIoLimit = 1024;

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
    std::uint32_t handled_fatal_breakpoint_count = 0;
    std::uint32_t last_fatal_breakpoint_address = 0;
    std::uint32_t last_fatal_message_address = 0;
    std::string last_fatal_message;
    bool fatal_halt_reached = false;
    X86ExecutionSnapshot timeout_snapshot;
    X86ExecutionSnapshot last_single_step_snapshot;
    std::uint32_t single_step_trace_count = 0;
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
    std::uint32_t linexe_bridge_stack[12] = {};
    char linexe_bridge_argument_text[128] = {};
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
    std::string hle_console_output;
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
