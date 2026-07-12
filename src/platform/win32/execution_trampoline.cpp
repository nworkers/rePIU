#include "repiu/platform/win32/execution_trampoline.h"
#include "native_fast_path.h"
#include "repiu/platform/win32/live_telemetry.h"
#include "repiu/hle/linexe_call_gate.h"
#include "repiu/hle/glide_hle.h"
#include "repiu/platform/win32/glide_opengl_backend.h"
#include "repiu/platform/win32/cd_audio_wave_out.h"
#include "repiu/media/chd_cd_image.h"
#include "repiu/runtime/dos_low_memory.h"
#include "repiu/runtime/selector_table.h"

#include <Zydis.h>

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::platform::win32
{
namespace
{

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
};

struct DosInterruptVectorShadow
{
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
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
    bool aot_dynamic_translation_enabled = false;
    HANDLE aot_translation_thread = nullptr;
    HANDLE aot_translation_request_event = nullptr;
    HANDLE aot_translation_complete_event = nullptr;
    std::atomic<bool> aot_translation_shutdown{false};
    std::atomic<std::uint32_t> aot_translation_target{0};
    std::atomic<std::uint32_t> aot_worker_operation{
        static_cast<std::uint32_t>(AotWorkerOperation::kTranslate)};
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
    std::atomic<std::uint32_t> aot_reentry_count{0};
    std::atomic<std::uint32_t> aot_legacy_fallback_count{0};
    std::atomic<std::uint32_t> aot_last_fallback_address{0};
    std::atomic<std::uint32_t> aot_dynamic_attempt_count{0};
    std::atomic<std::uint32_t> aot_dynamic_success_count{0};
    std::atomic<std::uint32_t> aot_dynamic_added_bytes{0};
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
    repiu::hle::GlideLogicalState glide_state;
    GlideOpenGlBackend glide_backend;
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
    char hle_stdout_output[4096] = {};
    std::uint32_t hle_stdout_output_size = 0;
    char hle_stderr_output[4096] = {};
    std::uint32_t hle_stderr_output_size = 0;
    std::string hle_message;
};

class ExceptionDispatchScope
{
public:
    ExceptionDispatchScope(ThreadContext* context, std::uint32_t eip)
        : context_(context)
    {
        context_->exception_dispatch_last_eip.store(
            eip,
            std::memory_order_relaxed);
        context_->exception_dispatch_entry_count.fetch_add(
            1,
            std::memory_order_relaxed);
        context_->live_telemetry_phase.store(2, std::memory_order_relaxed);
        context_->live_telemetry_heartbeat.fetch_add(
            1,
            std::memory_order_relaxed);
        if (context_->shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context_->shared_live_telemetry->last_eip,
                static_cast<long>(eip));
            InterlockedIncrement(
                &context_->shared_live_telemetry->dispatch_entry_count);
            InterlockedIncrement(
                &context_->shared_live_telemetry->heartbeat);
        }
    }

    ~ExceptionDispatchScope()
    {
        context_->exception_dispatch_exit_count.fetch_add(
            1,
            std::memory_order_relaxed);
        context_->live_telemetry_phase.store(3, std::memory_order_relaxed);
        context_->live_telemetry_heartbeat.fetch_add(
            1,
            std::memory_order_relaxed);
        if (context_->shared_live_telemetry != nullptr)
        {
            InterlockedIncrement(
                &context_->shared_live_telemetry->dispatch_exit_count);
            InterlockedIncrement(
                &context_->shared_live_telemetry->heartbeat);
        }
    }

    ExceptionDispatchScope(const ExceptionDispatchScope&) = delete;
    ExceptionDispatchScope& operator=(const ExceptionDispatchScope&) = delete;

private:
    ThreadContext* context_;
};

bool IsDirectX86ExecutionSupported()
{
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    return true;
#else
    return false;
#endif
}

bool IsGuestStackSwitchSupported()
{
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
    return true;
#else
    return false;
#endif
}

#if defined(_WIN32)
// Guest execution is serialized to one worker per loader process. Keeping the
// VEH context outside Win32 TLS prevents a guest-modified FS selector from
// escaping into compiler-generated TLS access during host recovery.
ThreadContext* g_active_thread_context = nullptr;
std::uint32_t g_recovery_host_fs = 0;
std::uint32_t g_recovery_host_ds = 0;
std::uint32_t g_recovery_host_es = 0;
std::uint32_t g_recovery_host_gs = 0;

using CreateThreadFn = HANDLE(WINAPI*)(
    LPSECURITY_ATTRIBUTES,
    SIZE_T,
    LPTHREAD_START_ROUTINE,
    LPVOID,
    DWORD,
    LPDWORD);
using CloseHandleFn = BOOL(WINAPI*)(HANDLE);
using GetExitCodeThreadFn = BOOL(WINAPI*)(HANDLE, LPDWORD);
using GetLastErrorFn = DWORD(WINAPI*)();
using GetThreadContextFn = BOOL(WINAPI*)(HANDLE, LPCONTEXT);
using ResumeThreadFn = DWORD(WINAPI*)(HANDLE);
using SuspendThreadFn = DWORD(WINAPI*)(HANDLE);
using TerminateThreadFn = BOOL(WINAPI*)(HANDLE, DWORD);

struct Win32ThreadApi
{
    CreateThreadFn create_thread = nullptr;
    CloseHandleFn close_handle = nullptr;
    GetExitCodeThreadFn get_exit_code_thread = nullptr;
    GetLastErrorFn get_last_error = nullptr;
    GetThreadContextFn get_thread_context = nullptr;
    ResumeThreadFn resume_thread = nullptr;
    SuspendThreadFn suspend_thread = nullptr;
    TerminateThreadFn terminate_thread = nullptr;
};

template <typename FunctionType>
FunctionType ResolveKernel32Function(HMODULE kernel32, const char* name)
{
    return reinterpret_cast<FunctionType>(GetProcAddress(kernel32, name));
}

const Win32ThreadApi& GetWin32ThreadApi()
{
    static const Win32ThreadApi api = []
    {
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        Win32ThreadApi resolved;
        if (kernel32 == nullptr)
        {
            return resolved;
        }

        resolved.create_thread =
            ResolveKernel32Function<CreateThreadFn>(kernel32, "CreateThread");
        resolved.close_handle =
            ResolveKernel32Function<CloseHandleFn>(kernel32, "CloseHandle");
        resolved.get_exit_code_thread =
            ResolveKernel32Function<GetExitCodeThreadFn>(
                kernel32,
                "GetExitCodeThread");
        resolved.get_last_error =
            ResolveKernel32Function<GetLastErrorFn>(kernel32, "GetLastError");
        resolved.get_thread_context =
            ResolveKernel32Function<GetThreadContextFn>(
                kernel32,
                "GetThreadContext");
        resolved.resume_thread =
            ResolveKernel32Function<ResumeThreadFn>(kernel32, "ResumeThread");
        resolved.suspend_thread =
            ResolveKernel32Function<SuspendThreadFn>(
                kernel32,
                "SuspendThread");
        resolved.terminate_thread =
            ResolveKernel32Function<TerminateThreadFn>(
                kernel32,
                "TerminateThread");
        return resolved;
    }();

    return api;
}

struct SharedTelemetryMapping
{
    HANDLE mapping = nullptr;
    Win32SharedLiveTelemetry* telemetry = nullptr;

    SharedTelemetryMapping() = default;
    SharedTelemetryMapping(const SharedTelemetryMapping&) = delete;
    SharedTelemetryMapping& operator=(const SharedTelemetryMapping&) = delete;
    SharedTelemetryMapping(SharedTelemetryMapping&& other) noexcept
        : mapping(other.mapping), telemetry(other.telemetry)
    {
        other.mapping = nullptr;
        other.telemetry = nullptr;
    }

    ~SharedTelemetryMapping()
    {
        if (telemetry != nullptr)
        {
            UnmapViewOfFile(telemetry);
        }
        if (mapping != nullptr)
        {
            CloseHandle(mapping);
        }
    }
};

SharedTelemetryMapping OpenSharedTelemetryMapping()
{
    SharedTelemetryMapping result;
    char mapping_name[256] = {};
    if (GetEnvironmentVariableA(kWin32LiveTelemetryEnvironment,
                                mapping_name,
                                sizeof(mapping_name)) == 0)
    {
        return result;
    }
    result.mapping = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        mapping_name);
    if (result.mapping == nullptr)
    {
        return result;
    }
    result.telemetry = static_cast<Win32SharedLiveTelemetry*>(
        MapViewOfFile(result.mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (result.telemetry == nullptr ||
        result.telemetry->magic != kWin32LiveTelemetryMagic ||
        result.telemetry->version != kWin32LiveTelemetryVersion)
    {
        if (result.telemetry != nullptr)
        {
            UnmapViewOfFile(result.telemetry);
            result.telemetry = nullptr;
        }
        CloseHandle(result.mapping);
        result.mapping = nullptr;
    }
    return result;
}

void WriteLiveTelemetrySnapshot(const ThreadContext& context,
                                DWORD elapsed_milliseconds,
                                DWORD poll_iteration)
{
    char buffer[320] = {};
    const int length = std::snprintf(
        buffer,
        sizeof(buffer),
        "[repiu-live] elapsed_ms=%lu poll=%lu phase=%u heartbeat=%u "
        "dispatch_entry=%u dispatch_exit=%u last_eip=0x%08X "
        "progress=%u single_step=%u fast=%u/%u/%u "
        "reject=0x%08X:0x%08X/0x%02X bytes=%08X%08X\r\n",
        static_cast<unsigned long>(elapsed_milliseconds),
        static_cast<unsigned long>(poll_iteration),
        context.live_telemetry_phase.load(std::memory_order_relaxed),
        context.live_telemetry_heartbeat.load(std::memory_order_relaxed),
        context.exception_dispatch_entry_count.load(
            std::memory_order_relaxed),
        context.exception_dispatch_exit_count.load(
            std::memory_order_relaxed),
        context.exception_dispatch_last_eip.load(std::memory_order_relaxed),
        context.diagnostic_progress_count.load(std::memory_order_relaxed),
        context.single_step_trace_count.load(std::memory_order_relaxed),
        context.native_fast_path.entry_count.load(std::memory_order_relaxed),
        context.native_fast_path.return_count.load(std::memory_order_relaxed),
        context.native_fast_path.cancel_count.load(std::memory_order_relaxed),
        context.native_fast_path.last_rejected_candidate.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_instruction.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_opcode.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_bytes_high.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_bytes_low.load(
            std::memory_order_relaxed));
    if (length <= 0)
    {
        return;
    }

    HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle == nullptr || stderr_handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    const DWORD byte_count = static_cast<DWORD>(
        length < static_cast<int>(sizeof(buffer)) ? length
                                                  : sizeof(buffer) - 1U);
    WriteFile(stderr_handle, buffer, byte_count, &written, nullptr);
}

DWORD PollThreadUntilExit(HANDLE thread,
                          DWORD timeout_milliseconds,
                          ThreadContext* progress_context,
                          DWORD* exit_code)
{
    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.get_exit_code_thread == nullptr)
    {
        return WAIT_FAILED;
    }

    const DWORD quiet_timeout_milliseconds = 1000U;
    const DWORD start_tick = GetTickCount();
    DWORD quiet_start_tick = start_tick;
    std::uint32_t last_progress_count = 0;
    std::uint32_t last_single_step_count = 0;
    std::uint32_t last_aot_progress_count = 0;
    if (progress_context != nullptr)
    {
        last_progress_count =
            progress_context->diagnostic_progress_count.load(
                std::memory_order_relaxed);
        last_single_step_count =
            progress_context->single_step_trace_count.load(
                std::memory_order_relaxed);
        last_aot_progress_count =
            progress_context->aot_boundary_count.load(
                std::memory_order_relaxed) +
            progress_context->aot_reentry_count.load(
                std::memory_order_relaxed);
    }

    DWORD quiet_iterations = 0;
    DWORD last_live_snapshot_tick = start_tick;
    if (progress_context != nullptr)
    {
        progress_context->live_telemetry_phase.store(
            1,
            std::memory_order_relaxed);
        WriteLiveTelemetrySnapshot(*progress_context, 0, 0);
    }
    for (DWORD iteration = 0;; ++iteration)
    {
        if (progress_context != nullptr)
        {
            progress_context->diagnostic_poll_iteration_count =
                iteration + 1;
        }
        DWORD current_exit_code = 0;
        if (!api.get_exit_code_thread(thread, &current_exit_code))
        {
            return WAIT_FAILED;
        }

        if (current_exit_code != STILL_ACTIVE)
        {
            if (exit_code != nullptr)
            {
                *exit_code = current_exit_code;
            }
            return WAIT_OBJECT_0;
        }

        bool progressed = false;
        if (progress_context != nullptr)
        {
            const std::uint32_t progress_count =
                progress_context->diagnostic_progress_count.load(
                    std::memory_order_relaxed);
            const std::uint32_t single_step_count =
                progress_context->single_step_trace_count.load(
                    std::memory_order_relaxed);
            const std::uint32_t aot_progress_count =
                progress_context->aot_boundary_count.load(
                    std::memory_order_relaxed) +
                progress_context->aot_reentry_count.load(
                    std::memory_order_relaxed);
            progressed =
                progress_count != last_progress_count ||
                single_step_count != last_single_step_count ||
                aot_progress_count != last_aot_progress_count;
            last_progress_count = progress_count;
            last_single_step_count = single_step_count;
            last_aot_progress_count = aot_progress_count;
        }
        if (progressed)
        {
            quiet_iterations = 0;
            quiet_start_tick = GetTickCount();
        }
        else
        {
            ++quiet_iterations;
        }
        if (progress_context != nullptr)
        {
            progress_context->diagnostic_quiet_iteration_count =
                quiet_iterations;
        }

        if (timeout_milliseconds != INFINITE &&
            GetTickCount() - quiet_start_tick >=
            quiet_timeout_milliseconds)
        {
            if (progress_context != nullptr)
            {
                WriteLiveTelemetrySnapshot(
                    *progress_context,
                    GetTickCount() - start_tick,
                    iteration + 1);
            }
            return WAIT_TIMEOUT;
        }

        if (timeout_milliseconds != INFINITE &&
            GetTickCount() - start_tick >= timeout_milliseconds)
        {
            if (progress_context != nullptr)
            {
                WriteLiveTelemetrySnapshot(
                    *progress_context,
                    GetTickCount() - start_tick,
                    iteration + 1);
            }
            return WAIT_TIMEOUT;
        }

        const DWORD current_tick = GetTickCount();
        if (progress_context != nullptr &&
            current_tick - last_live_snapshot_tick >= 1000U)
        {
            WriteLiveTelemetrySnapshot(*progress_context,
                                       current_tick - start_tick,
                                       iteration + 1);
            last_live_snapshot_tick = current_tick;
        }
        Sleep(1);
    }
}

void CopySnapshotFromContextRecord(const CONTEXT& source,
                                   X86ExecutionSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return;
    }

#if defined(_M_IX86)
    snapshot->captured = true;
    snapshot->eip = source.Eip;
    snapshot->eax = source.Eax;
    snapshot->ebx = source.Ebx;
    snapshot->ecx = source.Ecx;
    snapshot->edx = source.Edx;
    snapshot->esi = source.Esi;
    snapshot->edi = source.Edi;
    snapshot->esp = source.Esp;
    snapshot->ebp = source.Ebp;
    snapshot->eflags = source.EFlags;
    snapshot->cs = static_cast<std::uint16_t>(source.SegCs);
    snapshot->ds = static_cast<std::uint16_t>(source.SegDs);
    snapshot->es = static_cast<std::uint16_t>(source.SegEs);
    snapshot->ss = static_cast<std::uint16_t>(source.SegSs);
    snapshot->fs = static_cast<std::uint16_t>(source.SegFs);
    snapshot->gs = static_cast<std::uint16_t>(source.SegGs);
#else
    (void)source;
    snapshot->captured = false;
#endif
}

void CaptureSuspendedThreadSnapshot(HANDLE thread,
                                    X86ExecutionSnapshot* snapshot)
{
    if (thread == nullptr || snapshot == nullptr)
    {
        return;
    }

#if defined(_M_IX86)
    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.suspend_thread == nullptr ||
        api.get_thread_context == nullptr ||
        api.resume_thread == nullptr)
    {
        return;
    }

    if (api.suspend_thread(thread) == static_cast<DWORD>(-1))
    {
        return;
    }

    CONTEXT thread_context = {};
    thread_context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
    if (api.get_thread_context(thread, &thread_context))
    {
        CopySnapshotFromContextRecord(thread_context, snapshot);
    }
    api.resume_thread(thread);
#else
    (void)thread;
    snapshot->captured = false;
#endif
}

std::vector<std::uint8_t> BuildDosEnvironmentBlock()
{
    std::vector<std::uint8_t> block;

#if defined(_WIN32)
    LPCH environment = GetEnvironmentStringsA();
    if (environment != nullptr)
    {
        const char* cursor = environment;
        while (*cursor != '\0')
        {
            const char* entry_begin = cursor;
            while (*cursor != '\0')
            {
                ++cursor;
            }

            bool before_equals = true;
            for (const char* current = entry_begin; current != cursor;
                 ++current)
            {
                unsigned char byte = static_cast<unsigned char>(*current);
                if (before_equals && byte == '=')
                {
                    before_equals = false;
                }
                else if (before_equals)
                {
                    byte = static_cast<unsigned char>(
                        std::toupper(static_cast<unsigned char>(byte)));
                }
                block.push_back(static_cast<std::uint8_t>(byte));
            }
            block.push_back(0);
            ++cursor;
        }
        FreeEnvironmentStringsA(environment);
    }
#endif

    if (block.empty() || block.back() != 0)
    {
        block.push_back(0);
    }
    block.push_back(0);
    return block;
}

bool BuildSingleStepSnapshot(const ThreadContext& context,
                             X86ExecutionSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return false;
    }

    const std::uint32_t count =
        context.single_step_trace_count.load(std::memory_order_relaxed);
    if (count == 0)
    {
        *snapshot = X86ExecutionSnapshot{};
        return false;
    }

    snapshot->captured = true;
    snapshot->eip =
        context.single_step_eip.load(std::memory_order_relaxed);
    snapshot->eax =
        context.single_step_eax.load(std::memory_order_relaxed);
    snapshot->ebx =
        context.single_step_ebx.load(std::memory_order_relaxed);
    snapshot->ecx =
        context.single_step_ecx.load(std::memory_order_relaxed);
    snapshot->edx =
        context.single_step_edx.load(std::memory_order_relaxed);
    snapshot->esi =
        context.single_step_esi.load(std::memory_order_relaxed);
    snapshot->edi =
        context.single_step_edi.load(std::memory_order_relaxed);
    snapshot->esp =
        context.single_step_esp.load(std::memory_order_relaxed);
    snapshot->ebp =
        context.single_step_ebp.load(std::memory_order_relaxed);
    snapshot->eflags =
        context.single_step_eflags.load(std::memory_order_relaxed);
    snapshot->cs = static_cast<std::uint16_t>(
        context.single_step_cs.load(std::memory_order_relaxed));
    snapshot->ds = static_cast<std::uint16_t>(
        context.single_step_ds.load(std::memory_order_relaxed));
    snapshot->es = static_cast<std::uint16_t>(
        context.single_step_es.load(std::memory_order_relaxed));
    snapshot->ss = static_cast<std::uint16_t>(
        context.single_step_ss.load(std::memory_order_relaxed));
    snapshot->fs = static_cast<std::uint16_t>(
        context.single_step_fs.load(std::memory_order_relaxed));
    snapshot->gs = static_cast<std::uint16_t>(
        context.single_step_gs.load(std::memory_order_relaxed));
    return true;
}

void CopyThreadObservationToAttempt(const ThreadContext& context,
                                    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return;
    }

    attempt->dos_termination_captured = context.dos_termination_captured;
    attempt->dos_termination_ax = context.dos_termination_ax;
    attempt->dos_termination_eip = context.dos_termination_eip;
    attempt->dos_termination_esp = context.dos_termination_esp;
    std::memcpy(attempt->dos_termination_stack,
                context.dos_termination_stack,
                sizeof(attempt->dos_termination_stack));

    attempt->single_step_trace_count =
        context.single_step_trace_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_entry_count =
        context.native_fast_path.entry_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_return_count =
        context.native_fast_path.return_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_cancel_count =
        context.native_fast_path.cancel_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_last_entry =
        context.native_fast_path.last_entry;
    attempt->native_fast_path_last_return =
        context.native_fast_path.last_return;
    attempt->aot_backend_active = context.aot_placement != nullptr;
    attempt->aot_cache_entry_count = context.aot_cache_entry_count.load(
        std::memory_order_relaxed);
    attempt->aot_boundary_count = context.aot_boundary_count.load(
        std::memory_order_relaxed);
    attempt->aot_reentry_count = context.aot_reentry_count.load(
        std::memory_order_relaxed);
    attempt->aot_legacy_fallback_count =
        context.aot_legacy_fallback_count.load(std::memory_order_relaxed);
    attempt->aot_last_fallback_address =
        context.aot_last_fallback_address.load(std::memory_order_relaxed);
    attempt->aot_dynamic_attempt_count =
        context.aot_dynamic_attempt_count.load(std::memory_order_relaxed);
    attempt->aot_dynamic_success_count =
        context.aot_dynamic_success_count.load(std::memory_order_relaxed);
    attempt->aot_dynamic_added_bytes =
        context.aot_dynamic_added_bytes.load(std::memory_order_relaxed);
    attempt->aot_indirect_dispatch_count =
        context.aot_indirect_dispatch_count.load(std::memory_order_relaxed);
    attempt->aot_inline_cache_patch_attempt_count =
        context.aot_inline_cache_patch_attempt_count.load(
            std::memory_order_relaxed);
    attempt->aot_inline_cache_patch_success_count =
        context.aot_inline_cache_patch_success_count.load(
            std::memory_order_relaxed);
    attempt->aot_inline_cache_site_count = context.aot_placement != nullptr
        ? static_cast<std::uint32_t>(
              context.aot_placement->indirect_inline_cache_sites.size())
        : 0U;
    attempt->aot_last_reentry_cache_address =
        context.aot_reentry_cache_address;
    attempt->aot_code_write_count =
        context.aot_code_write_count.load(std::memory_order_relaxed);
    attempt->aot_page_retire_attempt_count =
        context.aot_page_retire_attempt_count.load(std::memory_order_relaxed);
    attempt->aot_page_retire_success_count =
        context.aot_page_retire_success_count.load(std::memory_order_relaxed);
    attempt->aot_generation_publish_count =
        context.aot_generation_publish_count.load(std::memory_order_relaxed);
    attempt->aot_generation_failure_count =
        context.aot_generation_failure_count.load(std::memory_order_relaxed);
    attempt->aot_generation_relinked_entry_count =
        context.aot_generation_relinked_entry_count.load(
            std::memory_order_relaxed);
    attempt->aot_retired_entry_trap_count =
        context.aot_retired_entry_trap_count.load(std::memory_order_relaxed);
    attempt->aot_quarantine_count =
        context.aot_quarantine_count.load(std::memory_order_relaxed);
    attempt->aot_last_code_write_source =
        context.aot_last_code_write_source.load(std::memory_order_relaxed);
    attempt->aot_last_code_write_destination =
        context.aot_last_code_write_destination.load(
            std::memory_order_relaxed);
    attempt->aot_last_retired_page =
        context.aot_last_retired_page.load(std::memory_order_relaxed);
    attempt->aot_last_published_generation =
        context.aot_last_published_generation.load(
            std::memory_order_relaxed);
    attempt->aot_exception_mapping_valid =
        context.aot_exception_mapping_valid;
    attempt->aot_exception_cache_address =
        context.aot_exception_cache_address;
    attempt->aot_exception_guest_address =
        context.aot_exception_guest_address;
    std::memcpy(attempt->aot_exception_cache_bytes,
                context.aot_exception_cache_bytes,
                sizeof(attempt->aot_exception_cache_bytes));
    std::memcpy(attempt->aot_exception_guest_bytes,
                context.aot_exception_guest_bytes,
                sizeof(attempt->aot_exception_guest_bytes));
    attempt->aot_last_indirect_source =
        context.aot_last_indirect_source.load(std::memory_order_relaxed);
    attempt->aot_last_indirect_target =
        context.aot_last_indirect_target.load(std::memory_order_relaxed);
    attempt->aot_return_dispatch_count =
        context.aot_return_dispatch_count.load(std::memory_order_relaxed);
    attempt->aot_last_return_target =
        context.aot_last_return_target.load(std::memory_order_relaxed);
    attempt->aot_last_return_source =
        context.aot_last_return_source.load(std::memory_order_relaxed);
    std::memcpy(attempt->aot_last_return_stack,
                context.aot_last_return_stack,
                sizeof(attempt->aot_last_return_stack));
    attempt->execution_probe_configured = context.execution_probe_configured;
    attempt->execution_probe_hit = context.execution_probe_hit;
    attempt->execution_probe_offset = context.execution_probe_offset;
    attempt->execution_probe_snapshot = context.execution_probe_snapshot;
    std::memcpy(attempt->execution_probe_stack,
                context.execution_probe_stack,
                sizeof(attempt->execution_probe_stack));
    attempt->aot_call_depth = context.aot_call_depth;
    attempt->aot_last_return_matches_call =
        context.aot_last_return_matches_call;
    attempt->aot_last_expected_return = context.aot_last_expected_return;
    attempt->aot_last_call_source = context.aot_last_call_source;
    attempt->aot_last_call_target = context.aot_last_call_target;
    attempt->aot_last_expected_call_source =
        context.aot_last_expected_call_source;
    attempt->aot_last_expected_call_target =
        context.aot_last_expected_call_target;
    attempt->aot_return_trace_count = context.aot_return_trace_count;
    std::memcpy(attempt->aot_return_trace, context.aot_return_trace,
                sizeof(attempt->aot_return_trace));
    attempt->aot_transfer_trace_count = context.aot_transfer_trace_count;
    std::memcpy(attempt->aot_transfer_trace, context.aot_transfer_trace,
                sizeof(attempt->aot_transfer_trace));
    attempt->diagnostic_poll_iteration_count =
        context.diagnostic_poll_iteration_count;
    attempt->diagnostic_progress_count =
        context.diagnostic_progress_count.load(std::memory_order_relaxed);
    attempt->diagnostic_quiet_iteration_count =
        context.diagnostic_quiet_iteration_count;
    attempt->exception_dispatch_entry_count =
        context.exception_dispatch_entry_count.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_exit_count =
        context.exception_dispatch_exit_count.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_last_eip =
        context.exception_dispatch_last_eip.load(
            std::memory_order_relaxed);
    attempt->selector_table_valid = context.selector_table.valid;
    attempt->selector_descriptor_count =
        static_cast<std::uint32_t>(
            context.selector_table.descriptors.size());
    attempt->linexe_environment_active = context.linexe_environment_active;
    attempt->linexe_gs_byte_load_count = context.linexe_gs_byte_load_count;
    attempt->linexe_first_gs_byte_offset = context.linexe_first_gs_byte_offset;
    attempt->linexe_first_gs_byte_value = context.linexe_first_gs_byte_value;
    const repiu::runtime::GuestDescriptor* client_descriptor =
        repiu::runtime::FindDescriptor(context.selector_table,
                                       kDos4gwClientDataSelector);
    if (client_descriptor != nullptr && client_descriptor->present)
    {
        attempt->linexe_client_descriptor_valid = true;
        attempt->linexe_client_descriptor_base = client_descriptor->base;
        attempt->linexe_client_descriptor_limit = client_descriptor->limit;
        if (client_descriptor->limit >= kDos4gwPrivateRootOffset + 3U &&
            static_cast<std::uint64_t>(client_descriptor->base) +
                    kDos4gwPrivateRootOffset + 4U <=
                static_cast<std::uint64_t>(context.runtime_base) +
                    context.runtime_size)
        {
            const auto* root = reinterpret_cast<const std::uint16_t*>(
                static_cast<std::uintptr_t>(client_descriptor->base +
                                            kDos4gwPrivateRootOffset));
            attempt->linexe_root_offset = root[0];
            attempt->linexe_root_selector = root[1];
        }
    }
    const repiu::runtime::GuestDescriptor* data_descriptor =
        repiu::runtime::FindDescriptor(context.selector_table,
                                       kDos4gwLinexeDataSelector);
    if (data_descriptor != nullptr && data_descriptor->present)
    {
        attempt->linexe_data_descriptor_valid = true;
        attempt->linexe_data_descriptor_base = data_descriptor->base;
        const auto* module = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(data_descriptor->base +
                                        kDos4gwLinexeLoaderOffset));
        attempt->linexe_module_name_offset = module[2];
        attempt->linexe_module_name_selector = module[3];
        attempt->linexe_direct_export_count = module[8];
        attempt->linexe_direct_export_table_offset = module[9];
        attempt->linexe_direct_export_table_selector = module[10];
        const auto* first_export = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(data_descriptor->base + module[9]));
        attempt->linexe_direct_first_export_name_offset = first_export[0];
        attempt->linexe_direct_first_export_name_selector = first_export[1];
        const char* name = reinterpret_cast<const char*>(
            static_cast<std::uintptr_t>(data_descriptor->base + module[2]));
        for (std::uint32_t index = 0; index < 64 && name[index] != '\0'; ++index)
        {
            attempt->linexe_direct_module_name.push_back(name[index]);
        }
    }
    attempt->linexe_scan_entry_count = context.linexe_scan_entry_count;
    attempt->linexe_module_candidate_count =
        context.linexe_module_candidate_count;
    attempt->linexe_module_match_count = context.linexe_module_match_count;
    attempt->linexe_name_pointer_valid_count =
        context.linexe_name_pointer_valid_count;
    attempt->linexe_name_byte_instruction_count =
        context.linexe_name_byte_instruction_count;
    attempt->linexe_data_gs_load_count = context.linexe_data_gs_load_count;
    attempt->linexe_module_selector_stack_value =
        context.linexe_module_selector_stack_value;
    attempt->linexe_module_offset_stack_value =
        context.linexe_module_offset_stack_value;
    attempt->linexe_export_offset_stack_value =
        context.linexe_export_offset_stack_value;
    attempt->linexe_export_selector_stack_value =
        context.linexe_export_selector_stack_value;
    attempt->linexe_export_jump_source_esp =
        context.linexe_export_jump_source_esp;
    attempt->linexe_export_jump_source_module_offset =
        context.linexe_export_jump_source_module_offset;
    attempt->linexe_export_jump_source_module_selector =
        context.linexe_export_jump_source_module_selector;
    attempt->linexe_export_jump_target_esp =
        context.linexe_export_jump_target_esp;
    attempt->linexe_export_jump_target_module_offset =
        context.linexe_export_jump_target_module_offset;
    attempt->linexe_export_jump_target_module_selector =
        context.linexe_export_jump_target_module_selector;
    attempt->linexe_export_name_compare_count =
        context.linexe_export_name_compare_count;
    attempt->linexe_export_name_compare_gs =
        context.linexe_export_name_compare_gs;
    attempt->linexe_export_name_compare_edi =
        context.linexe_export_name_compare_edi;
    attempt->linexe_export_name_compare_esi =
        context.linexe_export_name_compare_esi;
    attempt->linexe_export_name_actual_byte =
        context.linexe_export_name_actual_byte;
    attempt->linexe_export_name_expected_byte =
        context.linexe_export_name_expected_byte;
    attempt->linexe_export_name_stage_mask =
        context.linexe_export_name_stage_mask;
    attempt->linexe_export_entry_name_offset_value =
        context.linexe_export_entry_name_offset_value;
    attempt->linexe_export_entry_name_selector_value =
        context.linexe_export_entry_name_selector_value;
    attempt->linexe_export_result_store_destination =
        context.linexe_export_result_store_destination;
    attempt->linexe_export_result_store_value =
        context.linexe_export_result_store_value;
    attempt->linexe_export_result_store_count =
        context.linexe_export_result_store_count;
    attempt->linexe_export_value_load_selector =
        context.linexe_export_value_load_selector;
    attempt->linexe_export_value_load_offset =
        context.linexe_export_value_load_offset;
    attempt->linexe_export_value_load_value =
        context.linexe_export_value_load_value;
    attempt->linexe_root_selector_eax = context.linexe_root_selector_eax;
    attempt->linexe_root_read_gs = context.linexe_root_read_gs;
    attempt->linexe_shared_load_entry_count =
        context.linexe_shared_load_entry_count;
    attempt->linexe_shared_load_read_count =
        context.linexe_shared_load_read_count;
    attempt->linexe_shared_load_selector = context.linexe_shared_load_selector;
    attempt->linexe_shared_load_offset = context.linexe_shared_load_offset;
    attempt->linexe_shared_load_value = context.linexe_shared_load_value;
    attempt->linexe_root_offset_load_value =
        context.linexe_root_offset_load_value;
    attempt->linexe_root_selector_load_value =
        context.linexe_root_selector_load_value;
    attempt->linexe_root_offset_load_success =
        context.linexe_root_offset_load_success;
    attempt->linexe_root_selector_load_success =
        context.linexe_root_selector_load_success;
    attempt->linexe_export_match_count = context.linexe_export_match_count;
    attempt->linexe_export_entry_loop_count =
        context.linexe_export_entry_loop_count;
    attempt->linexe_export_compare_count = context.linexe_export_compare_count;
    attempt->linexe_export_compare_eax = context.linexe_export_compare_eax;
    attempt->linexe_export_compare_ecx = context.linexe_export_compare_ecx;
    attempt->linexe_export_compare_eflags =
        context.linexe_export_compare_eflags;
    attempt->linexe_export_count_load_edx =
        context.linexe_export_count_load_edx;
    attempt->linexe_export_count_load_gs =
        context.linexe_export_count_load_gs;
    attempt->linexe_scan_return_count = context.linexe_scan_return_count;
    attempt->linexe_bridge_entry_count = context.linexe_bridge_entry_count;
    attempt->linexe_bridge_gate_valid = context.linexe_bridge_gate_valid;
    attempt->linexe_bridge_selector = context.linexe_bridge_selector;
    attempt->linexe_bridge_offset = context.linexe_bridge_offset;
    attempt->linexe_bridge_service = context.linexe_bridge_service;
    attempt->linexe_bridge_esp = context.linexe_bridge_esp;
    attempt->linexe_bridge_ebp = context.linexe_bridge_ebp;
    std::memcpy(attempt->linexe_bridge_stack,
                context.linexe_bridge_stack,
                sizeof(attempt->linexe_bridge_stack));
    std::memcpy(attempt->linexe_bridge_argument_text,
                context.linexe_bridge_argument_text,
                sizeof(attempt->linexe_bridge_argument_text));
    std::memcpy(attempt->linexe_bridge_stack_text,
                context.linexe_bridge_stack_text,
                sizeof(attempt->linexe_bridge_stack_text));
    attempt->linexe_virtual_module_load_count =
        context.linexe_virtual_module_load_count;
    attempt->linexe_virtual_module_handle =
        context.linexe_virtual_module_handle;
    attempt->linexe_get_proc_count = context.linexe_get_proc_count;
    attempt->linexe_get_proc_result_pointer =
        context.linexe_get_proc_result_pointer;
    std::memcpy(attempt->linexe_get_proc_name,
                context.linexe_get_proc_name,
                sizeof(attempt->linexe_get_proc_name));
    attempt->glide_gate_entry_count = context.glide_gate_entry_count;
    attempt->glide_gate_handled_count = context.glide_gate_handled_count;
    attempt->glide_gate_esp = context.glide_gate_esp;
    std::memcpy(attempt->glide_gate_stack,
                context.glide_gate_stack,
                sizeof(attempt->glide_gate_stack));
    attempt->glide_gate_ordinal = context.glide_gate_ordinal;
    attempt->glide_gate_argument_bytes = context.glide_gate_argument_bytes;
    std::memcpy(attempt->glide_gate_name,
                context.glide_gate_name,
                sizeof(attempt->glide_gate_name));
    for (std::size_t ordinal = 0;
         ordinal < context.glide_call_counts.size(); ++ordinal)
    {
        if (context.glide_call_counts[ordinal] == 0U)
        {
            continue;
        }
        Win32MinimalExecutionAttempt::GlideCallObservation observation;
        observation.ordinal = static_cast<std::uint16_t>(ordinal);
        observation.count = context.glide_call_counts[ordinal];
        observation.name = context.glide_call_names[ordinal];
        std::copy(context.glide_first_stacks[ordinal].begin(),
                  context.glide_first_stacks[ordinal].end(),
                  observation.first_stack);
        attempt->glide_calls.push_back(std::move(observation));
    }
    attempt->mscdex_available = context.mscdex_available;
    attempt->cd_audio_available = context.cd_audio_available;
    attempt->mscdex_track_count = static_cast<std::uint32_t>(
        context.cd_image.tracks().size());
    attempt->mscdex_request_count = context.mscdex_request_count;
    attempt->cd_audio_current_lba = context.cd_audio.current_lba();
    attempt->glide_window_open_count = context.glide_window_open_count;
    attempt->glide_logical_width = context.glide_logical_width;
    attempt->glide_logical_height = context.glide_logical_height;
    attempt->glide_backend_message = context.glide_backend_message;
    attempt->glide_texture_memory_bytes =
        context.glide_state.texture_memory_bytes;
    repiu::hle::CalculateGlideTextureMaxAddress(
        context.glide_state.texture_memory_bytes,
        &attempt->glide_texture_max_address);
    attempt->linexe_scan_return_eax = context.linexe_scan_return_eax;
    attempt->linexe_scan_return_ebp = context.linexe_scan_return_ebp;
    attempt->linexe_scan_caller_eax = context.linexe_scan_caller_eax;
    std::memcpy(attempt->linexe_selector_init_results,
                context.linexe_selector_init_results,
                sizeof(attempt->linexe_selector_init_results));
    attempt->dpmi_allocate_call_count = context.dpmi_allocate_call_count;
    attempt->dpmi_last_allocate_requested_count =
        context.dpmi_last_allocate_requested_count;
    attempt->dpmi_last_allocated_selector =
        context.dpmi_last_allocated_selector;
    constexpr std::uint32_t kSelectorWordsOffset = 0x000C68C0U;
    constexpr std::uint32_t kResolvedExportsOffset = 0x001A62C4U;
    constexpr std::uint32_t kSavedClientGsOffset = 0x001A6354U;
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context.runtime_base) +
        context.runtime_size;
    if (context.linexe_environment_active &&
        static_cast<std::uint64_t>(context.runtime_base) +
                kResolvedExportsOffset + 8U * 8U <= runtime_end)
    {
        const auto* saved_gs = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(context.runtime_base +
                                        kSavedClientGsOffset));
        attempt->linexe_saved_client_gs = *saved_gs;
        const auto* selector_words = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(context.runtime_base +
                                        kSelectorWordsOffset));
        std::memcpy(attempt->linexe_selector_words,
                    selector_words,
                    sizeof(attempt->linexe_selector_words));
        for (std::uint32_t index = 0; index < 8; ++index)
        {
            const auto* value = reinterpret_cast<const std::uint32_t*>(
                static_cast<std::uintptr_t>(
                    context.runtime_base + kResolvedExportsOffset +
                    index * 8U));
            attempt->linexe_resolved_exports[index] = *value;
            if (*value != 0)
            {
                ++attempt->linexe_resolved_export_count;
            }
        }
    }
    attempt->dos_low_memory_valid = context.dos_low_memory.valid;
    attempt->dos_low_memory_size =
        repiu::runtime::kDosLowMemorySize;
    BuildSingleStepSnapshot(context, &attempt->last_single_step_snapshot);
    attempt->dos_environment_block_size =
        static_cast<std::uint32_t>(context.dos_environment_block.size());
    attempt->last_dos_environment_access_valid =
        context.last_dos_environment_access_valid;
    attempt->last_dos_environment_access_offset =
        context.last_dos_environment_access_offset;
    attempt->last_dos_environment_entry_offset =
        context.last_dos_environment_entry_offset;
    attempt->last_dos_environment_value_length =
        context.last_dos_environment_value_length;
    attempt->last_dos_environment_entry_name =
        context.last_dos_environment_entry_name;
    attempt->handled_hle_trap_count = context.handled_hle_trap_count;
    attempt->last_hle_trap_address = context.last_hle_trap_address;
    attempt->last_hle_trap_opcode = context.last_hle_trap_opcode;
    attempt->port_io = context.port_io;
    attempt->dos_path = context.dos_path;
    attempt->dos_file_io = context.dos_file_io;
    attempt->allocator_probe = context.allocator_probe;
    attempt->allocator_control_flow = context.allocator_control_flow;
    attempt->handled_dos_interrupt_count =
        context.handled_dos_interrupt_count;
    attempt->last_dos_interrupt_vector = context.last_dos_interrupt_vector;
    attempt->last_dos_interrupt_ah = context.last_dos_interrupt_ah;
    attempt->last_dos_interrupt_ax = context.last_dos_interrupt_ax;
    attempt->handled_dos_chdir_count = context.handled_dos_chdir_count;
    attempt->last_dos_chdir_guest_path =
        context.last_dos_chdir_guest_path;
    attempt->last_dos_chdir_host_path = context.last_dos_chdir_host_path;
    attempt->last_dos_chdir_virtual_path =
        context.last_dos_chdir_virtual_path;
    attempt->last_dos_chdir_success = context.last_dos_chdir_success;
    attempt->last_dos_chdir_error = context.last_dos_chdir_error;
    attempt->handled_dos_getcwd_count = context.handled_dos_getcwd_count;
    attempt->last_dos_getcwd_drive = context.last_dos_getcwd_drive;
    attempt->last_dos_getcwd_path = context.last_dos_getcwd_path;
    attempt->last_dos_getcwd_success = context.last_dos_getcwd_success;
    attempt->last_dos_getcwd_error = context.last_dos_getcwd_error;
    attempt->handled_dos_getdrive_count =
        context.handled_dos_getdrive_count;
    attempt->last_dos_getdrive_value = context.last_dos_getdrive_value;
    attempt->handled_dos_open_count = context.handled_dos_open_count;
    attempt->last_dos_open_guest_path = context.last_dos_open_guest_path;
    attempt->last_dos_open_host_path = context.last_dos_open_host_path;
    attempt->last_dos_open_virtual_path =
        context.last_dos_open_virtual_path;
    attempt->last_dos_open_success = context.last_dos_open_success;
    attempt->last_dos_open_error = context.last_dos_open_error;
    attempt->last_dos_open_handle = context.last_dos_open_handle;
    attempt->last_dos_open_access_mode = context.last_dos_open_access_mode;
    attempt->handled_dos_read_count = context.handled_dos_read_count;
    attempt->last_dos_read_handle = context.last_dos_read_handle;
    attempt->last_dos_read_requested_bytes =
        context.last_dos_read_requested_bytes;
    attempt->last_dos_read_actual_bytes =
        context.last_dos_read_actual_bytes;
    attempt->last_dos_read_buffer = context.last_dos_read_buffer;
    attempt->last_dos_read_success = context.last_dos_read_success;
    attempt->last_dos_read_error = context.last_dos_read_error;
    attempt->handled_dos_seek_count = context.handled_dos_seek_count;
    attempt->last_dos_seek_handle = context.last_dos_seek_handle;
    attempt->last_dos_seek_origin = context.last_dos_seek_origin;
    attempt->last_dos_seek_offset = context.last_dos_seek_offset;
    attempt->last_dos_seek_position = context.last_dos_seek_position;
    attempt->last_dos_seek_success = context.last_dos_seek_success;
    attempt->last_dos_seek_error = context.last_dos_seek_error;
    attempt->handled_dos_close_count = context.handled_dos_close_count;
    attempt->last_dos_close_handle = context.last_dos_close_handle;
    attempt->last_dos_close_success = context.last_dos_close_success;
    attempt->last_dos_close_error = context.last_dos_close_error;
    attempt->handled_dos_ioctl_count = context.handled_dos_ioctl_count;
    attempt->last_dos_ioctl_subfunction =
        context.last_dos_ioctl_subfunction;
    attempt->last_dos_ioctl_handle = context.last_dos_ioctl_handle;
    attempt->last_dos_ioctl_success = context.last_dos_ioctl_success;
    attempt->last_dos_ioctl_error = context.last_dos_ioctl_error;
    attempt->last_dos_ioctl_device_info =
        context.last_dos_ioctl_device_info;
    attempt->handled_dos_resize_count = context.handled_dos_resize_count;
    attempt->last_dos_resize_selector = context.last_dos_resize_selector;
    attempt->last_dos_resize_paragraphs =
        context.last_dos_resize_paragraphs;
    attempt->last_dos_resize_success = context.last_dos_resize_success;
    attempt->last_dos_resize_error = context.last_dos_resize_error;
    attempt->handled_segment_load_count =
        context.handled_segment_load_count;
    attempt->last_segment_load_address = context.last_segment_load_address;
    attempt->last_segment_load_opcode = context.last_segment_load_opcode;
    attempt->last_segment_load_register =
        context.last_segment_load_register;
    attempt->last_segment_load_selector =
        context.last_segment_load_selector;
    attempt->last_segment_load_source = context.last_segment_load_source;
    attempt->segment_load = context.segment_load;
    attempt->handled_segment_store_count =
        context.handled_segment_store_count;
    attempt->last_segment_store_address = context.last_segment_store_address;
    attempt->last_segment_store_opcode = context.last_segment_store_opcode;
    attempt->last_segment_store_register =
        context.last_segment_store_register;
    attempt->last_segment_store_selector =
        context.last_segment_store_selector;
    attempt->last_segment_store_destination =
        context.last_segment_store_destination;
    attempt->handled_segment_memory_load_count =
        context.handled_segment_memory_load_count;
    attempt->last_segment_memory_load_address =
        context.last_segment_memory_load_address;
    attempt->last_segment_memory_load_opcode =
        context.last_segment_memory_load_opcode;
    attempt->last_segment_memory_load_register =
        context.last_segment_memory_load_register;
    attempt->last_segment_memory_load_selector =
        context.last_segment_memory_load_selector;
    attempt->last_segment_memory_load_offset =
        context.last_segment_memory_load_offset;
    attempt->last_segment_memory_load_width =
        context.last_segment_memory_load_width;
    attempt->last_segment_memory_load_value =
        context.last_segment_memory_load_value;
    attempt->handled_low_memory_access_count =
        context.handled_low_memory_access_count;
    attempt->last_low_memory_access_address =
        context.last_low_memory_access_address;
    attempt->last_low_memory_access_opcode =
        context.last_low_memory_access_opcode;
    attempt->last_low_memory_access_esi =
        context.last_low_memory_access_esi;
    attempt->last_low_memory_access_edi =
        context.last_low_memory_access_edi;
    attempt->last_low_memory_access_destination =
        context.last_low_memory_access_destination;
    attempt->last_low_memory_access_value =
        context.last_low_memory_access_value;
    attempt->rep_movs_copy_failure_count =
        context.rep_movs_copy_failure_count;
    attempt->last_rep_movs_copy_failure_stage =
        context.last_rep_movs_copy_failure_stage;
    attempt->last_rep_movs_copy_error =
        context.last_rep_movs_copy_error;
    attempt->last_rep_movs_copy_source =
        context.last_rep_movs_copy_source;
    attempt->last_rep_movs_copy_destination =
        context.last_rep_movs_copy_destination;
    attempt->last_rep_movs_copy_bytes =
        context.last_rep_movs_copy_bytes;
    attempt->handled_memory_store_count = context.handled_memory_store_count;
    attempt->last_memory_store_address = context.last_memory_store_address;
    attempt->last_memory_store_opcode = context.last_memory_store_opcode;
    attempt->last_memory_store_destination =
        context.last_memory_store_destination;
    attempt->last_memory_store_value = context.last_memory_store_value;
    attempt->last_memory_store_width = context.last_memory_store_width;
    attempt->last_memory_store_source_kind =
        context.last_memory_store_source_kind;
    attempt->last_memory_store_applied = context.last_memory_store_applied;
    attempt->shadow_memory_write_count = context.shadow_memory_write_count;
    attempt->shadow_memory_read_hit_count =
        context.shadow_memory_read_hit_count;
    attempt->shadow_memory_byte_count =
        static_cast<std::uint32_t>(context.shadow_memory.size());
    attempt->shadow_memory_range_valid = context.shadow_memory_range_valid;
    attempt->shadow_memory_min_address = context.shadow_memory_min_address;
    attempt->shadow_memory_max_address = context.shadow_memory_max_address;
    attempt->handled_fatal_breakpoint_count =
        context.handled_fatal_breakpoint_count;
    attempt->last_fatal_breakpoint_address =
        context.last_fatal_breakpoint_address;
    attempt->last_fatal_message_address =
        context.last_fatal_message_address;
    attempt->last_fatal_message = context.last_fatal_message;
    attempt->fatal_halt_reached = context.fatal_halt_reached;
}

int CaptureException(EXCEPTION_POINTERS* exception_info,
                     ThreadContext* context)
{
    if (exception_info != nullptr && context != nullptr)
    {
        context->exception_caught = true;
        context->exception_code =
            exception_info->ExceptionRecord->ExceptionCode;
        context->exception_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(
                exception_info->ExceptionRecord->ExceptionAddress));
#if defined(_M_IX86)
        CopySnapshotFromContextRecord(*exception_info->ContextRecord,
                                      &context->exception_snapshot);
        context->exception_eax = exception_info->ContextRecord->Eax;
        context->exception_ebx = exception_info->ContextRecord->Ebx;
        context->exception_ecx = exception_info->ContextRecord->Ecx;
        context->exception_edx = exception_info->ContextRecord->Edx;
        context->exception_esi = exception_info->ContextRecord->Esi;
        context->exception_edi = exception_info->ContextRecord->Edi;
#endif
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

#if defined(_MSC_VER) && defined(_M_IX86)
static_assert(offsetof(StackSwitchCallState, entry_address) == 0);
static_assert(offsetof(StackSwitchCallState, initial_esp) == 4);
static_assert(offsetof(StackSwitchCallState, host_esp) == 8);
static_assert(offsetof(StackSwitchCallState, guest_return_esp) == 12);
static_assert(offsetof(StackSwitchCallState, result_code) == 16);
static_assert(offsetof(StackSwitchCallState, enable_single_step_trace) == 20);
static_assert(offsetof(StackSwitchCallState, host_fs) == 24);
static_assert(offsetof(StackSwitchCallState, host_ds) == 28);
static_assert(offsetof(StackSwitchCallState, host_es) == 32);
static_assert(offsetof(StackSwitchCallState, host_gs) == 36);
static_assert(offsetof(StackSwitchCallState, host_ss) == 40);

extern "C" void RecoverHostStackException();

extern "C" __declspec(naked) std::uint32_t __stdcall
CallGuestEntryWithStack(StackSwitchCallState* state)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi

        mov ecx, [ebp + 8]
        mov eax, [ecx + 0]
        mov edx, [ecx + 4]
        xor ebx, ebx
        mov bx, fs
        mov [ecx + 24], ebx
        mov g_recovery_host_fs, ebx
        mov bx, ds
        mov [ecx + 28], ebx
        mov g_recovery_host_ds, ebx
        mov bx, es
        mov [ecx + 32], ebx
        mov g_recovery_host_es, ebx
        mov bx, gs
        mov [ecx + 36], ebx
        mov g_recovery_host_gs, ebx
        mov bx, ss
        mov [ecx + 40], ebx
        mov [ecx + 8], esp

        mov esp, edx
        cmp dword ptr [ecx + 20], 0
        je no_single_step_trace
        pushfd
        or dword ptr [esp], 100h
        popfd
no_single_step_trace:
        push ecx
        call eax
        pop ecx

        mov [ecx + 12], esp
        mov esp, [ecx + 8]
        mov dword ptr [ecx + 16], 0
        xor eax, eax

        pop edi
        pop esi
        pop ebx
        pop ebp
        ret 4
    }
}

extern "C" __declspec(naked) void __stdcall
RecoverGuestStackException()
{
    __asm
    {
        mov eax, dword ptr cs:[g_recovery_host_fs]
        mov fs, ax
        mov eax, dword ptr cs:[g_recovery_host_gs]
        mov gs, ax
        mov eax, dword ptr cs:[g_recovery_host_es]
        mov es, ax
        mov eax, dword ptr cs:[g_recovery_host_ds]
        mov ds, ax
        pop edi
        pop esi
        pop ebx
        pop ebp
        mov eax, 2
        ret 4
    }
}

void RecoverToHost(CONTEXT* context, ThreadContext* thread_context)
{
    context->Eip = reinterpret_cast<DWORD_PTR>(&RecoverGuestStackException);
    context->EFlags &= ~0x00000100U;
    context->EFlags &= ~0x00000400U;
    if (thread_context->active_call_state != nullptr)
    {
        context->Ecx = reinterpret_cast<DWORD_PTR>(
            thread_context->active_call_state);
        context->SegFs = static_cast<DWORD>(
            thread_context->active_call_state->host_fs);
        context->SegDs = static_cast<DWORD>(
            thread_context->active_call_state->host_ds);
        context->SegEs = static_cast<DWORD>(
            thread_context->active_call_state->host_es);
        context->SegGs = static_cast<DWORD>(
            thread_context->active_call_state->host_gs);
        context->SegSs = static_cast<DWORD>(
            thread_context->active_call_state->host_ss);
    }
    std::uint32_t host_esp = thread_context->host_esp;
    if (host_esp == 0 && thread_context->active_call_state != nullptr)
    {
        host_esp = thread_context->active_call_state->host_esp;
    }
    thread_context->host_esp = host_esp;
    context->Esp = host_esp;
}

bool IsGuestRangeReadable(ThreadContext* context,
                          const void* source,
                          std::uint32_t byte_count)
{
    if (context == nullptr || source == nullptr || byte_count == 0)
    {
        return false;
    }

    const std::uintptr_t base =
        static_cast<std::uintptr_t>(context->runtime_base);
    const std::uintptr_t size =
        static_cast<std::uintptr_t>(context->runtime_size);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(source);
    const std::uintptr_t end = address + byte_count;
    return address >= base && end >= address && end <= base + size;
}

bool IsGuestRangeWritable(ThreadContext* context,
                          void* destination,
                          std::uint32_t byte_count)
{
    return IsGuestRangeReadable(context, destination, byte_count);
}

bool HandlePrivilegedTrapInstruction(CONTEXT* win32_context,
                                     ThreadContext* context);
std::uint16_t ReadRegister16(const CONTEXT& win32_context,
                             std::uint8_t register_index);
void WriteGeneralRegister32(CONTEXT* win32_context,
                            std::uint8_t register_index,
                            std::uint32_t value);
std::uint8_t ReadGeneralRegister8(const CONTEXT* win32_context,
                                  std::uint8_t register_index);
void UpdateSubtract8Flags(CONTEXT* win32_context,
                          std::uint8_t left,
                          std::uint8_t right,
                          std::uint8_t result);
bool DecodeModRmMemoryAddress(const CONTEXT* win32_context,
                              const std::uint8_t* instruction,
                              std::uint32_t* destination,
                              std::uint32_t* instruction_size);
bool HandleSelectorLimitInstruction(CONTEXT* win32_context,
                                    ThreadContext* context);
bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context);
bool HandleTracedDosInterrupt21(CONTEXT* win32_context,
                                ThreadContext* context);
bool HandleTracedDosInterrupt2F(CONTEXT* win32_context,
                                ThreadContext* context);
bool HandleTracedDpmiInterrupt31(CONTEXT* win32_context,
                                 ThreadContext* context);
bool HandleTracedMouseInterrupt33(CONTEXT* win32_context,
                                  ThreadContext* context);
bool ResolveSegmentLinearRange(ThreadContext* context,
                               std::uint16_t selector,
                               std::uint32_t offset,
                               std::uint32_t byte_count,
                               bool writable,
                               std::uint32_t* linear_address);
bool HandleMscdexRequest(ThreadContext* context,
                         std::uint16_t segment,
                         std::uint16_t offset);
bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context);

bool HandleSelectorLimitInstruction(CONTEXT* win32_context,
                                    ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x0F ||
        (instruction[1] != 0x02 && instruction[1] != 0x03))
    {
        return false;
    }
    const std::uint8_t modrm = instruction[2];
    const std::uint8_t mod = (modrm >> 6) & 0x03U;
    const std::uint8_t destination_register = (modrm >> 3) & 0x07U;
    std::uint16_t selector = 0;
    std::uint32_t instruction_size = 3;
    if (mod == 0x03U)
    {
        selector = ReadRegister16(*win32_context, modrm & 0x07U);
    }
    else
    {
        std::uint32_t source = 0;
        std::uint32_t unprefixed_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + 1,
                                      &source,
                                      &unprefixed_size))
        {
            return false;
        }
        const void* source_pointer = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(source));
        if (!IsGuestRangeReadable(context, source_pointer, sizeof(selector)))
        {
            return false;
        }
        std::memcpy(&selector, source_pointer, sizeof(selector));
        instruction_size = 1U + unprefixed_size;
    }

    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    const repiu::runtime::GuestDescriptor* descriptor =
        repiu::runtime::FindDescriptor(context->selector_table, selector);
    if (descriptor != nullptr && descriptor->present)
    {
        const std::uint32_t value = instruction[1] == 0x03
            ? descriptor->limit
            : (descriptor->flags & 0xFFFFU) << 8;
        WriteGeneralRegister32(win32_context, destination_register, value);
        win32_context->EFlags |= kZeroFlag;
    }
    else
    {
        win32_context->EFlags &= ~kZeroFlag;
    }
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleSegmentPopInstruction(CONTEXT* win32_context,
                                 ThreadContext* context);
bool HandleRepStosdInstruction(CONTEXT* win32_context,
                               ThreadContext* context);
bool HandleRepMovsInstruction(CONTEXT* win32_context,
                              ThreadContext* context);
bool HandleRepCmpsbInstruction(CONTEXT* win32_context,
                               ThreadContext* context);
bool HandleLodsbInstruction(CONTEXT* win32_context,
                            ThreadContext* context);
bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context);
bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context);
bool HandleSegmentOverrideMemoryLoadInstruction(CONTEXT* win32_context,
                                                ThreadContext* context);
bool HandleFsSegmentWordLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);
bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context);
bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);
bool ReadSegmentByte(ThreadContext* context,
                     std::uint8_t segment_register,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint8_t* value);
bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleTracedMemoryAddInstruction(CONTEXT* win32_context,
                                      ThreadContext* context);

bool HandleTracedMemoryOrInstruction(CONTEXT* win32_context,
                                     ThreadContext* context);
bool HandleTracedMemoryCompareByteInstruction(CONTEXT* win32_context,
                                              ThreadContext* context);
bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);
bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);
bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context);
bool HandleDosMemoryAccess(CONTEXT* win32_context,
                           ThreadContext* context);
bool NoteSuccessfulAotGuestWrite(ThreadContext* context,
                                 std::uint32_t destination,
                                 std::uint32_t byte_count);
std::uint32_t AotGuestAddressForExecutionAddress(
    const ThreadContext* context,
    std::uint32_t execution_address);

bool IsGuestInstructionPointer(const ThreadContext* context,
                               std::uint32_t eip)
{
    if (context == nullptr || context->runtime_size == 0)
    {
        return false;
    }

    const std::uint32_t runtime_end =
        context->runtime_base + context->runtime_size;
    return eip >= context->runtime_base &&
           runtime_end >= context->runtime_base &&
           eip < runtime_end;
}

void RecordExecutionProbe(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->execution_probe_configured || context->execution_probe_hit ||
        win32_context->Eip < context->runtime_base ||
        static_cast<std::uint32_t>(win32_context->Eip) -
                context->runtime_base != context->execution_probe_offset)
    {
        return;
    }
    context->execution_probe_hit = true;
    CopySnapshotFromContextRecord(*win32_context,
                                  &context->execution_probe_snapshot);
    const void* stack = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context, stack,
                             sizeof(context->execution_probe_stack)))
    {
        std::memcpy(context->execution_probe_stack, stack,
                    sizeof(context->execution_probe_stack));
    }
}

bool HandleSingleStepTrace(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->enable_single_step_trace)
    {
        return false;
    }
    RecordExecutionProbe(win32_context, context);
    const std::uint32_t eip_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (eip_offset >= 0x000F38F6U && eip_offset <= 0x000F3902U)
    {
        constexpr std::uint32_t stages[] = {
            0x000F38F6U, 0x000F38FAU, 0x000F38FEU,
            0x000F3900U, 0x000F3902U};
        for (std::uint32_t index = 0; index < 5; ++index)
        {
            if (eip_offset == stages[index])
            {
                context->linexe_export_name_stage_mask |= 1U << index;
            }
        }
    }
    if (eip_offset == 0x000F37E8U)
    {
        ++context->linexe_scan_entry_count;
    }
    else if (eip_offset == 0x000F382DU)
    {
        ++context->linexe_module_candidate_count;
        const auto* selector = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x10U));
        if (IsGuestRangeReadable(context, selector, sizeof(*selector)))
        {
            context->linexe_module_selector_stack_value = *selector;
        }
        const auto* offset = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x08U));
        if (IsGuestRangeReadable(context, offset, sizeof(*offset)))
        {
            context->linexe_module_offset_stack_value = *offset;
        }
    }
    else if (eip_offset == 0x000F3818U)
    {
        context->linexe_root_selector_eax = win32_context->Eax;
        context->linexe_root_read_gs = context->guest_gs;
    }
    else if (eip_offset == 0x000F3889U)
    {
        ++context->linexe_module_match_count;
    }
    else if (eip_offset == 0x000F384CU)
    {
        ++context->linexe_name_pointer_valid_count;
    }
    else if (eip_offset == 0x000F3853U)
    {
        ++context->linexe_name_byte_instruction_count;
    }
    else if (eip_offset == 0x000F393FU)
    {
        ++context->linexe_export_match_count;
    }
    else if (eip_offset == 0x000F38BEU)
    {
        ++context->linexe_export_entry_loop_count;
    }
    else if (eip_offset == 0x000F3974U)
    {
        ++context->linexe_export_compare_count;
        context->linexe_export_compare_eax = win32_context->Eax;
        context->linexe_export_compare_ecx = win32_context->Ecx;
        context->linexe_export_compare_eflags = win32_context->EFlags;
    }
    else if (eip_offset == 0x000F396DU)
    {
        context->linexe_export_count_load_edx = win32_context->Edx;
        context->linexe_export_count_load_gs = context->guest_gs;
    }
    else if (eip_offset == 0x000F3963U)
    {
        const auto* offset = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x08U));
        const auto* selector = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x10U));
        if (IsGuestRangeReadable(context, offset, sizeof(*offset)))
        {
            context->linexe_export_offset_stack_value = *offset;
        }
        if (IsGuestRangeReadable(context, selector, sizeof(*selector)))
        {
            context->linexe_export_selector_stack_value = *selector;
        }
    }
    else if (eip_offset == 0x000F38B9U ||
             eip_offset == 0x000F395FU)
    {
        const auto* offset = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x08U));
        const auto* selector = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp + 0x10U));
        const std::uint32_t module_offset =
            IsGuestRangeReadable(context, offset, sizeof(*offset)) ?
                *offset : 0;
        const std::uint16_t module_selector =
            IsGuestRangeReadable(context, selector, sizeof(*selector)) ?
                *selector : 0;
        if (eip_offset == 0x000F38B9U)
        {
            context->linexe_export_jump_source_esp = win32_context->Esp;
            context->linexe_export_jump_source_module_offset = module_offset;
            context->linexe_export_jump_source_module_selector =
                module_selector;
        }
        else
        {
            context->linexe_export_jump_target_esp = win32_context->Esp;
            context->linexe_export_jump_target_module_offset = module_offset;
            context->linexe_export_jump_target_module_selector =
                module_selector;
        }
    }
    else if (eip_offset == 0x000F3900U)
    {
        ++context->linexe_export_name_compare_count;
        context->linexe_export_name_compare_gs = context->guest_gs;
        context->linexe_export_name_compare_edi = win32_context->Edi;
        context->linexe_export_name_compare_esi = win32_context->Esi;
        std::uint8_t actual = 0;
        if (ReadSegmentByte(context, 5, context->guest_gs,
                            win32_context->Edi, &actual))
        {
            context->linexe_export_name_actual_byte = actual;
        }
        const auto* expected = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(win32_context->Esi));
        if (IsGuestRangeReadable(context, expected, sizeof(*expected)))
        {
            context->linexe_export_name_expected_byte = *expected;
        }
    }
    else if (eip_offset == 0x000F37A5U)
    {
        ++context->linexe_bridge_entry_count;
        context->linexe_bridge_selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        context->linexe_bridge_offset = win32_context->Edi;
        context->linexe_bridge_esp = win32_context->Esp;
        context->linexe_bridge_ebp = win32_context->Ebp;
        repiu::hle::LinexeService service{};
        context->linexe_bridge_gate_valid =
            context->linexe_bridge_selector ==
                context->linexe_gate_plan.linexe_code_selector &&
            repiu::hle::DecodeLinexeCallGate(
                context->linexe_gate_plan,
                context->linexe_bridge_offset,
                &service);
        if (context->linexe_bridge_gate_valid)
        {
            context->linexe_bridge_service =
                static_cast<std::uint32_t>(service);
        }
        const auto* stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp));
        if (IsGuestRangeReadable(context,
                                 stack,
                                 sizeof(context->linexe_bridge_stack)))
        {
            std::memcpy(context->linexe_bridge_stack,
                        stack,
                        sizeof(context->linexe_bridge_stack));
        }
    }
    else if (eip_offset == 0x000F39A6U)
    {
        ++context->linexe_scan_return_count;
        context->linexe_scan_return_eax = win32_context->Eax;
        context->linexe_scan_return_ebp = win32_context->Ebp;
    }
    else if (eip_offset == 0x000F3F9BU)
    {
        context->linexe_scan_caller_eax = win32_context->Eax;
    }
    else if (eip_offset == 0x000F3FD2U)
    {
        context->linexe_selector_init_results[0] = win32_context->Eax;
    }
    else if (eip_offset == 0x000F3FE1U)
    {
        context->linexe_selector_init_results[1] = win32_context->Eax;
    }
    else if (eip_offset == 0x000F3FF0U)
    {
        context->linexe_selector_init_results[2] = win32_context->Eax;
    }
    const std::uint32_t eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    if (IsGuestInstructionPointer(context, eip))
    {
        context->single_step_eip.store(eip, std::memory_order_relaxed);
        context->single_step_eax.store(win32_context->Eax,
                                       std::memory_order_relaxed);
        context->single_step_ebx.store(win32_context->Ebx,
                                       std::memory_order_relaxed);
        context->single_step_ecx.store(win32_context->Ecx,
                                       std::memory_order_relaxed);
        context->single_step_edx.store(win32_context->Edx,
                                       std::memory_order_relaxed);
        context->single_step_esi.store(win32_context->Esi,
                                       std::memory_order_relaxed);
        context->single_step_edi.store(win32_context->Edi,
                                       std::memory_order_relaxed);
        context->single_step_esp.store(win32_context->Esp,
                                       std::memory_order_relaxed);
        context->single_step_ebp.store(win32_context->Ebp,
                                       std::memory_order_relaxed);
        context->single_step_eflags.store(win32_context->EFlags,
                                          std::memory_order_relaxed);
        context->single_step_cs.store(win32_context->SegCs,
                                      std::memory_order_relaxed);
        context->single_step_ds.store(win32_context->SegDs,
                                      std::memory_order_relaxed);
        context->single_step_es.store(win32_context->SegEs,
                                      std::memory_order_relaxed);
        context->single_step_ss.store(win32_context->SegSs,
                                      std::memory_order_relaxed);
        context->single_step_fs.store(win32_context->SegFs,
                                      std::memory_order_relaxed);
        context->single_step_gs.store(win32_context->SegGs,
                                      std::memory_order_relaxed);
        context->single_step_trace_count.fetch_add(
            1,
            std::memory_order_relaxed);
    }

    if (context->enable_privileged_trap_hle &&
        (HandleSelectorLimitInstruction(win32_context, context) ||
         HandlePrivilegedTrapInstruction(win32_context, context)))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }
    if (context->enable_privileged_trap_hle &&
        HandlePortIoInstruction(win32_context, context))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }
    if (context->enable_traced_dos_hle &&
        (HandleTracedDosInterrupt21(win32_context, context) ||
         HandleTracedDosInterrupt2F(win32_context, context) ||
         HandleTracedDpmiInterrupt31(win32_context, context) ||
         HandleTracedMouseInterrupt33(win32_context, context)))
    {
        win32_context->EFlags |= 0x00000100U;
        return true;
    }
    if (context->enable_segment_load_hle &&
        (HandleSegmentLoadInstruction(win32_context, context) ||
         HandleSegmentPopInstruction(win32_context, context) ||
         HandleRepStosdInstruction(win32_context, context) ||
         HandleRepMovsInstruction(win32_context, context) ||
         HandleRepCmpsbInstruction(win32_context, context) ||
         HandleLodsbInstruction(win32_context, context) ||
         HandleSegmentStoreInstruction(win32_context, context) ||
         HandleSegmentOverrideMemoryLoadInstruction(win32_context, context) ||
         HandleSegmentOverrideByteLoadInstruction(win32_context, context) ||
         HandleFsSegmentWordLoadInstruction(win32_context, context) ||
         HandleSegmentMemoryCompareInstruction(win32_context, context) ||
         HandleSegmentMemoryLoadInstruction(win32_context, context) ||
         HandleTracedMemoryLoadInstruction(win32_context, context) ||
         HandleTracedMemoryAddInstruction(win32_context, context) ||
         HandleTracedMemoryOrInstruction(win32_context, context) ||
         HandleTracedMemoryCompareByteInstruction(win32_context, context) ||
         HandleTracedMemoryStoreInstruction(win32_context, context) ||
         HandleTracedMemoryTestInstruction(win32_context, context) ||
         HandleTracedFpuMemoryInstruction(win32_context, context) ||
         HandleDosMemoryAccess(win32_context, context)))
    {
        HandleSegmentStoreInstruction(win32_context, context);
        HandleSegmentOverrideMemoryLoadInstruction(win32_context, context);
        win32_context->EFlags |= 0x00000100U;
        return true;
    }

    if (detail::TryEnterNativeFastPath(win32_context,
                                       &context->native_fast_path,
                                       context->runtime_base,
                                       context->runtime_size))
    {
        return true;
    }
    win32_context->EFlags |= 0x00000100U;
    return true;
}

bool WriteGuestUInt16(ThreadContext* context,
                      void* destination,
                      std::uint16_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest segment store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    DWORD ignored_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        previous_protect,
                        &ignored_protect))
    {
        return false;
    }
    return NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value));
}

bool WriteGuestUInt8(ThreadContext* context,
                     void* destination,
                     std::uint8_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest byte store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    DWORD ignored_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        previous_protect,
                        &ignored_protect))
    {
        return false;
    }
    return NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value));
}

bool WriteGuestUInt32(ThreadContext* context,
                      void* destination,
                      std::uint32_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest dword store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    DWORD ignored_protect = 0;
    if (!VirtualProtect(destination,
                        sizeof(value),
                        previous_protect,
                        &ignored_protect))
    {
        return false;
    }
    return NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value));
}

bool WriteGuestBytes(ThreadContext* context,
                     void* destination,
                     const void* source,
                     std::size_t byte_count)
{
    if (source == nullptr ||
        !IsGuestRangeWritable(context, destination, byte_count))
    {
        return false;
    }

    DWORD previous_protect = 0;
    if (!VirtualProtect(destination,
                        byte_count,
                        PAGE_EXECUTE_READWRITE,
                        &previous_protect))
    {
        std::ostringstream stream;
        stream << "VirtualProtect failed for guest byte store with error "
               << GetLastError();
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, source, byte_count);

    DWORD ignored_protect = 0;
    if (!VirtualProtect(destination,
                        byte_count,
                        previous_protect,
                        &ignored_protect))
    {
        return false;
    }
    if (byte_count > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    return NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        static_cast<std::uint32_t>(byte_count));
}

bool ReadGuestUInt32(ThreadContext* context,
                     const void* source,
                     std::uint32_t* value)
{
    if (value == nullptr ||
        !IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }

    std::memcpy(value, source, sizeof(*value));
    return true;
}

void WriteShadowMemory(ThreadContext* context,
                       std::uint32_t destination,
                       std::uint32_t value,
                       std::uint32_t byte_count)
{
    if (context == nullptr || byte_count == 0)
    {
        return;
    }

    ++context->shadow_memory_write_count;
    const ShadowWriteProvenance provenance = {
        context->shadow_write_provenance_count + 1,
        context->last_memory_store_address,
        context->last_memory_store_opcode,
        destination,
        value,
        byte_count,
    };
    const std::uint32_t provenance_slot =
        context->shadow_write_provenance_count %
        kShadowWriteProvenanceCapacity;
    context->shadow_write_provenance[provenance_slot] = provenance;
    ++context->shadow_write_provenance_count;
    for (std::uint32_t index = 0; index < byte_count; ++index)
    {
        const std::uint32_t address = destination + index;
        context->shadow_memory[address] =
            static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU);
        if (!context->shadow_memory_range_valid)
        {
            context->shadow_memory_range_valid = true;
            context->shadow_memory_min_address = address;
            context->shadow_memory_max_address = address;
        }
        else
        {
            context->shadow_memory_min_address =
                std::min(context->shadow_memory_min_address, address);
            context->shadow_memory_max_address =
                std::max(context->shadow_memory_max_address, address);
        }
    }
}

bool ReadShadowUInt32(ThreadContext* context,
                      std::uint32_t source,
                      std::uint32_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const auto found = context->shadow_memory.find(source + index);
        if (found != context->shadow_memory.end())
        {
            result |=
                static_cast<std::uint32_t>(found->second) << (index * 8);
            continue;
        }

        const std::uint32_t address = source + index;
        if (!context->shadow_zero_payload_valid ||
            address < context->shadow_zero_payload_begin ||
            address >= context->shadow_zero_payload_end)
        {
            return false;
        }
    }

    *value = result;
    ++context->shadow_memory_read_hit_count;
    return true;
}

bool ReadShadowUInt8(ThreadContext* context,
                     std::uint32_t source,
                     std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    const auto found = context->shadow_memory.find(source);
    if (found != context->shadow_memory.end())
    {
        *value = found->second;
    }
    else if (context->shadow_zero_payload_valid &&
             source >= context->shadow_zero_payload_begin &&
             source < context->shadow_zero_payload_end)
    {
        *value = 0;
    }
    else
    {
        return false;
    }
    ++context->shadow_memory_read_hit_count;
    return true;
}

bool AppendConsoleOutput(ThreadContext* context,
                         const void* source,
                         std::uint32_t byte_count,
                         bool stderr_stream = false)
{
    if (context == nullptr || source == nullptr || byte_count == 0)
    {
        return false;
    }

    if (!IsGuestRangeReadable(context, source, byte_count))
    {
        context->hle_message = "DOS console output buffer is outside runtime memory";
        return false;
    }

    char* output = stderr_stream
        ? context->hle_stderr_output
        : context->hle_stdout_output;
    std::uint32_t* output_size = stderr_stream
        ? &context->hle_stderr_output_size
        : &context->hle_stdout_output_size;
    const std::uint32_t capacity = stderr_stream
        ? sizeof(context->hle_stderr_output)
        : sizeof(context->hle_stdout_output);
    const std::uint32_t available = capacity - *output_size;
    const std::uint32_t copied = std::min(byte_count, available);
    if (copied == 0)
    {
        return false;
    }

    std::memcpy(
        output + *output_size,
        source,
        copied);
    *output_size += copied;
    return true;
}

bool ReadGuestAsciz(ThreadContext* context,
                    std::uint32_t address,
                    std::uint32_t max_length,
                    std::string* value)
{
    if (context == nullptr || value == nullptr || max_length == 0)
    {
        return false;
    }

    value->clear();
    const char* text = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(address));
    for (std::uint32_t index = 0; index < max_length; ++index)
    {
        if (!IsGuestRangeReadable(context, text + index, 1))
        {
            return false;
        }

        const char ch = text[index];
        if (ch == '\0')
        {
            return true;
        }
        value->push_back(ch);
    }

    return false;
}

void RecoverFromHleExit(CONTEXT* win32_context,
                        ThreadContext* thread_context)
{
    thread_context->process_exit = true;
    thread_context->returned = true;
    if (thread_context->use_guest_stack)
    {
        RecoverToHost(win32_context, thread_context);
    }
    else
    {
        win32_context->Eip =
            reinterpret_cast<DWORD_PTR>(&RecoverHostStackException);
    }
}

void RecordHandledDosInterrupt(ThreadContext* context,
                               std::uint8_t vector,
                               std::uint16_t ax);

void RecordDosChangeDirectory(ThreadContext* context,
                              const std::string& guest_path,
                              const repiu::hle::DosResolvedPath& resolved)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_chdir_count;
    context->last_dos_chdir_guest_path = guest_path;
    context->last_dos_chdir_host_path = resolved.host_path.string();
    context->last_dos_chdir_virtual_path = resolved.dos_path;
    context->last_dos_chdir_success =
        resolved.result == repiu::hle::DosPathResult::kOk;
    context->last_dos_chdir_error =
        repiu::hle::DosPathResultToErrorCode(resolved.result);
}

void RecordDosPathTrace(ThreadContext* context,
                        const char* service,
                        const std::string& guest_path,
                        const std::string& virtual_path,
                        const std::string& host_path,
                        bool success,
                        std::uint16_t dos_error,
                        std::uint8_t drive,
                        std::uint8_t access_mode)
{
    if (context == nullptr || service == nullptr)
    {
        return;
    }

    Win32DosPathObservation& observation = context->dos_path;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32DosPathTraceCapacity;
    Win32DosPathTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.service = service;
    entry.guest_path = guest_path;
    entry.virtual_path = virtual_path;
    entry.host_path = host_path;
    entry.result = success ? "success" : "failure";
    entry.dos_error = dos_error;
    entry.drive = drive;
    entry.access_mode = access_mode;
    observation.observed_count = sequence;
    if (observation.trace_stored_count < kWin32DosPathTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_limit_reached = true;
    }
}

std::string BuildCurrentDosVirtualPath(
    const repiu::hle::DosVirtualFileSystemState& state)
{
    const std::string current_directory =
        repiu::hle::GetDosCurrentDirectory(state);
    if (current_directory.empty())
    {
        return "\\";
    }

    return "\\" + current_directory;
}

std::string BuildCurrentDosHostPath(
    const repiu::hle::DosVirtualFileSystemState& state)
{
    std::filesystem::path host_path = state.host_root;
    for (const std::string& component : state.current_components)
    {
        host_path /= component;
    }
    return host_path.lexically_normal().string();
}

bool HandleDosChangeDirectory(CONTEXT* win32_context, ThreadContext* context)
{
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    if (!ReadGuestAsciz(context,
                        static_cast<std::uint32_t>(win32_context->Edx),
                        260,
                        &guest_path))
    {
        resolved.result = repiu::hle::DosPathResult::kPathNotFound;
        resolved.guest_path = "";
        resolved.message = "DOS chdir path is outside runtime memory";
        RecordDosChangeDirectory(context, guest_path, resolved);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (!repiu::hle::ChangeDosCurrentDirectory(
            &context->dos_file_system,
            guest_path,
            &resolved))
    {
        return false;
    }

    RecordDosChangeDirectory(context, guest_path, resolved);
    RecordDosPathTrace(context,
                       "chdir",
                       guest_path,
                       resolved.dos_path,
                       resolved.host_path.string(),
                       resolved.result == repiu::hle::DosPathResult::kOk,
                       repiu::hle::DosPathResultToErrorCode(
                           resolved.result),
                       0,
                       0);
    if (resolved.result == repiu::hle::DosPathResult::kOk)
    {
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

bool HandleDosGetCurrentDirectory(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::string current_directory =
        repiu::hle::GetDosCurrentDirectory(context->dos_file_system);
    const std::uint8_t drive = static_cast<std::uint8_t>(
        win32_context->Edx & 0xFFU);
    ++context->handled_dos_getcwd_count;
    context->last_dos_getcwd_drive = drive;
    context->last_dos_getcwd_path = current_directory;
    if (current_directory.size() >= 64)
    {
        context->last_dos_getcwd_success = false;
        context->last_dos_getcwd_error = 0x0005U;
        RecordDosPathTrace(context,
                           "getcwd",
                           "",
                           BuildCurrentDosVirtualPath(
                               context->dos_file_system),
                           BuildCurrentDosHostPath(context->dos_file_system),
                           false,
                           context->last_dos_getcwd_error,
                           drive,
                           0);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0005U;
        win32_context->EFlags |= 1U;
        return true;
    }

    std::string asciz = current_directory;
    asciz.push_back('\0');
    void* destination = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(win32_context->Esi));
    if (!WriteGuestBytes(context,
                         destination,
                         asciz.data(),
                         asciz.size()))
    {
        context->last_dos_getcwd_success = false;
        context->last_dos_getcwd_error = 0x0003U;
        RecordDosPathTrace(context,
                           "getcwd",
                           "",
                           BuildCurrentDosVirtualPath(
                               context->dos_file_system),
                           BuildCurrentDosHostPath(context->dos_file_system),
                           false,
                           context->last_dos_getcwd_error,
                           drive,
                           0);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    context->last_dos_getcwd_success = true;
    context->last_dos_getcwd_error = 0;
    RecordDosPathTrace(context,
                       "getcwd",
                       "",
                       BuildCurrentDosVirtualPath(context->dos_file_system),
                       BuildCurrentDosHostPath(context->dos_file_system),
                       true,
                       0,
                       drive,
                       0);
    win32_context->EFlags &= ~1U;
    return true;
}

void HandleDosGetCurrentDrive(CONTEXT* win32_context, ThreadContext* context)
{
    constexpr std::uint8_t kDefaultDriveC = 2;
    ++context->handled_dos_getdrive_count;
    context->last_dos_getdrive_value = kDefaultDriveC;
    RecordDosPathTrace(context,
                       "getdrive",
                       "",
                       "",
                       "",
                       true,
                       0,
                       kDefaultDriveC,
                       0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFFFF00U) | kDefaultDriveC;
    win32_context->EFlags &= ~1U;
}

void RecordDosOpen(ThreadContext* context,
                   const std::string& guest_path,
                   const repiu::hle::DosResolvedPath& resolved,
                   std::uint16_t handle,
                   std::uint8_t access_mode)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_open_count;
    context->last_dos_open_guest_path = guest_path;
    context->last_dos_open_host_path = resolved.host_path.string();
    context->last_dos_open_virtual_path = resolved.dos_path;
    context->last_dos_open_success =
        resolved.result == repiu::hle::DosPathResult::kOk;
    context->last_dos_open_error =
        repiu::hle::DosPathResultToErrorCode(resolved.result);
    context->last_dos_open_handle = handle;
    context->last_dos_open_access_mode = access_mode;
    RecordDosPathTrace(context,
                       "open",
                       guest_path,
                       resolved.dos_path,
                       resolved.host_path.string(),
                       resolved.result == repiu::hle::DosPathResult::kOk,
                       repiu::hle::DosPathResultToErrorCode(
                           resolved.result),
                       0,
                       access_mode);
}

bool HandleDosOpenFile(CONTEXT* win32_context, ThreadContext* context)
{
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    const std::uint8_t access_mode = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    if (!ReadGuestAsciz(context,
                        static_cast<std::uint32_t>(win32_context->Edx),
                        260,
                        &guest_path))
    {
        resolved.result = repiu::hle::DosPathResult::kFileNotFound;
        resolved.guest_path = "";
        resolved.message = "DOS open path is outside runtime memory";
        RecordDosOpen(context, guest_path, resolved, 0, access_mode);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0002U;
        win32_context->EFlags |= 1U;
        return true;
    }

    std::uint16_t handle = 0;
    if (!repiu::hle::OpenDosFile(&context->dos_file_system,
                                 guest_path,
                                 access_mode,
                                 &resolved,
                                 &handle))
    {
        return false;
    }

    RecordDosOpen(context, guest_path, resolved, handle, access_mode);
    if (resolved.result == repiu::hle::DosPathResult::kOk)
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | handle;
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

bool HandleDosFileAttributes(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint8_t subfunction = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    if (!ReadGuestAsciz(context, win32_context->Edx, 260, &guest_path))
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    bool success = false;
    if (subfunction == 0x00)
    {
        std::uint16_t attributes = 0;
        success = repiu::hle::QueryDosFileAttributes(
            &context->dos_file_system,
            guest_path,
            &resolved,
            &attributes);
        if (success)
        {
            win32_context->Ecx =
                (win32_context->Ecx & 0xFFFF0000U) | attributes;
        }
    }
    else if (subfunction == 0x01)
    {
        success = repiu::hle::SetDosFileAttributes(
            &context->dos_file_system,
            guest_path,
            static_cast<std::uint16_t>(win32_context->Ecx & 0xFFFFU),
            &resolved);
    }
    else
    {
        resolved.result = repiu::hle::DosPathResult::kAccessDenied;
        resolved.message = "unsupported DOS file attribute subfunction";
    }

    RecordDosPathTrace(context,
                       subfunction == 0 ? "attributes-query" : "attributes-set",
                       guest_path,
                       resolved.dos_path,
                       resolved.host_path.string(),
                       success,
                       repiu::hle::DosPathResultToErrorCode(resolved.result),
                       0,
                       0);
    if (success)
    {
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

const repiu::hle::DosOpenFileHandle* FindDosOpenFile(
    const ThreadContext* context,
    std::uint16_t handle)
{
    if (context == nullptr)
    {
        return nullptr;
    }
    for (const repiu::hle::DosOpenFileHandle& candidate :
         context->dos_file_system.open_files)
    {
        if (candidate.open && candidate.handle == handle)
        {
            return &candidate;
        }
    }
    return nullptr;
}

void CaptureDosTermination(CONTEXT* win32_context,
                           ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }
    context->dos_termination_captured = true;
    context->dos_termination_ax = win32_context->Eax & 0xFFFFU;
    context->dos_termination_eip = win32_context->Eip;
    context->dos_termination_esp = win32_context->Esp;
    const void* stack = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(
            context, stack, sizeof(context->dos_termination_stack)))
    {
        std::memcpy(context->dos_termination_stack,
                    stack,
                    sizeof(context->dos_termination_stack));
    }
}

Win32DosFileIoTraceEntry& AllocateDosFileIoTrace(
    ThreadContext* context,
    const char* operation,
    std::uint16_t handle)
{
    Win32DosFileIoObservation& observation = context->dos_file_io;
    const std::uint32_t sequence = ++observation.observed_count;
    const std::uint32_t slot =
        (sequence - 1U) % kWin32DosFileIoTraceCapacity;
    Win32DosFileIoTraceEntry& entry = observation.trace[slot];
    entry = {};
    entry.valid = true;
    entry.sequence = sequence;
    entry.operation = operation;
    entry.handle = handle;
    const repiu::hle::DosOpenFileHandle* open_file =
        FindDosOpenFile(context, handle);
    if (open_file != nullptr)
    {
        entry.host_path = open_file->host_path.string();
        entry.position_before = static_cast<std::uint32_t>(
            open_file->file_offset);
        entry.position_after = entry.position_before;
    }
    observation.trace_stored_count = std::min(
        observation.observed_count, kWin32DosFileIoTraceCapacity);
    observation.trace_wrapped =
        observation.observed_count > kWin32DosFileIoTraceCapacity;
    return entry;
}

void RecordDosRead(const CONTEXT* win32_context,
                   ThreadContext* context,
                   std::uint16_t handle,
                   std::uint32_t requested_bytes,
                   std::uint32_t actual_bytes,
                   std::uint32_t buffer,
                   bool success,
                   std::uint16_t error,
                   const std::vector<std::uint8_t>* bytes)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_read_count;
    context->last_dos_read_handle = handle;
    context->last_dos_read_requested_bytes = requested_bytes;
    context->last_dos_read_actual_bytes = actual_bytes;
    context->last_dos_read_buffer = buffer;
    context->last_dos_read_success = success;
    context->last_dos_read_error = error;
    Win32DosFileIoTraceEntry& entry =
        AllocateDosFileIoTrace(context, "read", handle);
    if (win32_context != nullptr)
    {
        entry.guest_eip = win32_context->Eip;
        entry.guest_esp = win32_context->Esp;
        const auto* guest_stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp));
        if (IsGuestRangeReadable(context,
                                 guest_stack,
                                 sizeof(entry.guest_stack)))
        {
            std::memcpy(entry.guest_stack,
                        guest_stack,
                        sizeof(entry.guest_stack));
        }
    }
    entry.requested_bytes = requested_bytes;
    entry.actual_bytes = actual_bytes;
    entry.dos_error = error;
    const repiu::hle::DosOpenFileHandle* open_file =
        FindDosOpenFile(context, handle);
    if (open_file != nullptr)
    {
        entry.position_after = static_cast<std::uint32_t>(
            open_file->file_offset);
        entry.position_before = entry.position_after - actual_bytes;
    }
    if (bytes != nullptr)
    {
        entry.prefix_size = static_cast<std::uint32_t>(std::min<std::size_t>(
            bytes->size(), kWin32DosFileIoPrefixCapacity));
        if (entry.prefix_size != 0)
        {
            std::memcpy(entry.prefix, bytes->data(), entry.prefix_size);
        }
    }
}

bool HandleDosReadFile(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    const std::uint32_t requested_bytes =
        static_cast<std::uint32_t>(win32_context->Ecx);
    const std::uint32_t buffer =
        static_cast<std::uint32_t>(win32_context->Edx);

    std::vector<std::uint8_t> bytes;
    std::uint32_t actual_bytes = 0;
    std::uint16_t dos_error = 0;
    if (!repiu::hle::ReadDosFile(&context->dos_file_system,
                                 handle,
                                 requested_bytes,
                                 &bytes,
                                 &actual_bytes,
                                 &dos_error))
    {
        return false;
    }

    if (dos_error != 0)
    {
        RecordDosRead(win32_context,
                      context,
                      handle,
                      requested_bytes,
                      0,
                      buffer,
                      false,
                      dos_error,
                      &bytes);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | dos_error;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (!bytes.empty() &&
        !WriteGuestBytes(context,
                         reinterpret_cast<void*>(
                             static_cast<std::uintptr_t>(buffer)),
                         bytes.data(),
                         bytes.size()))
    {
        constexpr std::uint16_t kPathNotFound = 0x0003;
        RecordDosRead(win32_context,
                      context,
                      handle,
                      requested_bytes,
                      0,
                      buffer,
                      false,
                      kPathNotFound,
                      &bytes);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | kPathNotFound;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosRead(win32_context,
                  context,
                  handle,
                  requested_bytes,
                  actual_bytes,
                  buffer,
                  true,
                  0,
                  &bytes);
    win32_context->Eax = actual_bytes;
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosSeek(ThreadContext* context,
                   std::uint16_t handle,
                   std::uint8_t origin,
                   std::int32_t offset,
                   std::uint32_t position_before,
                   std::uint32_t position,
                   bool success,
                   std::uint16_t error)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_seek_count;
    context->last_dos_seek_handle = handle;
    context->last_dos_seek_origin = origin;
    context->last_dos_seek_offset = offset;
    context->last_dos_seek_position = position;
    context->last_dos_seek_success = success;
    context->last_dos_seek_error = error;
    Win32DosFileIoTraceEntry& entry =
        AllocateDosFileIoTrace(context, "seek", handle);
    entry.origin = origin;
    entry.seek_offset = offset;
    entry.position_before = position_before;
    entry.position_after = success ? position : position_before;
    entry.dos_error = error;
}

bool HandleDosSeekFile(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t origin = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    const std::uint32_t raw_offset =
        ((win32_context->Ecx & 0xFFFFU) << 16) |
        (win32_context->Edx & 0xFFFFU);
    const std::int32_t offset = static_cast<std::int32_t>(raw_offset);
    const repiu::hle::DosOpenFileHandle* open_file_before =
        FindDosOpenFile(context, handle);
    const std::uint32_t position_before = open_file_before != nullptr
        ? static_cast<std::uint32_t>(open_file_before->file_offset)
        : 0U;

    std::uint32_t new_position = 0;
    std::uint16_t dos_error = 0;
    if (!repiu::hle::SeekDosFile(&context->dos_file_system,
                                 handle,
                                 origin,
                                 offset,
                                 &new_position,
                                 &dos_error))
    {
        return false;
    }

    if (dos_error != 0)
    {
        RecordDosSeek(context,
                      handle,
                      origin,
                      offset,
                      position_before,
                      0,
                      false,
                      dos_error);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | dos_error;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosSeek(context,
                  handle,
                  origin,
                  offset,
                  position_before,
                  new_position,
                  true,
                  0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFF0000U) |
        (new_position & 0xFFFFU);
    win32_context->Edx =
        (win32_context->Edx & 0xFFFF0000U) |
        ((new_position >> 16) & 0xFFFFU);
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosClose(ThreadContext* context,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_close_count;
    context->last_dos_close_handle = handle;
    context->last_dos_close_success = success;
    context->last_dos_close_error = error;
}

bool HandleDosCloseFile(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);

    std::uint16_t dos_error = 0;
    if (!repiu::hle::CloseDosFile(&context->dos_file_system,
                                  handle,
                                  &dos_error))
    {
        return false;
    }

    if (dos_error != 0)
    {
        RecordDosClose(context, handle, false, dos_error);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | dos_error;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosClose(context, handle, true, 0);
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosIoctl(ThreadContext* context,
                    std::uint8_t subfunction,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error,
                    std::uint16_t device_info)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_ioctl_count;
    context->last_dos_ioctl_subfunction = subfunction;
    context->last_dos_ioctl_handle = handle;
    context->last_dos_ioctl_success = success;
    context->last_dos_ioctl_error = error;
    context->last_dos_ioctl_device_info = device_info;
}

bool HandleDosIoctl(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t subfunction = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    if (subfunction != 0x00)
    {
        std::ostringstream stream;
        stream << "unsupported DOS IOCTL subfunction AL=0x"
               << std::hex << static_cast<unsigned>(subfunction);
        context->hle_message = stream.str();
        return false;
    }

    std::uint16_t device_info = 0;
    if (handle < 5)
    {
        device_info = 0x0080;
        RecordDosIoctl(context, subfunction, handle, true, 0, device_info);
        win32_context->Edx =
            (win32_context->Edx & 0xFFFF0000U) | device_info;
        win32_context->EFlags &= ~1U;
        return true;
    }

    if (repiu::hle::IsDosFileHandleOpen(context->dos_file_system, handle))
    {
        RecordDosIoctl(context, subfunction, handle, true, 0, device_info);
        win32_context->Edx =
            (win32_context->Edx & 0xFFFF0000U) | device_info;
        win32_context->EFlags &= ~1U;
        return true;
    }

    constexpr std::uint16_t kInvalidHandle = 0x0006;
    RecordDosIoctl(context,
                   subfunction,
                   handle,
                   false,
                   kInvalidHandle,
                   0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFF0000U) | kInvalidHandle;
    win32_context->EFlags |= 1U;
    return true;
}

void RecordDosResize(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint16_t paragraphs,
                     bool success,
                     std::uint16_t error)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_resize_count;
    context->last_dos_resize_selector = selector;
    context->last_dos_resize_paragraphs = paragraphs;
    context->last_dos_resize_success = success;
    context->last_dos_resize_error = error;
}

void RecordLowMemoryAccess(CONTEXT* win32_context,
                           ThreadContext* context,
                           std::uint8_t opcode,
                           std::uint32_t destination,
                           std::uint32_t value)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_low_memory_access_count;
    context->last_low_memory_access_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_low_memory_access_opcode = opcode;
    context->last_low_memory_access_esi = win32_context->Esi;
    context->last_low_memory_access_edi = win32_context->Edi;
    context->last_low_memory_access_destination = destination;
    context->last_low_memory_access_value = value;
}

bool HandleDosResizeMemoryBlock(CONTEXT* win32_context,
                                ThreadContext* context)
{
    const std::uint16_t paragraphs = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    constexpr std::uint16_t kObservedPiuResizeSelector = 0x0024;
    constexpr std::uint16_t kObservedPiuStageCfgFailureLimitParagraphs =
        0x4AE0;
    constexpr std::uint16_t kObservedPiuResizeLimitParagraphs = 0xE700;
    constexpr std::uint16_t kInsufficientMemory = 0x0008;
    if (context->guest_es == kObservedPiuResizeSelector &&
        paragraphs > kObservedPiuResizeLimitParagraphs)
    {
        RecordDosResize(context,
                        context->guest_es,
                        paragraphs,
                        false,
                        kInsufficientMemory);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | kInsufficientMemory;
        win32_context->Ebx =
            (win32_context->Ebx & 0xFFFF0000U) |
            kObservedPiuResizeLimitParagraphs;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (context->guest_es == kObservedPiuResizeSelector &&
        !context->last_dos_open_success &&
        context->last_dos_open_guest_path == "stage.cfg" &&
        paragraphs > kObservedPiuStageCfgFailureLimitParagraphs)
    {
        RecordDosResize(context,
                        context->guest_es,
                        paragraphs,
                        false,
                        kInsufficientMemory);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | kInsufficientMemory;
        win32_context->Ebx =
            (win32_context->Ebx & 0xFFFF0000U) |
            kObservedPiuStageCfgFailureLimitParagraphs;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosResize(context, context->guest_es, paragraphs, true, 0);
    win32_context->EFlags &= ~1U;
    return true;
}

void HandleDosGetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t vector = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const DosInterruptVectorShadow& entry =
        context->dos_interrupt_vectors[vector];
    const std::uint16_t segment = entry.valid ? entry.segment : 0;
    const std::uint16_t offset = entry.valid ? entry.offset : 0;

    context->guest_es = segment;
    win32_context->SegEs = segment;
    win32_context->Ebx =
        (win32_context->Ebx & 0xFFFF0000U) | offset;
    win32_context->EFlags &= ~1U;
}

void HandleDosSetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t vector = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    DosInterruptVectorShadow& entry =
        context->dos_interrupt_vectors[vector];

    entry.segment = context->guest_ds != 0
        ? context->guest_ds
        : static_cast<std::uint16_t>(win32_context->SegDs);
    entry.offset = static_cast<std::uint16_t>(
        win32_context->Edx & 0xFFFFU);
    entry.valid = true;
    win32_context->EFlags &= ~1U;
}

bool HandleDosInterrupt21(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFF);

    switch (ah)
    {
        case 0x09:
        {
            RecordHandledDosInterrupt(context, 0x21, ax);
            const char* text = reinterpret_cast<const char*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            if (text == nullptr ||
                !IsGuestRangeReadable(context, text, 1))
            {
                win32_context->Eax = 0;
                break;
            }

            std::uint32_t length = 0;
            while (length < 4096 &&
                   IsGuestRangeReadable(context, text, length + 1) &&
                   text[length] != '$')
            {
                ++length;
            }
            AppendConsoleOutput(context, text, length);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFFFF00U) | static_cast<DWORD>('$');
            break;
        }
        case 0x19:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetCurrentDrive(win32_context, context);
            break;
        case 0x30:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x0007U;
            win32_context->Ebx = 0;
            win32_context->Ecx = 0;
            break;
        case 0x25:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetInterruptVector(win32_context, context);
            break;
        case 0x35:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetInterruptVector(win32_context, context);
            break;
        case 0x3B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosChangeDirectory(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3D:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosOpenFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3E:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosCloseFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3F:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosReadFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x40:
        {
            const void* text = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            const std::uint32_t byte_count = win32_context->Ecx;
            AppendConsoleOutput(
                context, text, byte_count, win32_context->Ebx == 2U);
            win32_context->Eax = byte_count;
            win32_context->EFlags &= ~1U;
            break;
        }
        case 0x42:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosSeekFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x43:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosFileAttributes(win32_context, context))
            {
                return false;
            }
            break;
        case 0x44:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosIoctl(win32_context, context))
            {
                return false;
            }
            break;
        case 0x47:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosGetCurrentDirectory(win32_context, context))
            {
                return false;
            }
            break;
        case 0x4C:
            CaptureDosTermination(win32_context, context);
            RecoverFromHleExit(win32_context, context);
            return true;
        case 0x4A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosResizeMemoryBlock(win32_context, context))
            {
                return false;
            }
            break;
        case 0xFF:
            if (ax == kDos4gIdentificationAx &&
                (win32_context->Edx & 0xFFFFU) == kDos4gIdentificationDx &&
                context->linexe_environment_active)
            {
                win32_context->Eax =
                    (win32_context->Eax & 0xFFFF0000U) |
                    kDos4gwIdentificationAxResult;
                context->guest_gs = kDos4gwClientDataSelector;
                if (kDos4gwIdentificationCarry)
                {
                    win32_context->EFlags |= 1U;
                }
            }
            else
            {
                win32_context->Eax &= 0xFFFFFF00U;
                win32_context->EFlags &= ~1U;
            }
            break;
        case 0xED:
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            break;
        default:
        {
            std::ostringstream stream;
            stream << "unsupported DOS INT 21h AH=0x"
                   << std::hex << static_cast<unsigned>(ah);
            context->hle_message = stream.str();
            return false;
        }
    }

    win32_context->Eip += 2;
    return true;
}

bool HandleDosInterrupt2F(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x1686)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x1500)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedIncrement(
                &context->shared_live_telemetry->mscdex_probe_count);
        }
        win32_context->Ebx = (win32_context->Ebx & 0xFFFF0000U) |
            (context->mscdex_available ? 1U : 0U);
        win32_context->Ecx = (win32_context->Ecx & 0xFFFF0000U) |
            (context->mscdex_available ? context->mscdex_drive : 0U);
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x1510)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        const bool called = context->mscdex_available &&
            (win32_context->Ecx & 0xFFFFU) == context->mscdex_drive &&
            HandleMscdexRequest(
                context,
                context->guest_es,
                static_cast<std::uint16_t>(win32_context->Ebx & 0xFFFFU));
        if (called)
        {
            win32_context->EFlags &= ~1U;
        }
        else
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x000FU;
            win32_context->EFlags |= 1U;
        }
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported DOS interrupt 0x2f AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

std::uint8_t* ResolveMscdexBuffer(ThreadContext* context,
                                  std::uint16_t segment,
                                  std::uint16_t offset,
                                  std::uint32_t bytes)
{
    std::uint32_t linear = 0;
    if (ResolveSegmentLinearRange(context, segment, offset, bytes, true,
                                  &linear))
    {
        return reinterpret_cast<std::uint8_t*>(
            static_cast<std::uintptr_t>(linear));
    }
    const std::uint32_t real_linear =
        static_cast<std::uint32_t>(segment) * 16U + offset;
    if (context->dos_low_memory.valid &&
        static_cast<std::uint64_t>(real_linear) + bytes <=
            context->dos_low_memory.bytes.size())
    {
        return context->dos_low_memory.bytes.data() + real_linear;
    }
    return nullptr;
}

std::uint32_t ReadPacketU32(const std::uint8_t* packet,
                            std::size_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, packet + offset, sizeof(value));
    return value;
}

void WritePacketU16(std::uint8_t* packet, std::size_t offset,
                    std::uint16_t value)
{
    std::memcpy(packet + offset, &value, sizeof(value));
}

void WritePacketU32(std::uint8_t* packet, std::size_t offset,
                    std::uint32_t value)
{
    std::memcpy(packet + offset, &value, sizeof(value));
}

void WritePacketMsf3(std::uint8_t* packet, std::size_t offset,
                     std::uint32_t msf)
{
    packet[offset] = static_cast<std::uint8_t>((msf >> 16U) & 0xFFU);
    packet[offset + 1U] = static_cast<std::uint8_t>((msf >> 8U) & 0xFFU);
    packet[offset + 2U] = static_cast<std::uint8_t>(msf & 0xFFU);
}

std::uint32_t MscdexMsfToLba(std::uint32_t msf)
{
    const std::uint32_t minute = (msf >> 16U) & 0xFFU;
    const std::uint32_t second = (msf >> 8U) & 0xFFU;
    const std::uint32_t frame = msf & 0xFFU;
    const std::uint32_t absolute = (minute * 60U + second) * 75U + frame;
    return absolute >= 150U ? absolute - 150U : 0U;
}

std::uint32_t MscdexLbaToMsf(std::uint32_t lba)
{
    lba += 150U;
    return ((lba / (60U * 75U)) << 16U) |
        (((lba / 75U) % 60U) << 8U) | (lba % 75U);
}

bool HandleMscdexIoctl(ThreadContext* context, std::uint8_t* request)
{
    const std::uint16_t offset = static_cast<std::uint16_t>(
        request[14] | (static_cast<std::uint16_t>(request[15]) << 8U));
    const std::uint16_t segment = static_cast<std::uint16_t>(
        request[16] | (static_cast<std::uint16_t>(request[17]) << 8U));
    const std::uint16_t length = static_cast<std::uint16_t>(
        request[18] | (static_cast<std::uint16_t>(request[19]) << 8U));
    std::uint8_t* control = ResolveMscdexBuffer(
        context, segment, offset, std::max<std::uint16_t>(length, 16U));
    if (control == nullptr)
    {
        return false;
    }
    const auto& tracks = context->cd_image.tracks();
    switch (control[0])
    {
        case 6:  // device status
            if (length < 5U) return false;
            WritePacketU32(control, 1, 0x00000290U);
            return true;
        case 9:  // media changed
            if (length < 2U) return false;
            control[1] = 1U;
            return true;
        case 10:  // audio disc information
            if (length < 7U || tracks.empty()) return false;
            control[1] = tracks.front().number;
            control[2] = tracks.back().number;
            WritePacketU32(control, 3,
                           MscdexLbaToMsf(context->cd_image.lead_out_lba()));
            return true;
        case 11:  // audio track information
        {
            if (length < 7U) return false;
            const repiu::media::ChdCdTrack* track =
                context->cd_image.FindTrack(control[1]);
            if (track == nullptr) return false;
            WritePacketU32(control, 2, MscdexLbaToMsf(track->start_lba));
            control[6] = track->audio ? 0U : 0x40U;
            return true;
        }
        case 12:  // Q-channel information
        {
            if (length < 11U) return false;
            const std::uint32_t lba = context->cd_audio.current_lba();
            const repiu::media::ChdCdTrack* track =
                context->cd_image.FindTrackByLba(lba);
            control[1] = track != nullptr && !track->audio ? 0x40U : 0U;
            control[2] = track != nullptr ? track->number : 0U;
            control[3] = 1U;
            WritePacketMsf3(control, 4, MscdexLbaToMsf(
                track != nullptr ? lba - track->start_lba : 0U));
            control[7] = 0U;
            WritePacketMsf3(control, 8, MscdexLbaToMsf(lba));
            return true;
        }
        case 15:  // audio status
            if (length < 11U) return false;
            WritePacketU16(control, 1,
                context->cd_audio.playing() ? 0U : 1U);
            WritePacketU32(control, 3,
                           MscdexLbaToMsf(context->cd_audio.current_lba()));
            WritePacketU32(control, 7, 0U);
            return true;
        default:
            return false;
    }
}

bool HandleMscdexRequest(ThreadContext* context,
                         std::uint16_t segment,
                         std::uint16_t offset)
{
    std::uint8_t* request = ResolveMscdexBuffer(context, segment, offset, 26U);
    if (request == nullptr || request[0] < 13U)
    {
        return false;
    }
    ++context->mscdex_request_count;
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_request_count,
            static_cast<long>(context->mscdex_request_count));
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_last_command,
            static_cast<long>(request[2]));
    }
    bool success = false;
    switch (request[2])
    {
        case 0x03:
            success = HandleMscdexIoctl(context, request);
            break;
        case 0x84:
        {
            const std::uint32_t start = request[13] == 1U
                ? MscdexMsfToLba(ReadPacketU32(request, 14))
                : ReadPacketU32(request, 14);
            success = context->cd_audio_available &&
                context->cd_audio.Play(start, ReadPacketU32(request, 18));
            break;
        }
        case 0x85:
            context->cd_audio.Stop();
            success = true;
            break;
        case 0x88:
            success = context->cd_audio.Resume();
            break;
        default:
            break;
    }
    WritePacketU16(request, 3,
                   success ? 0x0100U : 0x8103U);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_last_status,
            success ? 0x0100L : 0x8103L);
    }
    return true;
}

bool HandleDpmiInterrupt31(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x0000)
    {
        const std::uint16_t count = static_cast<std::uint16_t>(
            win32_context->Ecx & 0xFFFFU);
        ++context->dpmi_allocate_call_count;
        context->dpmi_last_allocate_requested_count = count;
        std::uint16_t first_selector = 0;
        bool success = count != 0;
        for (std::uint32_t index = 0; success && index < count; ++index)
        {
            std::uint16_t selector = 0;
            success = repiu::runtime::AllocateSelector(
                &context->dpmi_selector_allocator, &selector) &&
                repiu::runtime::RegisterDescriptor(
                    &context->selector_table,
                    {selector, 0, 0, 0x0092U, true});
            if (index == 0)
            {
                first_selector = selector;
            }
        }
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (success)
        {
            context->dpmi_last_allocated_selector = first_selector;
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | first_selector;
            win32_context->EFlags &= ~1U;
        }
        else
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8011U;
            win32_context->EFlags |= 1U;
        }
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0300 && (win32_context->Ebx & 0xFFU) == 0x2FU)
    {
        constexpr std::size_t kRealModeFrameBytes = 0x34U;
        constexpr std::size_t kFrameEbxOffset = 0x10U;
        constexpr std::size_t kFrameEcxOffset = 0x18U;
        constexpr std::size_t kFrameEaxOffset = 0x1CU;
        constexpr std::size_t kFrameFlagsOffset = 0x20U;
        constexpr std::size_t kFrameEsOffset = 0x24U;
        void* frame = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
            win32_context->Edi));
        if (!context->fatal_breakpoint_continued)
        {
            if (!IsGuestRangeWritable(
                    context, frame, kRealModeFrameBytes))
            {
                return false;
            }
            auto* bytes = static_cast<std::uint8_t*>(frame);
            std::uint32_t frame_eax = 0;
            std::uint32_t frame_ebx = 0;
            std::uint32_t frame_ecx = 0;
            std::uint16_t frame_es = 0;
            std::memcpy(&frame_eax, bytes + kFrameEaxOffset,
                        sizeof(frame_eax));
            std::memcpy(&frame_ebx, bytes + kFrameEbxOffset,
                        sizeof(frame_ebx));
            std::memcpy(&frame_ecx, bytes + kFrameEcxOffset,
                        sizeof(frame_ecx));
            std::memcpy(&frame_es, bytes + kFrameEsOffset,
                        sizeof(frame_es));
            if (context->shared_live_telemetry != nullptr)
            {
                InterlockedExchange(
                    &context->shared_live_telemetry->dpmi_frame_eax,
                    static_cast<long>(frame_eax));
                InterlockedExchange(
                    &context->shared_live_telemetry->dpmi_frame_ebx,
                    static_cast<long>(frame_ebx));
                InterlockedExchange(
                    &context->shared_live_telemetry->dpmi_frame_ecx,
                    static_cast<long>(frame_ecx));
            }
            const std::uint16_t frame_ax = static_cast<std::uint16_t>(
                frame_eax & 0xFFFFU);
            if (frame_ax == 0x1500U)
            {
                frame_ebx = (frame_ebx & 0xFFFF0000U) |
                    (context->mscdex_available ? 1U : 0U);
                frame_ecx = (frame_ecx & 0xFFFF0000U) |
                    (context->mscdex_available ? context->mscdex_drive : 0U);
                std::memcpy(bytes + kFrameEbxOffset, &frame_ebx,
                            sizeof(frame_ebx));
                std::memcpy(bytes + kFrameEcxOffset, &frame_ecx,
                            sizeof(frame_ecx));
            }
            else if (frame_ax == 0x1510U)
            {
                std::uint32_t frame_flags = 0;
                std::memcpy(&frame_flags, bytes + kFrameFlagsOffset,
                            sizeof(frame_flags));
                const bool called = context->mscdex_available &&
                    (frame_ecx & 0xFFFFU) == context->mscdex_drive &&
                    HandleMscdexRequest(
                        context, frame_es,
                        static_cast<std::uint16_t>(frame_ebx & 0xFFFFU));
                if (called)
                {
                    frame_flags &= ~1U;
                }
                else
                {
                    frame_eax = (frame_eax & 0xFFFF0000U) | 0x000FU;
                    frame_flags |= 1U;
                }
                std::memcpy(bytes + kFrameEaxOffset, &frame_eax,
                            sizeof(frame_eax));
                std::memcpy(bytes + kFrameFlagsOffset, &frame_flags,
                            sizeof(frame_flags));
            }
            else
            {
                return false;
            }
        }
        RecordHandledDosInterrupt(context, 0x31, ax);
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0006)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const repiu::runtime::GuestDescriptor* descriptor =
            repiu::runtime::FindDescriptor(context->selector_table, selector);
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (descriptor == nullptr || !descriptor->present)
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        else
        {
            win32_context->Ecx =
                (win32_context->Ecx & 0xFFFF0000U) |
                ((descriptor->base >> 16) & 0xFFFFU);
            win32_context->Edx =
                (win32_context->Edx & 0xFFFF0000U) |
                (descriptor->base & 0xFFFFU);
            win32_context->EFlags &= ~1U;
        }
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0007)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const repiu::runtime::GuestDescriptor* existing =
            repiu::runtime::FindDescriptor(context->selector_table, selector);
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (existing == nullptr || !existing->present)
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        else
        {
            repiu::runtime::GuestDescriptor updated = *existing;
            updated.base = ((win32_context->Ecx & 0xFFFFU) << 16) |
                           (win32_context->Edx & 0xFFFFU);
            if (!repiu::runtime::RegisterDescriptor(
                    &context->selector_table, updated))
            {
                return false;
            }
            win32_context->EFlags &= ~1U;
        }
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0008 || ax == 0x0009)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const repiu::runtime::GuestDescriptor* existing =
            repiu::runtime::FindDescriptor(context->selector_table, selector);
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (existing == nullptr || !existing->present)
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        else
        {
            repiu::runtime::GuestDescriptor updated = *existing;
            if (ax == 0x0008)
            {
                updated.limit = ((win32_context->Ecx & 0xFFFFU) << 16) |
                                (win32_context->Edx & 0xFFFFU);
            }
            else
            {
                updated.flags = win32_context->Ecx & 0xFFFFU;
            }
            if (!repiu::runtime::RegisterDescriptor(
                    &context->selector_table, updated))
            {
                return false;
            }
            win32_context->EFlags &= ~1U;
        }
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0400)
    {
        RecordHandledDosInterrupt(context, 0x31, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported DPMI INT 31h AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

bool HandleMouseInterrupt33(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x0000)
    {
        RecordHandledDosInterrupt(context, 0x33, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->Ebx &= 0xFFFF0000U;
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0002)
    {
        RecordHandledDosInterrupt(context, 0x33, ax);
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported mouse INT 33h AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

void RecordHandledHleTrap(CONTEXT* win32_context,
                          ThreadContext* context,
                          std::uint8_t opcode)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_hle_trap_count;
    context->last_hle_trap_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_hle_trap_opcode = opcode;
}

void RecordHandledDosInterrupt(ThreadContext* context,
                               std::uint8_t vector,
                               std::uint16_t ax)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_interrupt_count;
    context->last_dos_interrupt_vector = vector;
    context->last_dos_interrupt_ah = static_cast<std::uint8_t>(ax >> 8);
    context->last_dos_interrupt_ax = ax;
}

std::uint16_t ReadRegister16(const CONTEXT& win32_context,
                             std::uint8_t register_id)
{
    switch (register_id & 0x07)
    {
        case 0:
            return static_cast<std::uint16_t>(win32_context.Eax & 0xFFFFU);
        case 1:
            return static_cast<std::uint16_t>(win32_context.Ecx & 0xFFFFU);
        case 2:
            return static_cast<std::uint16_t>(win32_context.Edx & 0xFFFFU);
        case 3:
            return static_cast<std::uint16_t>(win32_context.Ebx & 0xFFFFU);
        case 4:
            return static_cast<std::uint16_t>(win32_context.Esp & 0xFFFFU);
        case 5:
            return static_cast<std::uint16_t>(win32_context.Ebp & 0xFFFFU);
        case 6:
            return static_cast<std::uint16_t>(win32_context.Esi & 0xFFFFU);
        case 7:
            return static_cast<std::uint16_t>(win32_context.Edi & 0xFFFFU);
        default:
            return 0;
    }
}

void WriteRegister16(CONTEXT* win32_context,
                     std::uint8_t register_id,
                     std::uint16_t value)
{
    switch (register_id & 0x07)
    {
        case 0:
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | value;
            break;
        case 1:
            win32_context->Ecx =
                (win32_context->Ecx & 0xFFFF0000U) | value;
            break;
        case 2:
            win32_context->Edx =
                (win32_context->Edx & 0xFFFF0000U) | value;
            break;
        case 3:
            win32_context->Ebx =
                (win32_context->Ebx & 0xFFFF0000U) | value;
            break;
        case 4:
            win32_context->Esp =
                (win32_context->Esp & 0xFFFF0000U) | value;
            break;
        case 5:
            win32_context->Ebp =
                (win32_context->Ebp & 0xFFFF0000U) | value;
            break;
        case 6:
            win32_context->Esi =
                (win32_context->Esi & 0xFFFF0000U) | value;
            break;
        case 7:
            win32_context->Edi =
                (win32_context->Edi & 0xFFFF0000U) | value;
            break;
        default:
            break;
    }
}

std::uint8_t ReadRegister8(const CONTEXT& win32_context,
                           std::uint8_t register_index)
{
    const std::uint32_t registers[4] = {
        win32_context.Eax,
        win32_context.Ecx,
        win32_context.Edx,
        win32_context.Ebx,
    };
    const std::uint8_t base = register_index & 0x03U;
    const std::uint32_t shift = register_index < 4 ? 0U : 8U;
    return static_cast<std::uint8_t>((registers[base] >> shift) & 0xFFU);
}

void WriteRegister8(CONTEXT* win32_context,
                    std::uint8_t register_index,
                    std::uint8_t value)
{
    DWORD* registers[4] = {
        &win32_context->Eax,
        &win32_context->Ecx,
        &win32_context->Edx,
        &win32_context->Ebx,
    };
    const std::uint8_t base = register_index & 0x03U;
    const std::uint32_t shift = register_index < 4 ? 0U : 8U;
    const std::uint32_t mask = 0xFFU << shift;
    *registers[base] =
        (*registers[base] & ~mask) |
        (static_cast<std::uint32_t>(value) << shift);
}

void SetCompareFlags8(CONTEXT* win32_context,
                      std::uint8_t lhs,
                      std::uint8_t rhs)
{
    constexpr std::uint32_t kArithmeticFlags =
        0x000008D5U;
    const std::uint8_t result = static_cast<std::uint8_t>(lhs - rhs);
    std::uint32_t flags = win32_context->EFlags & ~kArithmeticFlags;
    if (lhs < rhs)
    {
        flags |= 0x00000001U;
    }
    std::uint8_t parity = result;
    parity ^= static_cast<std::uint8_t>(parity >> 4U);
    parity ^= static_cast<std::uint8_t>(parity >> 2U);
    parity ^= static_cast<std::uint8_t>(parity >> 1U);
    if ((parity & 1U) == 0)
    {
        flags |= 0x00000004U;
    }
    if (((lhs ^ rhs ^ result) & 0x10U) != 0)
    {
        flags |= 0x00000010U;
    }
    if (result == 0)
    {
        flags |= 0x00000040U;
    }
    if ((result & 0x80U) != 0)
    {
        flags |= 0x00000080U;
    }
    if (((lhs ^ rhs) & (lhs ^ result) & 0x80U) != 0)
    {
        flags |= 0x00000800U;
    }
    win32_context->EFlags = flags;
}

void RecordGuestSegmentLoad(CONTEXT* win32_context,
                            ThreadContext* context,
                            std::uint8_t segment_register,
                            std::uint16_t selector,
                            std::uint32_t source)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    if (segment_register == 5 && selector == kDos4gwLinexeDataSelector)
    {
        ++context->linexe_data_gs_load_count;
    }

    ++context->handled_segment_load_count;
    context->last_segment_load_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_load_opcode = 0x8E;
    context->last_segment_load_register = segment_register;
    context->last_segment_load_selector = selector;
    context->last_segment_load_source = source;
    Win32SegmentLoadObservation& observation = context->segment_load;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32SegmentLoadTraceCapacity;
    Win32SegmentLoadTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.eip_offset =
        static_cast<std::uint32_t>(win32_context->Eip) >=
                context->runtime_base
            ? static_cast<std::uint32_t>(win32_context->Eip) -
                  context->runtime_base
            : static_cast<std::uint32_t>(win32_context->Eip);
    entry.segment_register = segment_register;
    entry.selector = selector;
    entry.source = source;
    observation.observed_count = sequence;
    if (observation.trace_stored_count < kWin32SegmentLoadTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_wrapped = true;
    }
    if (selector != 0 &&
        repiu::runtime::FindDescriptor(
            context->selector_table, selector) == nullptr)
    {
        repiu::runtime::RegisterDescriptor(
            &context->selector_table,
            repiu::runtime::GuestDescriptor{
                selector,
                0,
                repiu::runtime::kDosLowMemorySize - 1U,
                0,
                true,
            });
    }

    switch (segment_register)
    {
        case 0:
            context->guest_es = selector;
            break;
        case 2:
            context->guest_ss = selector;
            break;
        case 3:
            context->guest_ds = selector;
            break;
        case 4:
            context->guest_fs = selector;
            break;
        case 5:
            context->guest_gs = selector;
            break;
        default:
            break;
    }
}

std::uint16_t ReadGuestSegmentSelector(const ThreadContext& context,
                                       std::uint8_t segment_register)
{
    switch (segment_register)
    {
        case 0:
            return context.guest_es;
        case 2:
            return context.guest_ss;
        case 3:
            return context.guest_ds;
        case 4:
            return context.guest_fs;
        case 5:
            return context.guest_gs;
        default:
            return 0;
    }
}

void RecordGuestSegmentStore(CONTEXT* win32_context,
                             ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t destination)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_store_count;
    context->last_segment_store_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_store_opcode = 0x8C;
    context->last_segment_store_register = segment_register;
    context->last_segment_store_selector = selector;
    context->last_segment_store_destination = destination;
}

void RecordGuestSegmentMemoryLoad(CONTEXT* win32_context,
                                  ThreadContext* context,
                                  std::uint8_t opcode,
                                  std::uint8_t segment_register,
                                  std::uint16_t selector,
                                  std::uint32_t offset,
                                  std::uint32_t byte_width,
                                  std::uint32_t value)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_memory_load_count;
    context->last_segment_memory_load_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_memory_load_opcode = opcode;
    context->last_segment_memory_load_register = segment_register;
    context->last_segment_memory_load_selector = selector;
    context->last_segment_memory_load_offset = offset;
    context->last_segment_memory_load_width = byte_width;
    context->last_segment_memory_load_value = value;
}

void RecordGuestMemoryStore(CONTEXT* win32_context,
                            ThreadContext* context,
                            std::uint32_t opcode,
                            std::uint32_t destination,
                            std::uint32_t value,
                            std::uint32_t byte_width,
                            const char* source_kind,
                            bool applied)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_memory_store_count;
    context->diagnostic_progress_count.fetch_add(
        1,
        std::memory_order_relaxed);
    context->last_memory_store_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_memory_store_opcode = opcode;
    context->last_memory_store_destination = destination;
    context->last_memory_store_value = value;
    context->last_memory_store_width = byte_width;
    context->last_memory_store_source_kind =
        source_kind != nullptr ? source_kind : "unknown";
    context->last_memory_store_applied = applied;
}

std::uint32_t ReadGeneralRegister32(const CONTEXT* win32_context,
                                    std::uint8_t register_index)
{
    switch (register_index & 0x07U)
    {
        case 0:
            return win32_context->Eax;
        case 1:
            return win32_context->Ecx;
        case 2:
            return win32_context->Edx;
        case 3:
            return win32_context->Ebx;
        case 4:
            return win32_context->Esp;
        case 5:
            return win32_context->Ebp;
        case 6:
            return win32_context->Esi;
        case 7:
            return win32_context->Edi;
        default:
            return 0;
    }
}

void WriteGeneralRegister32(CONTEXT* win32_context,
                            std::uint8_t register_index,
                            std::uint32_t value)
{
    switch (register_index & 0x07U)
    {
        case 0:
            win32_context->Eax = value;
            break;
        case 1:
            win32_context->Ecx = value;
            break;
        case 2:
            win32_context->Edx = value;
            break;
        case 3:
            win32_context->Ebx = value;
            break;
        case 4:
            win32_context->Esp = value;
            break;
        case 5:
            win32_context->Ebp = value;
            break;
        case 6:
            win32_context->Esi = value;
            break;
        case 7:
            win32_context->Edi = value;
            break;
        default:
            break;
    }
}

bool DecodeModRmMemoryAddress(
    const CONTEXT* win32_context,
    const std::uint8_t* instruction,
    std::uint32_t* destination,
    std::uint32_t* instruction_size)
{
    const std::uint8_t modrm = instruction[1];
    const std::uint8_t mod = (modrm >> 6) & 0x03U;
    const std::uint8_t rm = modrm & 0x07U;
    if (mod == 0x03)
    {
        return false;
    }

    std::uint32_t base = 0;
    std::uint32_t index = 0;
    std::uint32_t displacement = 0;
    std::uint32_t size = 2;
    std::uint32_t displacement_offset = 2;
    if (rm == 0x04)
    {
        const std::uint8_t sib = instruction[2];
        const std::uint8_t scale = (sib >> 6) & 0x03U;
        const std::uint8_t index_register = (sib >> 3) & 0x07U;
        const std::uint8_t base_register = sib & 0x07U;
        if (index_register != 0x04)
        {
            index = ReadGeneralRegister32(win32_context, index_register)
                    << scale;
        }
        if (!(mod == 0x00 && base_register == 0x05))
        {
            base = ReadGeneralRegister32(win32_context, base_register);
        }
        displacement_offset = 3;
        size = 3;
    }
    else if (!(mod == 0x00 && rm == 0x05))
    {
        base = ReadGeneralRegister32(win32_context, rm);
    }

    const bool absolute_displacement =
        mod == 0x00 &&
        (rm == 0x05 ||
         (rm == 0x04 && (instruction[2] & 0x07U) == 0x05));
    if (absolute_displacement || mod == 0x02)
    {
        displacement =
            static_cast<std::uint32_t>(instruction[displacement_offset]) |
            (static_cast<std::uint32_t>(instruction[displacement_offset + 1]) << 8) |
            (static_cast<std::uint32_t>(instruction[displacement_offset + 2]) << 16) |
            (static_cast<std::uint32_t>(instruction[displacement_offset + 3]) << 24);
        size = displacement_offset + 4;
    }
    else if (mod == 0x01)
    {
        displacement = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(
                static_cast<std::int8_t>(instruction[displacement_offset])));
        size = displacement_offset + 1;
    }

    *destination = base + index + displacement;
    *instruction_size = size;
    return true;
}

bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context);
bool HandleSegmentPopInstruction(CONTEXT* win32_context,
                                 ThreadContext* context);

bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context);

bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context);

bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);

bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context);

bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);

bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context);

bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint32_t prefix_length = 0;
    while (instruction[prefix_length] == 0x26 ||
           instruction[prefix_length] == 0x2E ||
           instruction[prefix_length] == 0x36 ||
           instruction[prefix_length] == 0x3E ||
           instruction[prefix_length] == 0x64 ||
           instruction[prefix_length] == 0x65 ||
           instruction[prefix_length] == 0x66 ||
           instruction[prefix_length] == 0x67)
    {
        ++prefix_length;
    }

    if (instruction[prefix_length] != 0x8E)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[prefix_length + 1];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t segment_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t source_register =
        static_cast<std::uint8_t>(modrm & 0x07);

    if (segment_register == 1 || segment_register > 5)
    {
        return false;
    }

    std::uint16_t selector = 0;
    std::uint32_t source = 0;
    std::uint32_t instruction_length = prefix_length + 2;
    if (mod == 0x03)
    {
        selector = ReadRegister16(*win32_context, source_register);
    }
    else
    {
        std::uint32_t unprefixed_length = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + prefix_length,
                                      &source,
                                      &unprefixed_length))
        {
            return false;
        }
        const void* source_pointer = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(source));
        if (!IsGuestRangeReadable(context, source_pointer, 2))
        {
            return false;
        }

        std::memcpy(&selector, source_pointer, sizeof(selector));
        instruction_length = prefix_length + unprefixed_length;
    }

    RecordGuestSegmentLoad(win32_context,
                           context,
                           segment_register,
                           selector,
                           source);
    win32_context->Eip += instruction_length;
    HandleSegmentLoadInstruction(win32_context, context);
    HandleSegmentStoreInstruction(win32_context, context);
    return true;
}

bool HandleSegmentPopInstruction(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x1F)
    {
        return false;
    }

    const std::uint32_t source =
        static_cast<std::uint32_t>(win32_context->Esp);
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    if (!IsGuestRangeReadable(context, source_pointer, 2))
    {
        return false;
    }

    std::uint16_t selector = 0;
    std::memcpy(&selector, source_pointer, sizeof(selector));
    RecordGuestSegmentLoad(win32_context,
                           context,
                           3,
                           selector,
                           source);
    win32_context->Esp += 4;
    ++win32_context->Eip;
    return true;
}

bool HandleRepStosdInstruction(CONTEXT* win32_context,
                               ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xF3 || instruction[1] != 0xAB ||
        win32_context->Eax != 0 ||
        (win32_context->EFlags & 0x00000400U) != 0)
    {
        return false;
    }

    const std::uint64_t byte_count =
        static_cast<std::uint64_t>(win32_context->Ecx) * 4U;
    if (byte_count > 0xFFFFFFFFULL)
    {
        return false;
    }
    void* destination = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(win32_context->Edi));
    if (byte_count != 0 &&
        !IsGuestRangeWritable(context,
                              destination,
                              static_cast<std::uint32_t>(byte_count)))
    {
        return false;
    }

    if (byte_count != 0)
    {
        std::memset(destination, 0, static_cast<std::size_t>(byte_count));
    }
    win32_context->Edi += static_cast<std::uint32_t>(byte_count);
    win32_context->Ecx = 0;
    win32_context->Eip += 2;
    context->diagnostic_progress_count.fetch_add(
        1,
        std::memory_order_relaxed);
    return true;
}

bool HandleLodsbInstruction(CONTEXT* win32_context,
                            ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xAC)
    {
        return false;
    }
    std::uint8_t value = 0;
    if (!ReadSegmentByte(context, 3, context->guest_ds,
                         win32_context->Esi, &value))
    {
        return false;
    }
    win32_context->Eax =
        (win32_context->Eax & 0xFFFFFF00U) | value;
    win32_context->Esi +=
        (win32_context->EFlags & 0x00000400U) != 0 ? -1 : 1;
    ++win32_context->Eip;
    return true;
}

bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] == 0x66 && instruction[1] == 0x8C)
    {
        const std::uint8_t modrm = instruction[2];
        const std::uint8_t mod = (modrm >> 6) & 0x03U;
        const std::uint8_t segment_register = (modrm >> 3) & 0x07U;
        const std::uint8_t destination_register = modrm & 0x07U;
        if (mod != 0x03 || segment_register == 1 ||
            segment_register > 5)
        {
            return false;
        }
        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register);
        WriteRegister16(win32_context, destination_register, selector);
        RecordGuestSegmentStore(win32_context,
                                context,
                                segment_register,
                                selector,
                                destination_register);
        win32_context->Eip += 3;
        return true;
    }
    if (instruction[0] == 0x8C)
    {
        const std::uint8_t modrm = instruction[1];
        const std::uint8_t mod =
            static_cast<std::uint8_t>((modrm >> 6) & 0x03);
        const std::uint8_t segment_register =
            static_cast<std::uint8_t>((modrm >> 3) & 0x07);
        if (segment_register == 1 || segment_register > 5)
        {
            return false;
        }

        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register);
        if (mod == 0x03)
        {
            const std::uint8_t destination_register =
                static_cast<std::uint8_t>(modrm & 0x07);
            WriteRegister16(win32_context,
                            destination_register,
                            selector);
            RecordGuestSegmentStore(win32_context,
                                    context,
                                    segment_register,
                                    selector,
                                    destination_register);
            win32_context->Eip += 2;
            return true;
        }

        std::uint32_t destination = 0;
        std::uint32_t instruction_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction,
                                      &destination,
                                      &instruction_size))
        {
            return false;
        }
        void* destination_pointer = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
        if (!IsGuestRangeWritable(context, destination_pointer, 2) ||
            !WriteGuestUInt16(context, destination_pointer, selector))
        {
            return false;
        }
        RecordGuestSegmentStore(win32_context,
                                context,
                                segment_register,
                                selector,
                                destination);
        win32_context->Eip += instruction_size;
        return true;
    }
    if (instruction[0] != 0x66 || instruction[1] != 0x26 ||
        instruction[2] != 0x8C)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[3];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t segment_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t rm = static_cast<std::uint8_t>(modrm & 0x07);
    if (mod != 0x00 || rm != 0x05)
    {
        return false;
    }

    if (segment_register == 1 || segment_register > 5)
    {
        return false;
    }

    const std::uint32_t destination =
        static_cast<std::uint32_t>(instruction[4]) |
        (static_cast<std::uint32_t>(instruction[5]) << 8) |
        (static_cast<std::uint32_t>(instruction[6]) << 16) |
        (static_cast<std::uint32_t>(instruction[7]) << 24);
    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    if (!IsGuestRangeWritable(context, destination_pointer, 2))
    {
        return false;
    }

    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);
    if (!WriteGuestUInt16(context, destination_pointer, selector))
    {
        return false;
    }

    RecordGuestSegmentStore(win32_context,
                            context,
                            segment_register,
                            selector,
                            destination);
    win32_context->Eip += 8;
    return true;
}

bool ReadSegmentOverrideByte(ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t offset,
                             std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (segment_register == 0 && selector == context->guest_es &&
        selector != 0 && offset == 0x80)
    {
        *value = 0;
        return true;
    }

    return false;
}

bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if ((instruction[0] == 0x64 || instruction[0] == 0x65) &&
        instruction[1] == 0x8A)
    {
        std::uint32_t offset = 0;
        std::uint32_t unprefixed_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + 1,
                                      &offset,
                                      &unprefixed_size))
        {
            return false;
        }
        const std::uint8_t segment_register =
            instruction[0] == 0x64 ? 4 : 5;
        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register);
        std::uint8_t value = 0;
        if (!ReadSegmentByte(context,
                             segment_register,
                             selector,
                             offset,
                             &value))
        {
            return false;
        }
        if (segment_register == 5)
        {
            if (context->linexe_gs_byte_load_count == 0)
            {
                context->linexe_first_gs_byte_offset = offset;
                context->linexe_first_gs_byte_value = value;
            }
            ++context->linexe_gs_byte_load_count;
        }
        const std::uint8_t destination_register =
            (instruction[2] >> 3) & 0x07U;
        WriteRegister8(win32_context, destination_register, value);
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     instruction[1],
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        win32_context->Eip += 1 + unprefixed_size;
        return true;
    }
    if (instruction[0] == 0x26 &&
        (instruction[1] == 0x8A || instruction[1] == 0x3A))
    {
        const std::uint8_t modrm = instruction[2];
        const std::uint8_t mod =
            static_cast<std::uint8_t>((modrm >> 6) & 0x03);
        const std::uint8_t register_index =
            static_cast<std::uint8_t>((modrm >> 3) & 0x07);
        const std::uint8_t base_register =
            static_cast<std::uint8_t>(modrm & 0x07);
        if (mod == 0 && base_register == 0)
        {
            const std::uint8_t segment_register = 0;
            const std::uint16_t selector =
                ReadGuestSegmentSelector(*context, segment_register);
            const std::uint32_t offset = win32_context->Eax;
            std::uint8_t value = 0;
            if (!ReadSegmentByte(
                    context,
                    segment_register,
                    selector,
                    offset,
                    &value))
            {
                return false;
            }

            if (instruction[1] == 0x8A)
            {
                WriteRegister8(win32_context, register_index, value);
            }
            else
            {
                SetCompareFlags8(
                    win32_context,
                    ReadRegister8(*win32_context, register_index),
                    value);
            }
            RecordGuestSegmentMemoryLoad(win32_context,
                                         context,
                                         instruction[1],
                                         segment_register,
                                         selector,
                                         offset,
                                         1,
                                         value);
            win32_context->Eip += 3;
            return true;
        }
    }
    if (instruction[0] == 0x26 && instruction[1] == 0x80 &&
        instruction[2] == 0x38)
    {
        const std::uint8_t segment_register = 0;
        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register);
        const std::uint32_t offset = win32_context->Eax;
        std::uint8_t value = 0;
        if (!ReadSegmentByte(context,
                             segment_register,
                             selector,
                             offset,
                             &value))
        {
            return false;
        }
        SetCompareFlags8(win32_context, value, instruction[3]);
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     instruction[1],
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        win32_context->Eip += 4;
        return true;
    }
    if (instruction[0] != 0x26 || instruction[1] != 0x8A ||
        instruction[2] != 0x4F)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[2];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t destination_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t base_register =
        static_cast<std::uint8_t>(modrm & 0x07);
    if (mod != 0x01 || destination_register != 0x01 ||
        base_register != 0x07)
    {
        return false;
    }

    const std::int8_t displacement =
        static_cast<std::int8_t>(instruction[3]);
    const std::uint32_t offset =
        static_cast<std::uint32_t>(win32_context->Edi + displacement);
    const std::uint8_t segment_register = 0;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);

    std::uint8_t value = 0;
    if (!ReadSegmentOverrideByte(
            context, segment_register, selector, offset, &value))
    {
        return false;
    }

    win32_context->Ecx =
        (win32_context->Ecx & 0xFFFFFF00U) | value;
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x8A,
                                 segment_register,
                                 selector,
                                 offset,
                                 1,
                                 value);
    win32_context->Eip += 4;
    return true;
}

void RecordDosEnvironmentAccess(ThreadContext* context, std::uint32_t offset)
{
    if (context == nullptr ||
        offset >= context->dos_environment_block.size())
    {
        return;
    }

    const auto& block = context->dos_environment_block;
    std::size_t cursor = 0;
    while (cursor < block.size() && block[cursor] != 0)
    {
        const std::size_t entry_begin = cursor;
        while (cursor < block.size() && block[cursor] != 0)
        {
            ++cursor;
        }
        const std::size_t entry_end = cursor;

        if (offset >= entry_begin && offset <= entry_end)
        {
            std::size_t equals = entry_begin;
            while (equals < entry_end && block[equals] != '=')
            {
                ++equals;
            }

            const std::size_t name_end =
                equals < entry_end ? equals : entry_end;
            const std::size_t value_begin =
                equals < entry_end ? equals + 1 : entry_end;
            context->last_dos_environment_access_valid = true;
            context->last_dos_environment_access_offset = offset;
            context->last_dos_environment_entry_offset =
                static_cast<std::uint32_t>(entry_begin);
            context->last_dos_environment_value_length =
                static_cast<std::uint32_t>(entry_end - value_begin);
            if (name_end > entry_begin)
            {
                context->last_dos_environment_entry_name.assign(
                    reinterpret_cast<const char*>(&block[entry_begin]),
                    name_end - entry_begin);
            }
            else
            {
                context->last_dos_environment_entry_name = "<unnamed>";
            }
            context->diagnostic_progress_count.fetch_add(
                1,
                std::memory_order_relaxed);
            return;
        }

        if (cursor < block.size())
        {
            ++cursor;
        }
    }
}

bool ReadSegmentDword(ThreadContext* context,
                      std::uint8_t segment_register,
                      std::uint16_t selector,
                      std::uint32_t offset,
                      std::uint32_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }
    if (selector == 0U &&
        offset <= repiu::runtime::kDosLowMemorySize - sizeof(*value))
    {
        return repiu::runtime::ReadDosLowMemoryUInt32(
            context->dos_low_memory, offset, value);
    }

    if (segment_register == 3 && selector == context->guest_ds &&
        selector != 0 && offset < 0x10000)
    {
        RecordDosEnvironmentAccess(context, offset);
        std::uint32_t result = 0;
        for (std::uint32_t index = 0; index < 4; ++index)
        {
            std::uint8_t byte = 0;
            const std::uint32_t byte_offset = offset + index;
            if (byte_offset >= offset &&
                byte_offset < context->dos_environment_block.size())
            {
                byte = context->dos_environment_block[byte_offset];
            }
            result |= static_cast<std::uint32_t>(byte) << (index * 8);
        }
        *value = result;
        return true;
    }

    std::uint32_t linear_address = 0;
    if (!repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            sizeof(*value),
            &linear_address))
    {
        return false;
    }
    if (linear_address < repiu::runtime::kDosLowMemorySize)
    {
        return repiu::runtime::ReadDosLowMemoryUInt32(
            context->dos_low_memory,
            linear_address,
            value);
    }
    const void* source = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(linear_address));
    if (!IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }
    std::memcpy(value, source, sizeof(*value));
    return true;
}

bool ReadSegmentByte(ThreadContext* context,
                     std::uint8_t segment_register,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (selector == 0U && offset < repiu::runtime::kDosLowMemorySize)
    {
        return repiu::runtime::ReadDosLowMemoryUInt8(
            context->dos_low_memory, offset, value);
    }

    if (segment_register == 3 && selector == context->guest_ds &&
        selector != 0 && offset < 0x10000)
    {
        RecordDosEnvironmentAccess(context, offset);
        if (offset < context->dos_environment_block.size())
        {
            *value = context->dos_environment_block[offset];
        }
        else
        {
            *value = 0;
        }
        return true;
    }

    std::uint32_t linear_address = 0;
    if (!repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            sizeof(*value),
            &linear_address))
    {
        return false;
    }
    if (linear_address < repiu::runtime::kDosLowMemorySize)
    {
        return repiu::runtime::ReadDosLowMemoryUInt8(
            context->dos_low_memory,
            linear_address,
            value);
    }
    const void* source = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(linear_address));
    if (!IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }
    std::memcpy(value, source, sizeof(*value));
    return true;
}

bool ReadSegmentWord(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint16_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (selector == 0U &&
        offset <= repiu::runtime::kDosLowMemorySize - sizeof(*value))
    {
        return repiu::runtime::ReadDosLowMemoryUInt16(
            context->dos_low_memory, offset, value);
    }

    std::uint32_t linear_address = 0;
    if (repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            sizeof(*value),
            &linear_address))
    {
        if (linear_address < repiu::runtime::kDosLowMemorySize)
        {
            return repiu::runtime::ReadDosLowMemoryUInt16(
                context->dos_low_memory,
                linear_address,
                value);
        }
        const void* source = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(linear_address));
        if (!IsGuestRangeReadable(context, source, sizeof(*value)))
        {
            return false;
        }
        std::memcpy(value, source, sizeof(*value));
        return true;
    }

    return false;
}

bool HandleSegmentOverrideMemoryLoadInstruction(CONTEXT* win32_context,
                                                ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint32_t prefix_length = 0;
    std::uint8_t segment_register = 0xFFU;
    bool operand_word = false;
    while (true)
    {
        const std::uint8_t prefix = instruction[prefix_length];
        if (prefix == 0x66)
        {
            operand_word = true;
        }
        else if (prefix == 0x26)
        {
            segment_register = 0;
        }
        else if (prefix == 0x36)
        {
            segment_register = 2;
        }
        else if (prefix == 0x3E)
        {
            segment_register = 3;
        }
        else if (prefix == 0x64)
        {
            segment_register = 4;
        }
        else if (prefix == 0x65)
        {
            segment_register = 5;
        }
        else if (prefix == 0x2E || prefix == 0x67)
        {
            return false;
        }
        else
        {
            break;
        }
        ++prefix_length;
    }

    const std::uint8_t opcode = instruction[prefix_length];
    if (segment_register == 0xFFU ||
        (opcode != 0x8A && opcode != 0x8B && opcode != 0x3A))
    {
        return false;
    }
    ++context->linexe_shared_load_entry_count;
    const std::uint8_t modrm = instruction[prefix_length + 1];
    if (((modrm >> 6) & 0x03U) == 0x03U)
    {
        return false;
    }
    std::uint32_t offset = 0;
    std::uint32_t unprefixed_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                  instruction + prefix_length,
                                  &offset,
                                  &unprefixed_size))
    {
        return false;
    }
    const std::uint8_t destination_register = (modrm >> 3) & 0x07U;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);
    context->linexe_shared_load_selector = selector;
    context->linexe_shared_load_offset = offset;
    std::uint32_t value = 0;
    std::uint32_t width = 0;
    if (opcode == 0x8A || opcode == 0x3A)
    {
        std::uint8_t byte = 0;
        if (!ReadSegmentByte(context, segment_register, selector,
                             offset, &byte))
        {
            return false;
        }
        value = byte;
        width = 1;
        if (opcode == 0x3A)
        {
            const std::uint8_t left =
                ReadGeneralRegister8(win32_context, destination_register);
            UpdateSubtract8Flags(
                win32_context,
                left,
                byte,
                static_cast<std::uint8_t>(left - byte));
        }
        else
        {
            WriteRegister8(win32_context, destination_register, byte);
        }
    }
    else if (operand_word)
    {
        std::uint16_t word = 0;
        if (!ReadSegmentWord(context, selector, offset, &word))
        {
            return false;
        }
        value = word;
        width = 2;
        WriteRegister16(win32_context, destination_register, word);
    }
    else
    {
        if (!ReadSegmentDword(context, segment_register, selector,
                              offset, &value))
        {
            return false;
        }
        width = 4;
        WriteGeneralRegister32(win32_context, destination_register, value);
    }
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 opcode,
                                 segment_register,
                                 selector,
                                 offset,
                                 width,
                                 value);
    ++context->linexe_shared_load_read_count;
    context->linexe_shared_load_value = value;
    const std::uint32_t instruction_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (instruction_offset == 0x000F380FU)
    {
        context->linexe_root_offset_load_value = value;
        ++context->linexe_root_offset_load_success;
    }
    else if (instruction_offset == 0x000F3813U)
    {
        context->linexe_root_selector_load_value = value;
        ++context->linexe_root_selector_load_success;
    }
    else if (instruction_offset == 0x000F38C7U)
    {
        context->linexe_export_entry_name_offset_value = value;
    }
    else if (instruction_offset == 0x000F38CBU)
    {
        context->linexe_export_entry_name_selector_value = value;
    }
    else if (instruction_offset == 0x000F393AU)
    {
        context->linexe_export_value_load_selector = selector;
        context->linexe_export_value_load_offset = offset;
        context->linexe_export_value_load_value = value;
    }
    if (segment_register == 5 && width == 1)
    {
        if (context->linexe_gs_byte_load_count == 0)
        {
            context->linexe_first_gs_byte_offset = offset;
            context->linexe_first_gs_byte_value = value;
        }
        ++context->linexe_gs_byte_load_count;
    }
    win32_context->Eip += prefix_length + unprefixed_size;
    HandleSegmentOverrideMemoryLoadInstruction(win32_context, context);
    return true;
}

bool HandleFsSegmentWordLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x66 ||
        (instruction[1] != 0x64 && instruction[1] != 0x65) ||
        instruction[2] != 0x8B)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[3];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t destination_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t base_register =
        static_cast<std::uint8_t>(modrm & 0x07);
    if (mod == 0x03 || base_register == 0x04)
    {
        return false;
    }

    std::uint32_t instruction_length = 4;
    std::uint32_t offset = 0;
    if (mod == 0x00)
    {
        if (base_register == 0x05)
        {
            return false;
        }
        offset = ReadGeneralRegister32(win32_context, base_register);
    }
    else if (mod == 0x01)
    {
        const std::int8_t displacement =
            static_cast<std::int8_t>(instruction[4]);
        offset = ReadGeneralRegister32(win32_context, base_register) +
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(displacement));
        instruction_length = 5;
    }
    else
    {
        return false;
    }

    const std::uint8_t segment_register =
        instruction[1] == 0x64 ? 4 : 5;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);
    std::uint16_t value = 0;
    if (!ReadSegmentWord(context, selector, offset, &value))
    {
        return false;
    }

    WriteRegister16(win32_context, destination_register, value);
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x8B,
                                 segment_register,
                                 selector,
                                 offset,
                                 2,
                                 value);
    win32_context->Eip += instruction_length;
    return true;
}

bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    const std::uint8_t segment_register = 3;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);

    if (instruction[0] == 0x8B && instruction[1] == 0x06)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint32_t value = 0;
        if (!ReadSegmentDword(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        win32_context->Eax = value;
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0x8B,
                                     segment_register,
                                     selector,
                                     offset,
                                     4,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              offset,
                              value);
        win32_context->Eip += 2;
        return true;
    }

    if (instruction[0] == 0xAC)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint8_t value = 0;
        if (!ReadSegmentByte(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        win32_context->Eax =
            (win32_context->Eax & 0xFFFFFF00U) | value;
        if ((win32_context->EFlags & 0x400U) != 0)
        {
            --win32_context->Esi;
        }
        else
        {
            ++win32_context->Esi;
        }
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0xAC,
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              offset,
                              value);
        ++win32_context->Eip;
        return true;
    }

    if (instruction[0] == 0xA4)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint8_t value = 0;
        if (!ReadSegmentByte(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        const std::uint32_t destination_address = win32_context->Edi;
        void* destination = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination_address));
        if (!WriteGuestUInt8(context, destination, value))
        {
            return false;
        }

        if ((win32_context->EFlags & 0x400U) != 0)
        {
            --win32_context->Esi;
            --win32_context->Edi;
        }
        else
        {
            ++win32_context->Esi;
            ++win32_context->Edi;
        }
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0xA4,
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              destination_address,
                              value);
        ++win32_context->Eip;
        return true;
    }

    return false;
}

bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x80 || instruction[1] != 0x3E)
    {
        return false;
    }

    const std::uint8_t segment_register = 3;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register);
    const std::uint32_t offset = win32_context->Esi;

    std::uint8_t value = 0;
    if (!ReadSegmentByte(
            context, segment_register, selector, offset, &value))
    {
        return false;
    }

    const std::uint8_t immediate = instruction[2];
    if (value == immediate)
    {
        win32_context->EFlags |= 0x40U;
    }
    else
    {
        win32_context->EFlags &= ~0x40U;
    }
    win32_context->EFlags &= ~1U;
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x80,
                                 segment_register,
                                 selector,
                                 offset,
                                 1,
                                 value);
    RecordLowMemoryAccess(win32_context,
                          context,
                          instruction[0],
                          offset,
                          value);
    win32_context->Eip += 3;
    return true;
}

bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->guest_handler_phase,
            20);
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    const std::uint32_t modrm_offset = instruction[0] == 0x66 ? 2U : 1U;
    if ((instruction[modrm_offset] & 0xC0U) == 0xC0U)
    {
        return false;
    }
    std::uint32_t destination = 0;
    std::uint32_t value = 0;
    std::uint32_t instruction_size = 0;
    std::uint32_t value_width = 4;
    std::uint32_t store_opcode = instruction[0];
    const char* source_kind = "unknown";

    if (instruction[0] == 0xC7)
    {
        source_kind = "mov-imm32";
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        if (operation != 0 ||
            !DecodeModRmMemoryAddress(win32_context,
                                               instruction,
                                               &destination,
                                               &instruction_size))
        {
            return false;
        }
        const std::uint32_t immediate_offset = instruction_size;
        value =
            static_cast<std::uint32_t>(instruction[immediate_offset]) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 1]) << 8) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 2]) << 16) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 3]) << 24);
        instruction_size += 4;
    }
    else if (instruction[0] == 0xC6)
    {
        source_kind = "mov-imm8";
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        if (operation != 0 ||
            !DecodeModRmMemoryAddress(win32_context,
                                      instruction,
                                      &destination,
                                      &instruction_size))
        {
            return false;
        }
        value = instruction[instruction_size];
        ++instruction_size;
        value_width = 1;
    }
    else if (instruction[0] == 0x66 && instruction[1] == 0xC7)
    {
        store_opcode = 0x66C7;
        source_kind = "mov-imm16";
        const std::uint8_t operation = (instruction[2] >> 3) & 0x07U;
        std::uint32_t unprefixed_instruction_size = 0;
        if (operation != 0 ||
            !DecodeModRmMemoryAddress(
                win32_context,
                instruction + 1,
                &destination,
                &unprefixed_instruction_size))
        {
            return false;
        }
        const std::uint32_t immediate_offset =
            1 + unprefixed_instruction_size;
        value =
            static_cast<std::uint32_t>(instruction[immediate_offset]) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 1]) << 8);
        instruction_size = immediate_offset + 2;
        value_width = 2;
    }
    else if (instruction[0] == 0x89)
    {
        source_kind = "mov-reg32";
        if (!DecodeModRmMemoryAddress(win32_context,
                                               instruction,
                                               &destination,
                                               &instruction_size))
        {
            return false;
        }
        const std::uint8_t source_register = (instruction[1] >> 3) & 0x07U;
        value = ReadGeneralRegister32(win32_context, source_register);
    }
    else if (instruction[0] == 0x66 && instruction[1] == 0x89)
    {
        store_opcode = 0x6689;
        source_kind = "mov-reg16";
        std::uint32_t unprefixed_instruction_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + 1,
                                      &destination,
                                      &unprefixed_instruction_size))
        {
            return false;
        }
        const std::uint8_t source_register =
            (instruction[2] >> 3) & 0x07U;
        value = ReadRegister16(*win32_context, source_register);
        instruction_size = 1U + unprefixed_instruction_size;
        value_width = 2;
    }
    else
    {
        return false;
    }

    const std::uint32_t instruction_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (instruction_offset == 0x000F393FU)
    {
        ++context->linexe_export_result_store_count;
        context->linexe_export_result_store_destination = destination;
        context->linexe_export_result_store_value = value;
    }

    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->guest_handler_phase,
            21);
    }

    if (IsGuestRangeWritable(context, destination_pointer, value_width))
    {
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->guest_handler_phase,
                22);
        }
        const bool written = value_width == 1
            ? WriteGuestBytes(context,
                              destination_pointer,
                              &value,
                              value_width)
            : value_width == 2
                ? WriteGuestUInt16(context,
                                   destination_pointer,
                                   static_cast<std::uint16_t>(value))
                : WriteGuestUInt32(context, destination_pointer, value);
        if (!written)
        {
            return false;
        }
        RecordGuestMemoryStore(win32_context,
                               context,
                               store_opcode,
                               destination,
                               value,
                               value_width,
                               source_kind,
                               true);
        win32_context->Eip += instruction_size;
        return true;
    }

    constexpr std::uint32_t kAllocatorSentinelShadowLimit = 0x00100000;
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    const std::uint64_t sentinel_limit =
        runtime_end + kAllocatorSentinelShadowLimit;
    const bool allocator_failure_sentinel =
        instruction[0] == 0xC7 && value == 0xFFFFFFFFU &&
        destination >= runtime_end && destination < sentinel_limit;
    const std::uint64_t destination_end =
        static_cast<std::uint64_t>(destination) + value_width;
    const bool begins_allocator_metadata =
        instruction[0] == 0x89 && context->shadow_memory_range_valid &&
        destination < context->shadow_memory_min_address &&
        context->shadow_memory_min_address - destination == value;
    const bool extends_allocator_metadata =
        instruction[0] == 0x89 && context->shadow_memory_range_valid &&
        destination >= context->shadow_memory_min_address &&
        static_cast<std::uint64_t>(destination) <=
            static_cast<std::uint64_t>(
                context->shadow_memory_max_address) + 1;
    const bool allocator_metadata_store =
        destination >= runtime_end && destination_end <= sentinel_limit &&
        (begins_allocator_metadata || extends_allocator_metadata);
    constexpr std::uint32_t kBoundaryObjectWindow = 64;
    const std::uint8_t boundary_modrm =
        instruction[0] == 0x66 ? instruction[2] : instruction[1];
    const std::uint8_t mod =
        static_cast<std::uint8_t>((boundary_modrm >> 6) & 0x03U);
    const std::uint8_t rm =
        static_cast<std::uint8_t>(boundary_modrm & 0x07U);
    const std::uint64_t base =
        ReadGeneralRegister32(win32_context, rm);
    const bool supported_boundary_store =
        instruction[0] == 0xC7 || instruction[0] == 0x89 ||
        (instruction[0] == 0x66 && instruction[1] == 0xC7);
    const bool arena_boundary_object_store =
        supported_boundary_store && (mod == 0x01 || mod == 0x02) &&
        base < runtime_end && base + kBoundaryObjectWindow >= runtime_end &&
        destination >= runtime_end &&
        destination_end <= runtime_end + kBoundaryObjectWindow;
    const bool chained_boundary_object_store =
        supported_boundary_store && context->boundary_object_chain_valid &&
        (mod == 0x00 || mod == 0x01 || mod == 0x02) &&
        (base == context->boundary_object_chain_base ||
         base == context->boundary_object_chain_frontier) &&
        destination >= base &&
        destination_end <= base + kBoundaryObjectWindow &&
        destination_end <= context->boundary_object_chain_limit;
    if (!allocator_failure_sentinel && !allocator_metadata_store &&
        !arena_boundary_object_store && !chained_boundary_object_store &&
        (context->last_dos_open_success ||
         context->last_dos_open_guest_path.empty()))
    {
        return false;
    }

    RecordGuestMemoryStore(win32_context,
                           context,
                           store_opcode,
                           destination,
                           value,
                           value_width,
                           source_kind,
                           false);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->guest_handler_phase,
            23);
    }
    WriteShadowMemory(context, destination, value, value_width);
    if (arena_boundary_object_store || chained_boundary_object_store)
    {
        if (!context->boundary_object_chain_valid ||
            base == context->boundary_object_chain_frontier)
        {
            if (!context->boundary_object_chain_valid)
            {
                constexpr std::uint64_t kFallbackSpan = 4096;
                constexpr std::uint64_t kMaximumSpan = 0x00100000;
                const std::uint64_t observed_span =
                    static_cast<std::uint64_t>(win32_context->Esi) *
                    win32_context->Edx;
                const std::uint64_t chain_span =
                    observed_span >= kBoundaryObjectWindow &&
                            observed_span <= kMaximumSpan
                        ? observed_span
                        : kFallbackSpan;
                context->boundary_object_chain_limit =
                    runtime_end + chain_span;
            }
            context->boundary_object_chain_valid = true;
            context->boundary_object_chain_base =
                static_cast<std::uint32_t>(base);
        }
        context->boundary_object_chain_frontier = std::max(
            context->boundary_object_chain_frontier,
            static_cast<std::uint32_t>(destination_end));
    }
    win32_context->Eip += instruction_size;
    return true;
}

void AttachAllocatorReadProvenance(ThreadContext* context,
                                   std::uint32_t eip_offset,
                                   std::uint32_t source,
                                   std::uint32_t value)
{
    if (context == nullptr ||
        context->allocator_control_flow.observed_count == 0)
    {
        return;
    }

    const std::uint32_t sequence =
        context->allocator_control_flow.observed_count;
    const std::uint32_t slot =
        (sequence - 1) % kWin32AllocatorControlFlowTraceCapacity;
    Win32AllocatorControlFlowTraceEntry& entry =
        context->allocator_control_flow.trace[slot];
    if (!entry.valid || entry.sequence != sequence ||
        entry.eip_offset != eip_offset)
    {
        return;
    }

    entry.read_valid = true;
    entry.read_address = source;
    entry.read_value = value;
    entry.read_explicit_shadow = true;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        if (context->shadow_memory.find(source + index) ==
            context->shadow_memory.end())
        {
            entry.read_explicit_shadow = false;
            break;
        }
    }
    entry.read_zero_backed = !entry.read_explicit_shadow &&
        context->shadow_zero_payload_valid &&
        source >= context->shadow_zero_payload_begin &&
        static_cast<std::uint64_t>(source) + 4U <=
            context->shadow_zero_payload_end;

    const std::uint32_t stored_count = std::min(
        context->shadow_write_provenance_count,
        kShadowWriteProvenanceCapacity);
    const ShadowWriteProvenance* writer = nullptr;
    for (std::uint32_t reverse_index = 0;
         reverse_index < stored_count;
         ++reverse_index)
    {
        const std::uint32_t sequence =
            context->shadow_write_provenance_count - reverse_index;
        const std::uint32_t slot =
            (sequence - 1) % kShadowWriteProvenanceCapacity;
        const ShadowWriteProvenance& candidate =
            context->shadow_write_provenance[slot];
        const std::uint64_t candidate_end =
            static_cast<std::uint64_t>(candidate.destination) +
            candidate.width;
        if (candidate.sequence == sequence &&
            source >= candidate.destination &&
            static_cast<std::uint64_t>(source) + 4U <= candidate_end)
        {
            writer = &candidate;
            break;
        }
    }
    if (writer != nullptr)
    {
        entry.writer_valid = true;
        entry.writer_sequence = writer->sequence;
        entry.writer_eip_offset = writer->eip >= context->runtime_base
            ? writer->eip - context->runtime_base
            : writer->eip;
        entry.writer_opcode = writer->opcode;
        entry.writer_destination = writer->destination;
        entry.writer_value = writer->value;
        entry.writer_width = writer->width;
    }

    if (eip_offset == 0x000F7A62U && value == 0 &&
        !context->allocator_control_flow.root_transition_valid)
    {
        context->allocator_control_flow.root_transition_valid = true;
        context->allocator_control_flow.root_transition = entry;
    }
    if (eip_offset == 0x000F7A83U && source != 8U)
    {
        Win32AllocatorControlFlowObservation& observation =
            context->allocator_control_flow;
        if (value == 0 && !observation.null_link_transition_valid)
        {
            observation.null_link_transition_valid = true;
            observation.null_link_transition = entry;
        }
        else if (value == 0xFF000000U &&
                 !observation.poison_link_transition_valid)
        {
            observation.poison_link_transition_valid = true;
            observation.poison_link_transition = entry;
        }
    }
}

bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x8B)
    {
        return false;
    }
    if ((instruction[1] & 0xC0U) == 0xC0U)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    const std::uint64_t instruction_offset =
        static_cast<std::uint64_t>(win32_context->Eip) -
        context->runtime_base;
    const bool allocator_probe = instruction_offset == 0x000F7A71ULL;
    const bool pending_before = context->pending_shadow_allocation_valid;
    const std::uint32_t pending_size_before =
        context->pending_shadow_allocation_size;
    auto record_allocator_probe = [&](const char* result) {
        if (!allocator_probe)
        {
            return;
        }
        Win32AllocatorProbeObservation& observation =
            context->allocator_probe;
        const std::uint32_t sequence = observation.observed_count + 1;
        const std::uint32_t slot =
            (sequence - 1) % kWin32AllocatorProbeTraceCapacity;
        Win32AllocatorProbeTraceEntry& entry = observation.trace[slot];
        entry.valid = true;
        entry.sequence = sequence;
        entry.eax = win32_context->Eax;
        entry.esi = win32_context->Esi;
        entry.source = source;
        entry.ds = context->guest_ds;
        entry.pending_before = pending_before;
        entry.pending_size_before = pending_size_before;
        entry.pending_after = context->pending_shadow_allocation_valid;
        entry.pending_size_after = context->pending_shadow_allocation_size;
        entry.result = result != nullptr ? result : "unknown";
        observation.observed_count = sequence;
        if (observation.trace_stored_count <
            kWin32AllocatorProbeTraceCapacity)
        {
            ++observation.trace_stored_count;
        }
        else
        {
            observation.trace_wrapped = true;
        }
    };

    std::uint32_t value = 0;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    const bool read_from_guest =
        ReadGuestUInt32(context, source_pointer, &value);
    const bool read_from_shadow =
        !read_from_guest && ReadShadowUInt32(context, source, &value);
    if (!read_from_guest && !read_from_shadow)
    {
        std::uint32_t linear_address = 0;
        if (!repiu::runtime::TranslateSelectorOffset(
                context->selector_table,
                context->guest_ds,
                source,
                sizeof(value),
                &linear_address))
        {
            record_allocator_probe("rejected");
            return false;
        }
        const bool translated_read =
            linear_address < repiu::runtime::kDosLowMemorySize
                ? repiu::runtime::ReadDosLowMemoryUInt32(
                      context->dos_low_memory,
                      linear_address,
                      &value)
                : ReadGuestUInt32(
                      context,
                      reinterpret_cast<const void*>(
                          static_cast<std::uintptr_t>(linear_address)),
                      &value);
        if (!translated_read)
        {
            record_allocator_probe("rejected");
            return false;
        }
        const bool has_confirmed_allocation_size =
            win32_context->Eax == 0x0000002CU ||
            win32_context->Eax == 0x00001008U;
        bool captured = false;
        if (!context->pending_shadow_allocation_valid &&
            allocator_probe &&
            source == 0 &&
            has_confirmed_allocation_size)
        {
            context->pending_shadow_allocation_valid = true;
            context->pending_shadow_allocation_size = win32_context->Eax;
            captured = true;
        }
        record_allocator_probe(
            captured ? "captured"
                     : context->pending_shadow_allocation_valid
                           ? "pending-preserved"
                           : "zero-page");
    }
    else
    {
        record_allocator_probe("mapped-or-shadow");
    }

    if (instruction_offset >= 0x000F7A60ULL &&
        instruction_offset < 0x000F7AD5ULL)
    {
        AttachAllocatorReadProvenance(
            context,
            static_cast<std::uint32_t>(instruction_offset),
            source,
            value);
    }

    const std::uint8_t destination_register = (instruction[1] >> 3) & 0x07U;
    WriteGeneralRegister32(win32_context, destination_register, value);
    win32_context->Eip += instruction_size;
    HandleSegmentOverrideMemoryLoadInstruction(win32_context, context);
    return true;
}

bool HasEvenParity(std::uint8_t value)
{
    bool even_parity = true;
    while (value != 0)
    {
        even_parity = !even_parity;
        value = static_cast<std::uint8_t>(value & (value - 1U));
    }
    return even_parity;
}

void UpdateAdd32Flags(CONTEXT* win32_context,
                      std::uint32_t left,
                      std::uint32_t right,
                      std::uint32_t result)
{
    constexpr std::uint32_t kCarryFlag = 0x00000001U;
    constexpr std::uint32_t kParityFlag = 0x00000004U;
    constexpr std::uint32_t kAuxiliaryCarryFlag = 0x00000010U;
    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    constexpr std::uint32_t kSignFlag = 0x00000080U;
    constexpr std::uint32_t kOverflowFlag = 0x00000800U;
    constexpr std::uint32_t kArithmeticFlags =
        kCarryFlag | kParityFlag | kAuxiliaryCarryFlag | kZeroFlag |
        kSignFlag | kOverflowFlag;

    win32_context->EFlags &= ~kArithmeticFlags;
    if (static_cast<std::uint64_t>(left) + right > 0xFFFFFFFFULL)
    {
        win32_context->EFlags |= kCarryFlag;
    }
    if (HasEvenParity(static_cast<std::uint8_t>(result & 0xFFU)))
    {
        win32_context->EFlags |= kParityFlag;
    }
    if (((left ^ right ^ result) & 0x10U) != 0)
    {
        win32_context->EFlags |= kAuxiliaryCarryFlag;
    }
    if (result == 0)
    {
        win32_context->EFlags |= kZeroFlag;
    }
    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= kSignFlag;
    }
    if (((~(left ^ right) & (left ^ result)) & 0x80000000U) != 0)
    {
        win32_context->EFlags |= kOverflowFlag;
    }
}

std::uint8_t ReadGeneralRegister8(const CONTEXT* win32_context,
                                  std::uint8_t register_index)
{
    const std::uint8_t byte_register = register_index & 0x07U;
    if (byte_register < 4)
    {
        return static_cast<std::uint8_t>(
            ReadGeneralRegister32(win32_context, byte_register) & 0xFFU);
    }
    return static_cast<std::uint8_t>(
        (ReadGeneralRegister32(win32_context, byte_register - 4U) >> 8) &
        0xFFU);
}

void UpdateLogical32Flags(CONTEXT* win32_context, std::uint32_t result)
{
    constexpr std::uint32_t kCarryFlag = 0x00000001U;
    constexpr std::uint32_t kParityFlag = 0x00000004U;
    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    constexpr std::uint32_t kSignFlag = 0x00000080U;
    constexpr std::uint32_t kOverflowFlag = 0x00000800U;
    constexpr std::uint32_t kDefinedLogicalFlags =
        kCarryFlag | kParityFlag | kZeroFlag | kSignFlag | kOverflowFlag;

    win32_context->EFlags &= ~kDefinedLogicalFlags;
    if (HasEvenParity(static_cast<std::uint8_t>(result & 0xFFU)))
    {
        win32_context->EFlags |= kParityFlag;
    }
    if (result == 0)
    {
        win32_context->EFlags |= kZeroFlag;
    }
    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= kSignFlag;
    }
}

void UpdateSubtract8Flags(CONTEXT* win32_context,
                          std::uint8_t left,
                          std::uint8_t right,
                          std::uint8_t result)
{
    constexpr std::uint32_t kCarryFlag = 0x00000001U;
    constexpr std::uint32_t kParityFlag = 0x00000004U;
    constexpr std::uint32_t kAuxiliaryCarryFlag = 0x00000010U;
    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    constexpr std::uint32_t kSignFlag = 0x00000080U;
    constexpr std::uint32_t kOverflowFlag = 0x00000800U;
    constexpr std::uint32_t kArithmeticFlags =
        kCarryFlag | kParityFlag | kAuxiliaryCarryFlag | kZeroFlag |
        kSignFlag | kOverflowFlag;

    win32_context->EFlags &= ~kArithmeticFlags;
    if (left < right)
    {
        win32_context->EFlags |= kCarryFlag;
    }
    if (HasEvenParity(result))
    {
        win32_context->EFlags |= kParityFlag;
    }
    if (((left ^ right ^ result) & 0x10U) != 0)
    {
        win32_context->EFlags |= kAuxiliaryCarryFlag;
    }
    if (result == 0)
    {
        win32_context->EFlags |= kZeroFlag;
    }
    if ((result & 0x80U) != 0)
    {
        win32_context->EFlags |= kSignFlag;
    }
    if ((((left ^ right) & (left ^ result)) & 0x80U) != 0)
    {
        win32_context->EFlags |= kOverflowFlag;
    }
}

bool HandleTracedMemoryAddInstruction(CONTEXT* win32_context,
                                      ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x03)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    std::uint32_t source_value = 0;
    if (!ReadShadowUInt32(context, source, &source_value))
    {
        return false;
    }

    const std::uint8_t destination_register = (instruction[1] >> 3) & 0x07U;
    const std::uint32_t destination_value =
        ReadGeneralRegister32(win32_context, destination_register);
    const std::uint32_t result = destination_value + source_value;
    WriteGeneralRegister32(win32_context, destination_register, result);
    UpdateAdd32Flags(win32_context,
                     destination_value,
                     source_value,
                     result);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedMemoryOrInstruction(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x83 ||
        ((instruction[1] >> 3) & 0x07U) != 0x01U)
    {
        return false;
    }

    std::uint32_t destination = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &destination,
                                           &instruction_size))
    {
        return false;
    }

    std::uint32_t destination_value = 0;
    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    const bool real_destination =
        ReadGuestUInt32(context, destination_pointer, &destination_value);
    if (!real_destination &&
        !ReadShadowUInt32(context, destination, &destination_value))
    {
        return false;
    }

    const std::uint32_t immediate = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(
            static_cast<std::int8_t>(instruction[instruction_size])));
    const std::uint32_t result = destination_value | immediate;
    if (real_destination)
    {
        if (!WriteGuestUInt32(context, destination_pointer, result))
        {
            return false;
        }
    }
    else
    {
        WriteShadowMemory(context, destination, result, 4);
    }
    RecordGuestMemoryStore(win32_context,
                           context,
                           0x83,
                           destination,
                           result,
                           4,
                           "or-imm8",
                           real_destination);
    const std::uint64_t instruction_offset =
        static_cast<std::uint64_t>(win32_context->Eip) -
        context->runtime_base;
    if (!real_destination && context->pending_shadow_allocation_valid &&
        instruction_offset == 0x000F7AD4ULL)
    {
        const std::uint32_t allocation_size =
            context->pending_shadow_allocation_size;
        const std::uint64_t payload_begin =
            static_cast<std::uint64_t>(destination) + 4U;
        const std::uint64_t payload_end =
            static_cast<std::uint64_t>(destination) +
            allocation_size - 4U;
        if (payload_begin < payload_end && payload_end <= 0xFFFFFFFFULL)
        {
            context->shadow_zero_payload_valid = true;
            context->shadow_zero_payload_begin =
                static_cast<std::uint32_t>(payload_begin);
            context->shadow_zero_payload_end =
                static_cast<std::uint32_t>(payload_end);
        }
        context->pending_shadow_allocation_valid = false;
        context->pending_shadow_allocation_size = 0;
    }
    UpdateLogical32Flags(win32_context, result);
    win32_context->Eip += instruction_size + 1;
    return true;
}

bool HandleTracedMemoryCompareByteInstruction(CONTEXT* win32_context,
                                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x38)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    std::uint8_t source_value = 0;
    if (!ReadShadowUInt8(context, source, &source_value))
    {
        return false;
    }

    const std::uint8_t register_index = (instruction[1] >> 3) & 0x07U;
    const std::uint8_t register_value =
        ReadGeneralRegister8(win32_context, register_index);
    const std::uint8_t result = static_cast<std::uint8_t>(
        source_value - register_value);
    UpdateSubtract8Flags(win32_context,
                         source_value,
                         register_value,
                         result);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xF7 || instruction[1] != 0x07)
    {
        return false;
    }

    const std::uint32_t source = win32_context->Edi;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    std::uint32_t source_value = 0;
    if (!ReadGuestUInt32(context, source_pointer, &source_value) &&
        !ReadShadowUInt32(context, source, &source_value))
    {
        return false;
    }

    const std::uint32_t mask =
        static_cast<std::uint32_t>(instruction[2]) |
        (static_cast<std::uint32_t>(instruction[3]) << 8) |
        (static_cast<std::uint32_t>(instruction[4]) << 16) |
        (static_cast<std::uint32_t>(instruction[5]) << 24);
    const std::uint32_t result = source_value & mask;

    win32_context->EFlags &= ~1U;
    win32_context->EFlags &= ~0x800U;
    if (result == 0)
    {
        win32_context->EFlags |= 0x40U;
    }
    else
    {
        win32_context->EFlags &= ~0x40U;
    }

    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= 0x80U;
    }
    else
    {
        win32_context->EFlags &= ~0x80U;
    }

    win32_context->Eip += 6;
    return true;
}

bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xD9)
    {
        return false;
    }

    std::uint32_t address = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &address,
                                           &instruction_size))
    {
        return false;
    }

    const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(address));
    if (operation == 0)
    {
        std::uint32_t value = 0;
        if (ReadGuestUInt32(context, source_pointer, &value))
        {
            context->last_traced_fpu_m32_value = value;
            context->has_last_traced_fpu_m32_value = true;
            win32_context->Eip += instruction_size;
            return true;
        }
        if (ReadShadowUInt32(context, address, &value))
        {
            context->last_traced_fpu_m32_value = value;
            context->has_last_traced_fpu_m32_value = true;
            win32_context->Eip += instruction_size;
            return true;
        }
        return false;
    }

    if (operation != 2 && operation != 3)
    {
        return false;
    }
    if (!context->has_last_traced_fpu_m32_value)
    {
        return false;
    }

    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(address));
    const std::uint32_t value = context->last_traced_fpu_m32_value;
    if (IsGuestRangeWritable(context, destination_pointer, sizeof(value)))
    {
        if (!WriteGuestUInt32(context, destination_pointer, value))
        {
            return false;
        }
        RecordGuestMemoryStore(win32_context,
                               context,
                               0xD9,
                               address,
                               value,
                               4,
                               "fpu-m32",
                               true);
        win32_context->Eip += instruction_size;
        return true;
    }

    constexpr std::uint32_t kBoundaryObjectWindow = 64;
    const std::uint8_t mod =
        static_cast<std::uint8_t>((instruction[1] >> 6) & 0x03U);
    const std::uint8_t rm =
        static_cast<std::uint8_t>(instruction[1] & 0x07U);
    const std::uint64_t base =
        ReadGeneralRegister32(win32_context, rm);
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    const std::uint64_t address_end =
        static_cast<std::uint64_t>(address) + sizeof(value);
    const bool arena_boundary_object_store =
        (mod == 0x01 || mod == 0x02) && base < runtime_end &&
        base + kBoundaryObjectWindow >= runtime_end &&
        address >= runtime_end &&
        address_end <= runtime_end + kBoundaryObjectWindow;
    const bool chained_boundary_object_store =
        context->boundary_object_chain_valid &&
        (mod == 0x00 || mod == 0x01 || mod == 0x02) &&
        (base == context->boundary_object_chain_base ||
         base == context->boundary_object_chain_frontier) &&
        address >= base && address_end <= base + kBoundaryObjectWindow &&
        address_end <= context->boundary_object_chain_limit;
    if (!arena_boundary_object_store && !chained_boundary_object_store &&
        (context->last_dos_open_success ||
         context->last_dos_open_guest_path.empty()))
    {
        return false;
    }

    RecordGuestMemoryStore(win32_context,
                           context,
                           0xD9,
                           address,
                           value,
                           4,
                           "fpu-m32",
                           false);
    WriteShadowMemory(context, address, value, 4);
    if (arena_boundary_object_store || chained_boundary_object_store)
    {
        if (!context->boundary_object_chain_valid ||
            base == context->boundary_object_chain_frontier)
        {
            if (!context->boundary_object_chain_valid)
            {
                constexpr std::uint64_t kFallbackSpan = 4096;
                constexpr std::uint64_t kMaximumSpan = 0x00100000;
                const std::uint64_t observed_span =
                    static_cast<std::uint64_t>(win32_context->Esi) *
                    win32_context->Edx;
                const std::uint64_t chain_span =
                    observed_span >= kBoundaryObjectWindow &&
                            observed_span <= kMaximumSpan
                        ? observed_span
                        : kFallbackSpan;
                context->boundary_object_chain_limit =
                    runtime_end + chain_span;
            }
            context->boundary_object_chain_valid = true;
            context->boundary_object_chain_base =
                static_cast<std::uint32_t>(base);
        }
        context->boundary_object_chain_frontier = std::max(
            context->boundary_object_chain_frontier,
            static_cast<std::uint32_t>(address_end));
    }
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedDosInterrupt21(CONTEXT* win32_context,
                                ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x21)
    {
        return false;
    }

    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFF);
    switch (ah)
    {
        case 0x09:
            return HandleDosInterrupt21(win32_context, context);
        case 0x19:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetCurrentDrive(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x30:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x0007U;
            win32_context->Ebx = 0;
            win32_context->Ecx = 0;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0x25:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetInterruptVector(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x35:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetInterruptVector(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x3B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosChangeDirectory(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3D:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosOpenFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3E:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosCloseFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3F:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosReadFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x40:
        {
            RecordHandledDosInterrupt(context, 0x21, ax);
            const void* text = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            const std::uint32_t byte_count = win32_context->Ecx;
            AppendConsoleOutput(
                context, text, byte_count, win32_context->Ebx == 2U);
            win32_context->Eax = byte_count;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        }
        case 0x42:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosSeekFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x43:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosFileAttributes(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x44:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosIoctl(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x47:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosGetCurrentDirectory(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x4C:
            RecordHandledDosInterrupt(context, 0x21, ax);
            CaptureDosTermination(win32_context, context);
            RecoverFromHleExit(win32_context, context);
            return true;
        case 0xFF:
            RecordHandledDosInterrupt(context, 0x21, ax);
            return HandleDosInterrupt21(win32_context, context);
        case 0xED:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0x4A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosResizeMemoryBlock(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        default:
            return false;
    }
}

bool HandleTracedDosInterrupt2F(CONTEXT* win32_context,
                                ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x2F)
    {
        return false;
    }

    return HandleDosInterrupt2F(win32_context, context);
}

bool HandleTracedDpmiInterrupt31(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x31)
    {
        return false;
    }

    return HandleDpmiInterrupt31(win32_context, context);
}

bool HandleTracedMouseInterrupt33(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x33)
    {
        return false;
    }

    return HandleMouseInterrupt33(win32_context, context);
}

bool HandleOriginalFatalBreakpoint(EXCEPTION_POINTERS* exception_info,
                                   CONTEXT* win32_context,
                                   ThreadContext* context)
{
    if (exception_info == nullptr ||
        exception_info->ExceptionRecord == nullptr ||
        exception_info->ExceptionRecord->ExceptionCode !=
            EXCEPTION_BREAKPOINT ||
        win32_context == nullptr || context == nullptr ||
        win32_context->Eip == 0)
    {
        return false;
    }

    const std::uint32_t context_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    std::uint32_t breakpoint = context_eip;
    bool advance_to_continuation = false;
    if (IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(context_eip)),
            1U) &&
        *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(context_eip)) == 0xCC)
    {
        advance_to_continuation = true;
    }
    else
    {
        breakpoint = context_eip - 1U;
    }
    if (!IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(breakpoint)),
            8U))
    {
        return false;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(breakpoint));
    if (instruction[0] != 0xCC || instruction[1] != 0x52 ||
        instruction[2] != 0xE8 || instruction[7] != 0xF4)
    {
        return false;
    }

    context->handled_fatal_breakpoint_count += 1U;
    context->last_fatal_breakpoint_address = breakpoint;
    context->last_fatal_message_address =
        static_cast<std::uint32_t>(win32_context->Edx);
    context->last_fatal_message.clear();
    ReadGuestAsciz(context,
                   context->last_fatal_message_address,
                   512U,
                   &context->last_fatal_message);
    context->fatal_breakpoint_continued = true;
    if (advance_to_continuation)
    {
        win32_context->Eip += 1U;
    }
    return true;
}

bool ResolveSegmentLinearRange(ThreadContext* context,
                               std::uint16_t selector,
                               std::uint32_t offset,
                               std::uint32_t byte_count,
                               bool writable,
                               std::uint32_t* linear_address)
{
    if (context == nullptr || linear_address == nullptr)
    {
        return false;
    }

    std::uint32_t translated = 0;
    if (!repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            byte_count,
            &translated))
    {
        translated = offset;
    }
    void* pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(translated));
    const bool valid = writable
        ? IsGuestRangeWritable(context, pointer, byte_count)
        : IsGuestRangeReadable(context, pointer, byte_count);
    if (!valid)
    {
        translated = offset;
        pointer = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(translated));
        const bool direct_valid = writable
            ? IsGuestRangeWritable(context, pointer, byte_count)
            : IsGuestRangeReadable(context, pointer, byte_count);
        if (!direct_valid)
        {
            return false;
        }
    }
    *linear_address = translated;
    return true;
}

bool HandleRepCmpsbInstruction(CONTEXT* win32_context,
                               ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if ((instruction[0] != 0xF2 && instruction[0] != 0xF3) ||
        instruction[1] != 0xA6)
    {
        return false;
    }
    const bool decrement =
        (win32_context->EFlags & 0x00000400U) != 0;
    while (win32_context->Ecx != 0)
    {
        std::uint32_t source = 0;
        std::uint32_t destination = 0;
        if (!ResolveSegmentLinearRange(context, context->guest_ds,
                                       win32_context->Esi, 1, false,
                                       &source) ||
            !ResolveSegmentLinearRange(context, context->guest_es,
                                       win32_context->Edi, 1, false,
                                       &destination))
        {
            return false;
        }
        const std::uint8_t left = *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(source));
        const std::uint8_t right = *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(destination));
        const std::uint8_t result = static_cast<std::uint8_t>(left - right);
        UpdateSubtract8Flags(win32_context, left, right, result);
        win32_context->Esi += decrement ? -1 : 1;
        win32_context->Edi += decrement ? -1 : 1;
        --win32_context->Ecx;
        const bool equal = result == 0;
        if ((instruction[0] == 0xF3 && !equal) ||
            (instruction[0] == 0xF2 && equal))
        {
            break;
        }
    }
    win32_context->Eip += 2;
    return true;
}

bool CopyHostMemoryWithoutVehRecursion(ThreadContext* context,
                                       std::uint32_t destination,
                                       const void* source,
                                       std::uint32_t byte_count,
                                       std::uint32_t* failure_stage,
                                       std::uint32_t* windows_error)
{
    if (source == nullptr || byte_count == 0)
    {
        return byte_count == 0;
    }
    std::vector<std::uint8_t> temporary(byte_count);
    SIZE_T bytes_read = 0;
    HANDLE process = GetCurrentProcess();
    if (!ReadProcessMemory(process,
                           source,
                           temporary.data(),
                           byte_count,
                           &bytes_read) ||
        bytes_read != byte_count)
    {
        if (failure_stage != nullptr)
        {
            *failure_stage = 1;
        }
        if (windows_error != nullptr)
        {
            *windows_error = GetLastError();
        }
        return false;
    }
    if (!WriteGuestBytes(
            context,
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(destination)),
            temporary.data(),
            byte_count))
    {
        if (failure_stage != nullptr)
        {
            *failure_stage = 2;
        }
        if (windows_error != nullptr)
        {
            *windows_error = GetLastError();
        }
        return false;
    }
    return true;
}

bool HandleRepMovsInstruction(CONTEXT* win32_context,
                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if ((instruction[0] != 0xF2 && instruction[0] != 0xF3) ||
        (instruction[1] != 0xA4 && instruction[1] != 0xA5) ||
        (win32_context->EFlags & 0x00000400U) != 0)
    {
        return false;
    }

    const std::uint32_t width = instruction[1] == 0xA5 ? 4U : 1U;
    const std::uint64_t byte_count64 =
        static_cast<std::uint64_t>(win32_context->Ecx) * width;
    if (byte_count64 > 0xFFFFFFFFULL)
    {
        return false;
    }
    const std::uint32_t byte_count =
        static_cast<std::uint32_t>(byte_count64);
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    bool source_is_low_memory = false;
    bool destination_is_low_memory = false;
    if (byte_count != 0 &&
        static_cast<std::uint64_t>(win32_context->Esi) + byte_count <=
            repiu::runtime::kDosLowMemorySize)
    {
        source = static_cast<std::uint32_t>(win32_context->Esi);
        source_is_low_memory = true;
    }
    else if (byte_count != 0 &&
             !ResolveSegmentLinearRange(context,
                                        context->guest_ds,
                                        win32_context->Esi,
                                        byte_count,
                                        false,
                                        &source))
    {
        return false;
    }
    if (byte_count != 0)
    {
        if (static_cast<std::uint64_t>(win32_context->Edi) + byte_count <=
                repiu::runtime::kDosLowMemorySize)
        {
            destination = static_cast<std::uint32_t>(win32_context->Edi);
            destination_is_low_memory = true;
        }
        else if (repiu::runtime::TranslateSelectorOffset(
                context->selector_table,
                context->guest_es,
                win32_context->Edi,
                byte_count,
                &destination) &&
            destination < repiu::runtime::kDosLowMemorySize &&
            static_cast<std::uint64_t>(destination) + byte_count <=
                repiu::runtime::kDosLowMemorySize)
        {
            destination_is_low_memory = true;
        }
        else if (!ResolveSegmentLinearRange(context,
                                            context->guest_es,
                                            win32_context->Edi,
                                            byte_count,
                                            true,
                                            &destination))
        {
            return false;
        }
    }
    if (byte_count != 0)
    {
        if (destination_is_low_memory)
        {
            const auto* bytes = source_is_low_memory
                ? context->dos_low_memory.bytes.data() + source
                : reinterpret_cast<const std::uint8_t*>(
                      static_cast<std::uintptr_t>(source));
            for (std::uint32_t index = 0; index < byte_count; ++index)
            {
                if (!repiu::runtime::WriteDosLowMemory(
                        &context->dos_low_memory,
                        destination + index,
                        bytes[index],
                        1U))
                {
                    return false;
                }
            }
        }
        else
        {
            const void* source_pointer = source_is_low_memory
                ? static_cast<const void*>(
                      context->dos_low_memory.bytes.data() + source)
                : reinterpret_cast<const void*>(
                      static_cast<std::uintptr_t>(source));
            std::uint32_t failure_stage = 0;
            std::uint32_t windows_error = 0;
            if (!CopyHostMemoryWithoutVehRecursion(context,
                                                   destination,
                                                   source_pointer,
                                                   byte_count,
                                                   &failure_stage,
                                                   &windows_error))
            {
                ++context->rep_movs_copy_failure_count;
                context->last_rep_movs_copy_failure_stage = failure_stage;
                context->last_rep_movs_copy_error = windows_error;
                context->last_rep_movs_copy_source = source;
                context->last_rep_movs_copy_destination = destination;
                context->last_rep_movs_copy_bytes = byte_count;
                return false;
            }
        }
    }
    win32_context->Esi += byte_count;
    win32_context->Edi += byte_count;
    win32_context->Ecx = 0;
    win32_context->Eip += 2;
    context->diagnostic_progress_count.fetch_add(
        1,
        std::memory_order_relaxed);
    return true;
}

bool HandlePrivilegedTrapInstruction(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (*instruction == 0xFA)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        win32_context->EFlags &= ~0x00000200U;
        ++win32_context->Eip;
        return true;
    }
    if (*instruction == 0xFB)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        win32_context->EFlags |= 0x00000200U;
        ++win32_context->Eip;
        return true;
    }
    if (*instruction == 0xF4 && context->fatal_breakpoint_continued)
    {
        RecordHandledHleTrap(win32_context, context, *instruction);
        context->fatal_halt_reached = true;
        RecoverFromHleExit(win32_context, context);
        return true;
    }

    return false;
}

void RecordPortIo(ThreadContext* context,
                  std::uint32_t address,
                  std::uint32_t opcode,
                  std::uint16_t port,
                  std::uint32_t width,
                  std::uint32_t value,
                  bool is_input,
                  bool handled,
                  const std::string& result)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->port_io.observed_count;
    context->port_io.last_address = address;
    context->port_io.last_opcode = opcode;
    context->port_io.last_port = port;
    context->port_io.last_width = width;
    context->port_io.last_value = value;
    context->port_io.last_is_input = is_input;
    context->port_io.last_handled = handled;
    context->port_io.last_result = result;
    if (context->port_io.trace_stored_count < kWin32PortIoTraceCapacity)
    {
        Win32PortIoTraceEntry& entry =
            context->port_io.trace[context->port_io.trace_stored_count];
        entry.valid = true;
        entry.sequence = context->port_io.observed_count;
        entry.address = address;
        entry.opcode = opcode;
        entry.port = port;
        entry.width = width;
        entry.value = value;
        entry.is_input = is_input;
        entry.handled = handled;
        ++context->port_io.trace_stored_count;
    }
}

bool IsObservedPortInitializationWrite(std::uint16_t port,
                                       std::uint32_t width,
                                       std::uint32_t value)
{
    if (width != 4)
    {
        return false;
    }

    return (port == 0x02AC && value == 0x00000010U) ||
           (port == 0x02A0 && value == 0x00000001U) ||
           (port == 0x02A2 && value == 0x00000000U);
}

bool IsPortIoTraceCandidate(std::uint16_t port,
                            std::uint32_t width,
                            bool is_input)
{
    return !is_input && width == 4 && port >= 0x02A0 && port <= 0x02AF;
}

bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x66 ||
        (instruction[1] != 0xEF && instruction[1] != 0xED))
    {
        return false;
    }

    const std::uint16_t port = static_cast<std::uint16_t>(
        win32_context->Edx & 0xFFFFU);
    const bool is_input = instruction[1] == 0xED;
    const std::uint32_t value = is_input ? 0 : win32_context->Eax;
    const std::uint32_t width = 4;
    const std::uint32_t opcode = is_input ? 0x66ED : 0x66EF;
    if (is_input)
    {
        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     true,
                     false,
                     "unsupported-in");
        std::ostringstream stream;
        stream << "unsupported port I/O IN EAX,DX port=0x"
               << std::hex << static_cast<unsigned>(port);
        context->hle_message = stream.str();
        return false;
    }

    if (IsObservedPortInitializationWrite(port, width, value))
    {
        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "ignored");
        win32_context->Eip += 2;
        return true;
    }

    if (IsPortIoTraceCandidate(port, width, false))
    {
        if (context->port_io.observed_count >= kWin32DeferredPortIoLimit)
        {
            context->port_io.trace_limit_reached = true;
            RecordPortIo(context,
                         static_cast<std::uint32_t>(win32_context->Eip),
                         opcode,
                         port,
                         width,
                         value,
                         false,
                         false,
                         "deferred-limit");
            std::ostringstream stream;
            stream << "deferred port I/O limit reached for OUT DX,EAX port=0x"
                   << std::hex << static_cast<unsigned>(port)
                   << " value=0x" << value;
            context->hle_message = stream.str();
            return false;
        }

        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "deferred-ignored");
        win32_context->Eip += 2;
        return true;
    }

    RecordPortIo(context,
                 static_cast<std::uint32_t>(win32_context->Eip),
                 opcode,
                 port,
                 width,
                 value,
                 false,
                 false,
                 "unsupported");
    std::ostringstream stream;
    stream << "unsupported port I/O OUT DX,EAX port=0x";
    stream << std::hex << static_cast<unsigned>(port)
           << " value=0x" << value;
    context->hle_message = stream.str();
    return false;
}

bool HandleDosHleInstruction(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] == 0xCD && instruction[1] == 0x21)
    {
        return HandleDosInterrupt21(win32_context, context);
    }
    if (instruction[0] == 0xCD && instruction[1] == 0x2F)
    {
        return HandleDosInterrupt2F(win32_context, context);
    }
    if (instruction[0] == 0xCD && instruction[1] == 0x31)
    {
        return HandleDpmiInterrupt31(win32_context, context);
    }
    if (instruction[0] == 0xCD && instruction[1] == 0x33)
    {
        return HandleMouseInterrupt33(win32_context, context);
    }

    if (instruction[0] == 0xCD)
    {
        std::ostringstream stream;
        stream << "unsupported DOS interrupt 0x"
               << std::hex << static_cast<unsigned>(instruction[1]);
        context->hle_message = stream.str();
    }
    return false;
}

bool HandleDosMemoryAccess(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] == 0x66 && instruction[1] == 0x26 &&
        instruction[2] == 0x8B && instruction[3] == 0x0D)
    {
        const std::uint32_t offset =
            static_cast<std::uint32_t>(instruction[4]) |
            (static_cast<std::uint32_t>(instruction[5]) << 8) |
            (static_cast<std::uint32_t>(instruction[6]) << 16) |
            (static_cast<std::uint32_t>(instruction[7]) << 24);
        std::uint16_t value = 0;
        if (offset == 0x2C)
        {
            value = context->linexe_environment_active ? 0x002CU : 0;
        }
        win32_context->Ecx =
            (win32_context->Ecx & 0xFFFF0000U) | value;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[2],
                              offset,
                              value);
        win32_context->Eip += 8;
        return true;
    }
    if (instruction[0] == 0x66 && instruction[1] == 0x26 &&
        instruction[2] == 0x8C && instruction[3] == 0x1D)
    {
        const std::uint32_t offset =
            static_cast<std::uint32_t>(instruction[4]) |
            (static_cast<std::uint32_t>(instruction[5]) << 8) |
            (static_cast<std::uint32_t>(instruction[6]) << 16) |
            (static_cast<std::uint32_t>(instruction[7]) << 24);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[2],
                              offset,
                              0);
        win32_context->Eip += 8;
        return true;
    }
    if (instruction[0] == 0x26 && instruction[1] == 0x8A &&
        instruction[2] == 0x4F && instruction[3] == 0xFF)
    {
        win32_context->Ecx &= 0xFFFFFF00U;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[1],
                              win32_context->Edi - 1,
                              0);
        win32_context->Eip += 4;
        return true;
    }
    if (instruction[0] == 0x8B && instruction[1] == 0x06 &&
        win32_context->Esi < 0x10000)
    {
        win32_context->Eax = 0;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Esi,
                              0);
        win32_context->Eip += 2;
        return true;
    }
    if (instruction[0] == 0x80 && instruction[1] == 0x3E &&
        instruction[2] == 0x00 && win32_context->Esi < 0x10000)
    {
        win32_context->EFlags |= 0x40U;
        win32_context->EFlags &= ~1U;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Esi,
                              0);
        win32_context->Eip += 3;
        return true;
    }
    if (instruction[0] == 0xAC && win32_context->Esi < 0x10000)
    {
        win32_context->Eax &= 0xFFFFFF00U;
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Esi,
                              0);
        ++win32_context->Esi;
        ++win32_context->Eip;
        return true;
    }
    if (instruction[0] == 0xA4 && win32_context->Esi < 0x10000)
    {
        char* destination = reinterpret_cast<char*>(
            static_cast<std::uintptr_t>(win32_context->Edi));
        if (destination != nullptr &&
            IsGuestRangeReadable(context, destination, 1))
        {
            *destination = '\0';
        }
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              win32_context->Edi,
                              0);
        ++win32_context->Esi;
        ++win32_context->Edi;
        ++win32_context->Eip;
        return true;
    }

    return false;
}

extern "C" __declspec(naked) void
RecoverHostStackException()
{
    __asm
    {
        xor eax, eax
        ret
    }
}

void RecordAllocatorControlFlowException(
    EXCEPTION_POINTERS* exception_info,
    ThreadContext* context)
{
    if (exception_info == nullptr || context == nullptr ||
        exception_info->ContextRecord == nullptr)
    {
        return;
    }

    const std::uint32_t eip = static_cast<std::uint32_t>(
        exception_info->ContextRecord->Eip);
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    if (eip < context->runtime_base ||
        static_cast<std::uint64_t>(eip) + 4U > runtime_end)
    {
        return;
    }

    const std::uint32_t eip_offset = eip - context->runtime_base;
    constexpr std::uint32_t kAllocatorTraceBegin = 0x000F7A60U;
    constexpr std::uint32_t kAllocatorTraceEnd = 0x000F7AD5U;
    if (eip_offset < kAllocatorTraceBegin ||
        eip_offset >= kAllocatorTraceEnd)
    {
        return;
    }

    Win32AllocatorControlFlowObservation& observation =
        context->allocator_control_flow;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32AllocatorControlFlowTraceCapacity;
    Win32AllocatorControlFlowTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.eip_offset = eip_offset;
    entry.seh_code = exception_info->ExceptionRecord != nullptr
        ? exception_info->ExceptionRecord->ExceptionCode
        : 0;
    const std::uint8_t* instruction =
        reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(eip));
    std::memcpy(entry.opcode, instruction, sizeof(entry.opcode));
    entry.eax = exception_info->ContextRecord->Eax;
    entry.ebx = exception_info->ContextRecord->Ebx;
    entry.edx = exception_info->ContextRecord->Edx;
    entry.esi = exception_info->ContextRecord->Esi;
    entry.edi = exception_info->ContextRecord->Edi;
    entry.eflags = exception_info->ContextRecord->EFlags;
    entry.pending_valid = context->pending_shadow_allocation_valid;
    entry.pending_size = context->pending_shadow_allocation_size;
    observation.observed_count = sequence;
    if (observation.trace_stored_count <
        kWin32AllocatorControlFlowTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_wrapped = true;
    }
}

bool HandleLinexeFarTransferBoundary(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->linexe_environment_active)
    {
        return false;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    constexpr std::uint8_t kFarTransferPrefix[] =
        {0x66U, 0xEAU, 0x04U, 0x00U};
    if (!IsGuestRangeReadable(
            context, instruction, sizeof(kFarTransferPrefix) + 2U) ||
        std::memcmp(instruction,
                    kFarTransferPrefix,
                    sizeof(kFarTransferPrefix)) != 0)
    {
        return false;
    }

    const std::uint16_t target_selector = static_cast<std::uint16_t>(
        win32_context->Edi >> 16U);
    const std::uint16_t target_offset = static_cast<std::uint16_t>(
        win32_context->Edi & 0xFFFFU);
    repiu::hle::LinexeService service{};
    if (!repiu::hle::DecodeLinexeOriginalExport(
            context->linexe_gate_plan,
            target_selector,
            target_offset,
            &service))
    {
        return false;
    }

    ++context->linexe_bridge_entry_count;
    context->linexe_bridge_gate_valid = true;
    context->linexe_bridge_selector = target_selector;
    context->linexe_bridge_offset = target_offset;
    context->linexe_bridge_service = static_cast<std::uint32_t>(service);
    context->linexe_bridge_esp = win32_context->Esp;
    context->linexe_bridge_ebp = win32_context->Ebp;
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context,
                             stack,
                             sizeof(context->linexe_bridge_stack)))
    {
        std::memcpy(context->linexe_bridge_stack,
                    stack,
                    sizeof(context->linexe_bridge_stack));
    }
    std::memset(context->linexe_bridge_argument_text,
                0,
                sizeof(context->linexe_bridge_argument_text));
    std::memset(context->linexe_bridge_stack_text,
                0,
                sizeof(context->linexe_bridge_stack_text));
    for (std::size_t stack_index = 0;
         stack_index < std::size(context->linexe_bridge_stack);
         ++stack_index)
    {
        const auto* candidate = reinterpret_cast<const char*>(
            static_cast<std::uintptr_t>(
                context->linexe_bridge_stack[stack_index]));
        std::size_t text_length = 0;
        for (; text_length + 1U <
                   sizeof(context->linexe_bridge_stack_text[stack_index]);
             ++text_length)
        {
            if (!IsGuestRangeReadable(context, candidate + text_length, 1U))
            {
                break;
            }
            const unsigned char value = static_cast<unsigned char>(
                candidate[text_length]);
            if (value == 0)
            {
                break;
            }
            if (!std::isprint(value))
            {
                text_length = 0;
                break;
            }
            context->linexe_bridge_stack_text[stack_index][text_length] =
                static_cast<char>(value);
        }
        if (text_length == 0)
        {
            context->linexe_bridge_stack_text[stack_index][0] = '\0';
        }
    }
    const auto* argument = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(context->linexe_bridge_stack[9]));
    for (std::size_t index = 0;
         index + 1U < sizeof(context->linexe_bridge_argument_text);
         ++index)
    {
        if (!IsGuestRangeReadable(context, argument + index, 1U))
        {
            break;
        }
        context->linexe_bridge_argument_text[index] = argument[index];
        if (argument[index] == '\0')
        {
            break;
        }
    }

    constexpr std::uint32_t kVirtualGlideModuleHandle = 1U;
    // The bridge consumes its three dwords and restores the ES value saved by
    // the wrapper.  The shared epilogue then owns EBX/ESI/EDI/EBP and RET.
    const bool is_glide_module =
        _stricmp(context->linexe_bridge_argument_text, "glide2x.ovl") == 0;
    const repiu::hle::GlideExportGate* glide_export =
        repiu::hle::FindGlideExportByName(
            context->glide_gate_plan,
            context->linexe_bridge_stack_text[12]);
    if (service == repiu::hle::LinexeService::kGetProcedureAddress &&
        context->linexe_bridge_stack[11] == kVirtualGlideModuleHandle &&
        glide_export != nullptr)
    {
        const std::uint32_t result_pointer =
            context->linexe_bridge_stack[13];
        const std::uint32_t gate_address =
            context->linexe_arena_layout.gate_code_base +
            glide_export->gate_offset;
        const std::uint32_t procedure_pointer[2] = {
            gate_address,
            static_cast<std::uint32_t>(win32_context->SegCs),
        };
        if (!WriteGuestBytes(
                context,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(result_pointer)),
                procedure_pointer,
                sizeof(procedure_pointer)))
        {
            return false;
        }

        ++context->linexe_get_proc_count;
        context->linexe_get_proc_result_pointer = gate_address;
        std::strncpy(context->linexe_get_proc_name,
                     glide_export->name.c_str(),
                     sizeof(context->linexe_get_proc_name) - 1U);
        win32_context->Eax = 1U;
        context->guest_es = static_cast<std::uint16_t>(
            context->linexe_bridge_stack[5] & 0xFFFFU);
        win32_context->Ebx = context->linexe_bridge_stack[6];
        win32_context->Esi = context->linexe_bridge_stack[7];
        win32_context->Edi = context->linexe_bridge_stack[8];
        win32_context->Ebp = context->linexe_bridge_stack[9];
        win32_context->Eip = context->linexe_bridge_stack[10];
        win32_context->Esp += 11U * sizeof(std::uint32_t);
        return true;
    }
    if (service != repiu::hle::LinexeService::kLoadModule ||
        !is_glide_module)
    {
        return false;
    }

    ++context->linexe_virtual_module_load_count;
    context->linexe_virtual_module_handle = kVirtualGlideModuleHandle;
    win32_context->Eax = kVirtualGlideModuleHandle;
    context->guest_es = static_cast<std::uint16_t>(
        context->linexe_bridge_stack[3] & 0xFFFFU);
    win32_context->Ebx = context->linexe_bridge_stack[4];
    win32_context->Esi = context->linexe_bridge_stack[5];
    win32_context->Edi = context->linexe_bridge_stack[6];
    win32_context->Ebp = context->linexe_bridge_stack[7];
    win32_context->Eip = context->linexe_bridge_stack[8];
    win32_context->Esp += 9U * sizeof(std::uint32_t);
    return true;
}

bool HandleGlideGateBoundary(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint32_t gate_begin =
        context != nullptr
            ? context->linexe_arena_layout.gate_code_base +
                context->glide_gate_plan.first_gate_offset
            : 0U;
    if (win32_context == nullptr || context == nullptr ||
        !context->linexe_environment_active ||
        win32_context->Eip < gate_begin)
    {
        return false;
    }

    const std::uint32_t gate_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->linexe_arena_layout.gate_code_base;
    const repiu::hle::GlideExportGate* glide_export =
        repiu::hle::DecodeGlideGate(context->glide_gate_plan, gate_offset);
    if (glide_export == nullptr)
    {
        return false;
    }

    ++context->glide_gate_entry_count;
    context->glide_backend.PumpEvents();
    context->glide_gate_ordinal = glide_export->ordinal;
    context->glide_gate_argument_bytes = glide_export->argument_byte_count;
    std::memset(context->glide_gate_name,
                0,
                sizeof(context->glide_gate_name));
    std::strncpy(context->glide_gate_name,
                 glide_export->name.c_str(),
                 sizeof(context->glide_gate_name) - 1U);
    context->glide_gate_esp = win32_context->Esp;
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context,
                             stack,
                             sizeof(context->glide_gate_stack)))
    {
        std::memcpy(context->glide_gate_stack,
                    stack,
                    sizeof(context->glide_gate_stack));
    }
    if (glide_export->ordinal < context->glide_call_counts.size())
    {
        const std::size_t ordinal = glide_export->ordinal;
        if (context->glide_call_counts[ordinal]++ == 0U)
        {
            context->glide_call_names[ordinal] = glide_export->name;
            std::copy(std::begin(context->glide_gate_stack),
                      std::end(context->glide_gate_stack),
                      context->glide_first_stacks[ordinal].begin());
        }
    }
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ordinal,
            static_cast<long>(glide_export->ordinal));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_esp,
            static_cast<long>(win32_context->Esp));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ebx,
            static_cast<long>(win32_context->Ebx));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ecx,
            static_cast<long>(win32_context->Ecx));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_edx,
            static_cast<long>(win32_context->Edx));
        for (std::size_t index = 0; index < 8U; ++index)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->glide_gate_stack[index],
                static_cast<long>(context->glide_gate_stack[index]));
        }
    }
    const std::uint32_t return_address = context->glide_gate_stack[0];
    if (!IsGuestInstructionPointer(context, return_address))
    {
        return false;
    }
    const repiu::hle::GlideSignature* signature =
        repiu::hle::FindGlideSignature(glide_export->name);
    if (signature == nullptr ||
        signature->argument_byte_count !=
            glide_export->argument_byte_count)
    {
        return false;
    }
    if (glide_export->name == "_GRGLIDEINIT@0")
    {
        context->glide_state.initialized = true;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTQUERYHARDWARE@4")
    {
        void* configuration = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
        if (!WriteGuestUInt32(context, configuration, 1U))
        {
            return false;
        }
        ++context->glide_gate_handled_count;
        win32_context->Eax = 1U;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTSELECT@4" &&
        context->glide_gate_stack[1] == 0U)
    {
        context->glide_state.selected_board = 0U;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTWINOPEN@28")
    {
        const std::uint32_t window = context->glide_gate_stack[1];
        const std::uint32_t resolution = context->glide_gate_stack[2];
        const std::uint32_t refresh = context->glide_gate_stack[3];
        const std::uint32_t color_format = context->glide_gate_stack[4];
        const std::uint32_t origin = context->glide_gate_stack[5];
        const std::uint32_t color_buffers = context->glide_gate_stack[6];
        const std::uint32_t auxiliary_buffers =
            context->glide_gate_stack[7];
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        const bool mode_supported = window == 0U && refresh == 0U &&
            repiu::hle::DecodeGlideResolution(
                resolution, &width, &height);
        const bool opened = mode_supported &&
            context->glide_backend.OpenWindowed(
                width, height, color_buffers, auxiliary_buffers);
        context->glide_backend_message = context->glide_backend.message();
        if (opened)
        {
            ++context->glide_window_open_count;
            context->glide_logical_width = width;
            context->glide_logical_height = height;
            context->glide_state.window_open = true;
            context->glide_state.width = width;
            context->glide_state.height = height;
            context->glide_state.color_format = color_format;
            context->glide_state.origin = origin;
            context->glide_state.color_buffer_count = color_buffers;
            context->glide_state.auxiliary_buffer_count = auxiliary_buffers;
        }
        win32_context->Eax = opened ? 1U : 0U;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 8U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRSSTSCREENWIDTH@0" ||
        glide_export->name == "_GRSSTSCREENHEIGHT@0")
    {
        win32_context->Eax = glide_export->name == "_GRSSTSCREENWIDTH@0"
            ? context->glide_state.width
            : context->glide_state.height;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXMINADDRESS@4" &&
        context->glide_gate_stack[1] == 0U)
    {
        ++context->glide_gate_handled_count;
        win32_context->Eax = 0U;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRTEXMAXADDRESS@4" &&
        context->glide_gate_stack[1] == 0U)
    {
        std::uint32_t maximum_address = 0;
        if (!repiu::hle::CalculateGlideTextureMaxAddress(
                context->glide_state.texture_memory_bytes,
                &maximum_address))
        {
            return false;
        }
        ++context->glide_gate_handled_count;
        win32_context->Eax = maximum_address;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCOLORMASK@8")
    {
        const bool rgb = context->glide_gate_stack[1] != 0U;
        const bool alpha = context->glide_gate_stack[2] != 0U;
        if (!context->glide_backend.SetColorMask(rgb, alpha))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 3U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRRENDERBUFFER@4")
    {
        if (!context->glide_backend.SetRenderBuffer(
                context->glide_gate_stack[1]))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHMASK@4")
    {
        if (!context->glide_backend.SetDepthMask(
                context->glide_gate_stack[1] != 0U))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHBUFFERMODE@4")
    {
        if (!context->glide_backend.SetDepthBufferMode(
                context->glide_gate_stack[1]))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRLFBWRITECOLORFORMAT@4")
    {
        context->glide_state.lfb_write_color_format =
            context->glide_gate_stack[1];
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRALPHACOMBINE@20")
    {
        repiu::hle::GlideAlphaCombineState state;
        state.function = context->glide_gate_stack[1];
        state.factor = context->glide_gate_stack[2];
        state.local = context->glide_gate_stack[3];
        state.other = context->glide_gate_stack[4];
        state.invert = context->glide_gate_stack[5] != 0U;
        state.valid = true;
        if (!context->glide_backend.SetAlphaCombine(state))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.alpha_combine = state;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 6U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCOLORCOMBINE@20")
    {
        repiu::hle::GlideColorCombineState state;
        state.function = context->glide_gate_stack[1];
        state.factor = context->glide_gate_stack[2];
        state.local = context->glide_gate_stack[3];
        state.other = context->glide_gate_stack[4];
        state.invert = context->glide_gate_stack[5] != 0U;
        state.valid = true;
        if (!context->glide_backend.SetColorCombine(state))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.color_combine = state;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 6U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRALPHABLENDFUNCTION@16")
    {
        repiu::hle::GlideAlphaBlendState state;
        state.rgb_source = context->glide_gate_stack[1];
        state.rgb_destination = context->glide_gate_stack[2];
        state.alpha_source = context->glide_gate_stack[3];
        state.alpha_destination = context->glide_gate_stack[4];
        state.valid = true;
        if (!context->glide_backend.SetAlphaBlend(state))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.alpha_blend = state;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 5U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRALPHATESTFUNCTION@4")
    {
        const std::uint32_t function = context->glide_gate_stack[1];
        if (!context->glide_backend.SetAlphaTestFunction(function))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.alpha_test_function = function;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDEPTHBUFFERFUNCTION@4")
    {
        const std::uint32_t function = context->glide_gate_stack[1];
        if (!context->glide_backend.SetDepthBufferFunction(function))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.depth_buffer_function = function;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRFOGMODE@4")
    {
        const std::uint32_t mode = context->glide_gate_stack[1];
        if (!context->glide_backend.SetFogMode(mode))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.fog_mode = mode;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCLIPWINDOW@16")
    {
        const std::uint32_t min_x = context->glide_gate_stack[1];
        const std::uint32_t min_y = context->glide_gate_stack[2];
        const std::uint32_t max_x = context->glide_gate_stack[3];
        const std::uint32_t max_y = context->glide_gate_stack[4];
        if (!context->glide_backend.SetClipWindow(
                min_x, min_y, max_x, max_y))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.clip_min_x = min_x;
        context->glide_state.clip_min_y = min_y;
        context->glide_state.clip_max_x = max_x;
        context->glide_state.clip_max_y = max_y;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 5U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRCULLMODE@4")
    {
        const std::uint32_t mode = context->glide_gate_stack[1];
        if (!context->glide_backend.SetCullMode(mode))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.cull_mode = mode;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRGLIDEGETSTATE@4")
    {
        repiu::hle::GlideStateImage image;
        void* output = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
            context->glide_gate_stack[1]));
        if (!repiu::hle::BuildGlideStateImage(context->glide_state, &image) ||
            !WriteGuestBytes(context, output, image.data(), image.size()))
        {
            return false;
        }
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRGLIDESETSTATE@4")
    {
        repiu::hle::GlideStateImage image;
        const void* input = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
        repiu::hle::GlideLogicalState restored = context->glide_state;
        if (!IsGuestRangeReadable(context, input, image.size()))
        {
            return false;
        }
        std::memcpy(image.data(), input, image.size());
        if (!repiu::hle::ParseGlideStateImage(image, &restored))
        {
            return false;
        }
        context->glide_state = restored;
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    if (glide_export->name == "_GRDITHERMODE@4")
    {
        const std::uint32_t mode = context->glide_gate_stack[1];
        if (!context->glide_backend.SetDitherMode(mode))
        {
            context->glide_backend_message =
                context->glide_backend.message();
            return false;
        }
        context->glide_state.dither_mode = mode;
        context->glide_backend_message = context->glide_backend.message();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp += 2U * sizeof(std::uint32_t);
        return true;
    }
    return false;
}

bool IsAotCacheAddress(const ThreadContext* context, std::uint32_t address)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        !context->aot_placement->placed)
    {
        return false;
    }
    const std::uint64_t end =
        static_cast<std::uint64_t>(context->aot_placement->base_address) +
        context->aot_placement->size;
    return address >= context->aot_placement->base_address && address < end;
}

DWORD WINAPI AotTranslationWorkerProc(void* parameter)
{
    ThreadContext* context = static_cast<ThreadContext*>(parameter);
    if (context == nullptr || context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return 1;
    }
    for (;;)
    {
        if (WaitForSingleObject(context->aot_translation_request_event,
                                INFINITE) != WAIT_OBJECT_0)
        {
            return 2;
        }
        if (context->aot_translation_shutdown.load(std::memory_order_acquire))
        {
            return 0;
        }
        const auto operation = static_cast<AotWorkerOperation>(
            context->aot_worker_operation.load(std::memory_order_acquire));
        if (operation == AotWorkerOperation::kPatchInlineCache)
        {
            context->aot_inline_cache_patch_result =
                Win32AotInlineCachePatchResult{};
            PatchWin32AotIndirectInlineCache(
                context->aot_placement,
                context->aot_patch_cache_miss_address.load(
                    std::memory_order_acquire),
                context->aot_patch_guest_target.load(
                    std::memory_order_acquire),
                context->aot_patch_cache_target.load(
                    std::memory_order_acquire),
                &context->aot_inline_cache_patch_result);
        }
        else if (operation == AotWorkerOperation::kRetireGuestPage)
        {
            context->aot_guest_page_retire_result =
                Win32AotGuestPageRetireResult{};
            RetireWin32AotGuestPage(
                context->aot_placement,
                context->aot_retire_guest_page.load(
                    std::memory_order_acquire),
                context->aot_retire_quarantine.load(
                    std::memory_order_acquire),
                &context->aot_guest_page_retire_result);
        }
        else
        {
            const std::uint32_t target = context->aot_translation_target.load(
                std::memory_order_acquire);
            context->aot_translation_result = Win32AotDynamicAppendResult{};
            AppendWin32DynamicAotTranslation(
                context->runtime_base, context->runtime_size, target,
                context->aot_excluded_guest_ranges,
                &context->aot_page_write_watch, context->aot_placement,
                &context->aot_translation_result);
            if (context->aot_translation_result.unsafe_failure)
            {
                context->aot_terminal_failure.store(
                    true, std::memory_order_release);
            }
        }
        SetEvent(context->aot_translation_complete_event);
    }
}

bool RequestAotDynamicTranslation(ThreadContext* context,
                                  std::uint32_t target,
                                  std::uint32_t* cache_entry,
                                  std::uint32_t* added_bytes)
{
    if (context == nullptr || cache_entry == nullptr || added_bytes == nullptr ||
        context->aot_translation_thread == nullptr ||
        context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return false;
    }
    ResetEvent(context->aot_translation_complete_event);
    context->aot_worker_operation.store(
        static_cast<std::uint32_t>(AotWorkerOperation::kTranslate),
        std::memory_order_release);
    context->aot_translation_target.store(target, std::memory_order_release);
    if (SetEvent(context->aot_translation_request_event) == 0 ||
        WaitForSingleObject(context->aot_translation_complete_event,
                            INFINITE) != WAIT_OBJECT_0)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    if (context->aot_translation_result.unsafe_failure)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    if (!context->aot_translation_result.appended)
    {
        return false;
    }
    *cache_entry = context->aot_translation_result.cache_entry;
    *added_bytes = context->aot_translation_result.added_bytes;
    return true;
}

void ReleaseUnneededWin32AotGuestPageWatches(ThreadContext* context,
                                             std::uint32_t address,
                                             std::uint32_t size)
{
    if (context == nullptr || context->aot_placement == nullptr) return;

    constexpr std::uint32_t kPageMask = 0xFFFFF000U;
    const std::uint32_t first_page = address & kPageMask;
    const std::uint64_t end = static_cast<std::uint64_t>(address) + size;
    const std::uint32_t last_page = static_cast<std::uint32_t>((end - 1U) & kPageMask);

    for (std::uint32_t page = first_page; page <= last_page; page += 0x1000U)
    {
        bool relevant = Win32AotGuestRangeHasActiveTranslation(
            *context->aot_placement, page, 0x1000U);
        if (!relevant)
        {
            relevant = IsWin32AotGuestPageRetired(*context->aot_placement, page) ||
                       IsWin32AotGuestPageQuarantined(*context->aot_placement, page);
        }
        if (!relevant)
        {
            RemoveWin32AotPageWriteWatch(&context->aot_page_write_watch, page);
        }
    }
}

bool HandleAotGuestCodeWriteCompletion(EXCEPTION_POINTERS* exception_info,
                                       CONTEXT* win32_context,
                                       ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        !HasPendingWin32AotGuestWrite(context->aot_page_write_watch) ||
        exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
    {
        return false;
    }
    Win32AotGuestWriteCompletion completion;
    if (!CompleteWin32AotGuestWrite(
            &context->aot_page_write_watch, &completion) ||
        !NoteSuccessfulAotGuestWrite(
            context, completion.destination, completion.byte_count))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    ReleaseUnneededWin32AotGuestPageWatches(context, completion.destination, completion.byte_count);
    if (completion.keep_single_step ||
        (completion.from_guest && context->aot_reentry_pending))
    {
        win32_context->EFlags |= 0x00000100U;
    }
    else
    {
        win32_context->EFlags &= ~0x00000100U;
    }
    return true;
}

bool HandleAotGuestCodeWriteFault(EXCEPTION_POINTERS* exception_info,
                                  CONTEXT* win32_context,
                                  ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        exception_info->ExceptionRecord->ExceptionCode !=
            EXCEPTION_ACCESS_VIOLATION ||
        exception_info->ExceptionRecord->NumberParameters < 2U ||
        exception_info->ExceptionRecord->ExceptionInformation[0] != 1U)
    {
        return false;
    }
    const std::uintptr_t destination_value =
        exception_info->ExceptionRecord->ExceptionInformation[1];
    if (destination_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t destination =
        static_cast<std::uint32_t>(destination_value);
    if (!IsWin32AotGuestPageWriteWatched(
            context->aot_page_write_watch, destination))
    {
        return false;
    }
    const std::uint32_t execution_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    const bool from_guest = IsGuestInstructionPointer(
        context, execution_address);
    if (!from_guest && !IsAotCacheAddress(context, execution_address))
    {
        return false;
    }
    const bool keep_single_step =
        (win32_context->EFlags & 0x00000100U) != 0U ||
        context->enable_single_step_trace ||
        context->aot_reentry_pending || context->aot_legacy_fallback;
    if (!BeginWin32AotGuestWrite(
            &context->aot_page_write_watch, execution_address, destination,
            from_guest, keep_single_step,
            AotGuestAddressForExecutionAddress(context, execution_address)))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    context->exception_dispatch_last_eip.store(
        execution_address, std::memory_order_relaxed);
    win32_context->EFlags |= 0x00000100U;
    return true;
}

bool RequestAotInlineCachePatch(ThreadContext* context,
                                std::uint32_t cache_miss_address,
                                std::uint32_t guest_target,
                                std::uint32_t cache_target)
{
    if (context == nullptr || context->aot_translation_thread == nullptr ||
        context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return false;
    }
    ResetEvent(context->aot_translation_complete_event);
    context->aot_patch_cache_miss_address.store(
        cache_miss_address, std::memory_order_release);
    context->aot_patch_guest_target.store(guest_target,
                                           std::memory_order_release);
    context->aot_patch_cache_target.store(cache_target,
                                           std::memory_order_release);
    context->aot_worker_operation.store(
        static_cast<std::uint32_t>(AotWorkerOperation::kPatchInlineCache),
        std::memory_order_release);
    if (SetEvent(context->aot_translation_request_event) == 0 ||
        WaitForSingleObject(context->aot_translation_complete_event,
                            INFINITE) != WAIT_OBJECT_0)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    return context->aot_inline_cache_patch_result.patched;
}

bool RequestAotGuestPageRetirement(ThreadContext* context,
                                   std::uint32_t guest_page,
                                   bool quarantine)
{
    if (context == nullptr || context->aot_translation_thread == nullptr ||
        context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return false;
    }
    ResetEvent(context->aot_translation_complete_event);
    context->aot_retire_guest_page.store(
        guest_page, std::memory_order_release);
    context->aot_retire_quarantine.store(
        quarantine, std::memory_order_release);
    context->aot_worker_operation.store(
        static_cast<std::uint32_t>(AotWorkerOperation::kRetireGuestPage),
        std::memory_order_release);
    if (SetEvent(context->aot_translation_request_event) == 0 ||
        WaitForSingleObject(context->aot_translation_complete_event,
                            INFINITE) != WAIT_OBJECT_0)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    return context->aot_guest_page_retire_result.retired;
}

std::uint32_t AotGuestAddressForExecutionAddress(
    const ThreadContext* context,
    std::uint32_t execution_address)
{
    if (context == nullptr)
    {
        return 0U;
    }
    if (IsGuestInstructionPointer(context, execution_address))
    {
        return execution_address;
    }
    std::uint32_t guest_address = 0U;
    if (context->aot_placement != nullptr &&
        FindAotGuestAddress(*context->aot_placement,
                            execution_address, &guest_address))
    {
        return guest_address;
    }
    return 0U;
}

bool NoteSuccessfulAotGuestWrite(ThreadContext* context,
                                 std::uint32_t destination,
                                 std::uint32_t byte_count)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        context->aot_translation_thread == nullptr || byte_count == 0U)
    {
        return true;
    }
    constexpr std::uint32_t kPageMask = 0xFFFFF000U;
    const std::uint32_t first_page = destination & kPageMask;
    const std::uint64_t write_end =
        static_cast<std::uint64_t>(destination) + byte_count;
    if (write_end == 0U ||
        write_end > static_cast<std::uint64_t>(
                        std::numeric_limits<std::uint32_t>::max()) + 1U)
    {
        return false;
    }
    const std::uint32_t last_page = static_cast<std::uint32_t>(
        (write_end - 1U) & kPageMask);
    bool relevant = Win32AotGuestRangeHasActiveTranslation(
        *context->aot_placement, destination, byte_count);
    for (std::uint32_t page = first_page;
         !relevant; page += 0x1000U)
    {
        relevant = IsWin32AotGuestPageRetired(
                       *context->aot_placement, page) ||
            IsWin32AotGuestPageQuarantined(
                *context->aot_placement, page);
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    if (!relevant)
    {
        return true;
    }
    const std::uint32_t source = AotGuestAddressForExecutionAddress(
        context,
        context->exception_dispatch_last_eip.load(
            std::memory_order_relaxed));
    bool observed = false;
    bool retired_provenance = false;
    for (std::uint32_t page = first_page;; page += 0x1000U)
    {
        const std::uint32_t range_begin = std::max(destination, page);
        const std::uint64_t page_end =
            static_cast<std::uint64_t>(page) + 0x1000U;
        const std::uint32_t range_size = static_cast<std::uint32_t>(
            std::min(write_end, page_end) - range_begin);
        const bool active = Win32AotGuestRangeHasActiveTranslation(
            *context->aot_placement, range_begin, range_size);
        retired_provenance = retired_provenance ||
            IsWin32AotGuestPageRetired(*context->aot_placement, page) ||
            IsWin32AotGuestPageQuarantined(
                *context->aot_placement, page);
        if (active)
        {
            const bool same_page = source == 0U ||
                (source & kPageMask) == page;
            context->aot_page_retire_attempt_count.fetch_add(
                1, std::memory_order_relaxed);
            if (!RequestAotGuestPageRetirement(
                    context, page, same_page))
            {
                context->aot_terminal_failure.store(
                    true, std::memory_order_release);
                return false;
            }
            context->aot_page_retire_success_count.fetch_add(
                1, std::memory_order_relaxed);
            context->aot_last_retired_page.store(
                page, std::memory_order_relaxed);
            if (same_page)
            {
                context->aot_quarantine_count.fetch_add(
                    1, std::memory_order_relaxed);
            }
            observed = true;
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    if (observed || retired_provenance)
    {
        context->aot_code_write_count.fetch_add(
            1, std::memory_order_relaxed);
        context->aot_last_code_write_source.store(
            source, std::memory_order_relaxed);
        context->aot_last_code_write_destination.store(
            destination, std::memory_order_relaxed);
    }
    return true;
}

bool IsAotInlineCacheMiss(const ThreadContext* context,
                          std::uint32_t cache_address)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        cache_address < context->aot_placement->base_address)
    {
        return false;
    }
    const std::uint32_t offset =
        cache_address - context->aot_placement->base_address;
    for (const auto& site :
         context->aot_placement->indirect_inline_cache_sites)
    {
        if (offset == site.miss_cache_offset ||
            offset == site.miss_cache_offset + 1U)
        {
            return true;
        }
    }
    return false;
}

bool IsAotHleBoundaryAddress(const ThreadContext* context,
                             std::uint32_t guest_address)
{
    if (context == nullptr)
    {
        return false;
    }
    for (const runtime::AotExcludedGuestRange& range :
         context->aot_excluded_guest_ranges)
    {
        const std::uint64_t end =
            static_cast<std::uint64_t>(range.guest_address) +
            range.byte_count;
        if (range.byte_count != 0U && guest_address >= range.guest_address &&
            guest_address < end)
        {
            return true;
        }
    }
    return false;
}

bool ResolveAotTransferTarget(ThreadContext* context,
                              std::uint32_t target,
                              std::uint32_t* cache_target,
                              bool force_generation = false)
{
    if (context == nullptr || cache_target == nullptr ||
        context->aot_placement == nullptr)
    {
        return false;
    }
    if (IsAotHleBoundaryAddress(context, target))
    {
        return false;
    }
    if (IsWin32AotGuestPageQuarantined(
            *context->aot_placement, target))
    {
        return false;
    }
    if (IsAotCacheAddress(context, target) ||
        FindAotCacheAddress(*context->aot_placement, target, cache_target))
    {
        return true;
    }
    const bool retired_target = force_generation ||
        IsWin32AotGuestPageRetired(*context->aot_placement, target) ||
        HasWin32AotRetiredGuestAddress(*context->aot_placement, target);
    std::uint32_t dynamic_cache_entry = 0;
    std::uint32_t dynamic_added_bytes = 0;
    if (context->aot_dynamic_translation_enabled)
    {
        context->aot_dynamic_attempt_count.fetch_add(
            1, std::memory_order_relaxed);
    }
    if ((!context->aot_dynamic_translation_enabled && !retired_target) ||
        !RequestAotDynamicTranslation(
            context, target, &dynamic_cache_entry, &dynamic_added_bytes))
    {
        if (retired_target)
        {
            context->aot_generation_failure_count.fetch_add(
                1, std::memory_order_relaxed);
            if (!context->aot_terminal_failure.load(
                    std::memory_order_acquire) &&
                RequestAotGuestPageRetirement(context, target, true))
            {
                context->aot_quarantine_count.fetch_add(
                    1, std::memory_order_relaxed);
            }
            else
            {
                context->aot_terminal_failure.store(
                    true, std::memory_order_release);
            }
        }
        return false;
    }
    context->aot_dynamic_success_count.fetch_add(
        1, std::memory_order_relaxed);
    context->aot_dynamic_added_bytes.fetch_add(
        dynamic_added_bytes, std::memory_order_relaxed);
    if (retired_target)
    {
        context->aot_generation_publish_count.fetch_add(
            1, std::memory_order_relaxed);
        context->aot_generation_relinked_entry_count.fetch_add(
            context->aot_translation_result.relinked_entry_count,
            std::memory_order_relaxed);
        context->aot_last_published_generation.store(
            context->aot_translation_result.generation,
            std::memory_order_relaxed);
    }
    *cache_target = dynamic_cache_entry;
    return true;
}

bool EvaluateAotCondition(std::uint8_t condition, std::uint32_t eflags)
{
    const bool carry = (eflags & 0x00000001U) != 0U;
    const bool parity = (eflags & 0x00000004U) != 0U;
    const bool zero = (eflags & 0x00000040U) != 0U;
    const bool sign = (eflags & 0x00000080U) != 0U;
    const bool overflow = (eflags & 0x00000800U) != 0U;
    switch (condition & 0x0FU)
    {
        case 0x0U: return overflow;
        case 0x1U: return !overflow;
        case 0x2U: return carry;
        case 0x3U: return !carry;
        case 0x4U: return zero;
        case 0x5U: return !zero;
        case 0x6U: return carry || zero;
        case 0x7U: return !carry && !zero;
        case 0x8U: return sign;
        case 0x9U: return !sign;
        case 0xAU: return parity;
        case 0xBU: return !parity;
        case 0xCU: return sign != overflow;
        case 0xDU: return sign == overflow;
        case 0xEU: return zero || sign != overflow;
        case 0xFU: return !zero && sign == overflow;
    }
    return false;
}

bool HandleAotConditionalTransfer(EXCEPTION_POINTERS* exception_info,
                                  CONTEXT* win32_context,
                                  ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr || !context->aot_reentry_pending ||
        exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
    {
        return false;
    }
    const std::uint32_t source = static_cast<std::uint32_t>(win32_context->Eip);
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(source));
    std::uint8_t condition = 0;
    std::uint32_t instruction_size = 0;
    std::int32_t displacement = 0;
    if (instruction[0] >= 0x70U && instruction[0] <= 0x7FU)
    {
        condition = instruction[0] & 0x0FU;
        instruction_size = 2U;
        displacement = static_cast<std::int8_t>(instruction[1]);
    }
    else if (instruction[0] == 0x0FU && instruction[1] >= 0x80U &&
             instruction[1] <= 0x8FU)
    {
        condition = instruction[1] & 0x0FU;
        instruction_size = 6U;
        std::memcpy(&displacement, instruction + 2U, sizeof(displacement));
    }
    else
    {
        return false;
    }
    const bool taken = EvaluateAotCondition(
        condition, static_cast<std::uint32_t>(win32_context->EFlags));
    const std::uint32_t target = taken
        ? source + instruction_size + displacement
        : source + instruction_size;
    std::uint32_t cache_target = target;
    if (!ResolveAotTransferTarget(context, target, &cache_target))
    {
        context->aot_last_indirect_source.store(source,
                                                 std::memory_order_relaxed);
        context->aot_last_indirect_target.store(target,
                                                 std::memory_order_relaxed);
        return false;
    }
    win32_context->Eip = cache_target;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_indirect_dispatch_count.fetch_add(1, std::memory_order_relaxed);
    context->aot_transfer_trace[
        context->aot_transfer_trace_count % kWin32AotTransferTraceCapacity] = {
            source, target, false};
    ++context->aot_transfer_trace_count;
    context->aot_last_indirect_source.store(source, std::memory_order_relaxed);
    context->aot_last_indirect_target.store(target, std::memory_order_relaxed);
    context->aot_reentry_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool HandleAotIndirectTransfer(EXCEPTION_POINTERS* exception_info,
                               CONTEXT* win32_context,
                               ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        !context->aot_reentry_pending ||
        exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
    {
        return false;
    }
    const std::uint32_t source = static_cast<std::uint32_t>(win32_context->Eip);
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(source));
    bool is_call = false;
    std::uint32_t target = 0;
    std::uint32_t instruction_size = 0;
    if (instruction[0] == 0xE8U || instruction[0] == 0xE9U)
    {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, instruction + 1, sizeof(displacement));
        instruction_size = 5U;
        target = source + instruction_size + displacement;
        is_call = instruction[0] == 0xE8U;
    }
    else if (instruction[0] == 0xEBU)
    {
        instruction_size = 2U;
        target = source + instruction_size +
            static_cast<std::int8_t>(instruction[1]);
    }
    else if (instruction[0] != 0xFFU)
    {
        return false;
    }
    else
    {
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        is_call = operation == 2U;
        if (!is_call && operation != 4U)
        {
            return false;
        }
        const std::uint8_t mod = instruction[1] >> 6;
        const std::uint8_t rm = instruction[1] & 0x07U;
        instruction_size = 2U;
        if (mod == 3U)
        {
            target = ReadGeneralRegister32(win32_context, rm);
        }
        else
        {
            std::uint32_t pointer_address = 0;
            if (!DecodeModRmMemoryAddress(win32_context, instruction,
                                          &pointer_address,
                                          &instruction_size) ||
                !ReadGuestUInt32(
                    context,
                    reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(pointer_address)),
                    &target))
            {
                return false;
            }
        }
    }
    std::uint32_t cache_target = target;
    if (!ResolveAotTransferTarget(context, target, &cache_target))
    {
        context->aot_last_indirect_source.store(source,
                                                 std::memory_order_relaxed);
        context->aot_last_indirect_target.store(target,
                                                 std::memory_order_relaxed);
        return false;
    }
    if (IsAotInlineCacheMiss(context, context->aot_reentry_cache_address))
    {
        context->aot_inline_cache_patch_attempt_count.fetch_add(
            1, std::memory_order_relaxed);
        if (RequestAotInlineCachePatch(
                context, context->aot_reentry_cache_address,
                target, cache_target))
        {
            context->aot_inline_cache_patch_success_count.fetch_add(
                1, std::memory_order_relaxed);
        }
    }
    if (is_call)
    {
        const std::uint32_t return_address = source + instruction_size;
        const std::uint32_t stack_address = win32_context->Esp - 4U;
        if (!WriteGuestUInt32(
                context,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(stack_address)),
                return_address))
        {
            return false;
        }
        win32_context->Esp = stack_address;
        if (context->aot_call_depth < ThreadContext::kAotCallFrameCapacity)
        {
            ThreadContext::AotCallFrame& frame =
                context->aot_call_frames[context->aot_call_depth++];
            frame.source = source;
            frame.target = target;
            frame.fallthrough = return_address;
            context->aot_last_call_source = source;
            context->aot_last_call_target = target;
        }
    }
    win32_context->Eip = cache_target;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_indirect_dispatch_count.fetch_add(
        1, std::memory_order_relaxed);
    const std::uint32_t transfer_slot =
        context->aot_transfer_trace_count % kWin32AotTransferTraceCapacity;
    context->aot_transfer_trace[transfer_slot] = {source, target, is_call};
    ++context->aot_transfer_trace_count;
    context->aot_last_indirect_source.store(source,
                                             std::memory_order_relaxed);
    context->aot_last_indirect_target.store(target,
                                             std::memory_order_relaxed);
    context->aot_reentry_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool HandleAotReturnTransfer(EXCEPTION_POINTERS* exception_info,
                             CONTEXT* win32_context,
                             ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        !context->aot_reentry_pending ||
        exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
    {
        return false;
    }
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    if (instruction[0] != 0xC3U && instruction[0] != 0xC2U)
    {
        return false;
    }
    std::uint32_t target = 0;
    if (!ReadGuestUInt32(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Esp)),
            &target))
    {
        return false;
    }
    context->aot_last_return_target.store(target,
                                           std::memory_order_relaxed);
    context->aot_last_return_source.store(
        static_cast<std::uint32_t>(win32_context->Eip),
        std::memory_order_relaxed);
    const void* return_stack = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context, return_stack,
                             sizeof(context->aot_last_return_stack)))
    {
        std::memcpy(context->aot_last_return_stack, return_stack,
                    sizeof(context->aot_last_return_stack));
    }
    context->aot_last_return_matches_call = false;
    context->aot_last_expected_return = 0;
    if (context->aot_call_depth != 0U)
    {
        const ThreadContext::AotCallFrame& frame =
            context->aot_call_frames[context->aot_call_depth - 1U];
        context->aot_last_expected_return = frame.fallthrough;
        context->aot_last_expected_call_source = frame.source;
        context->aot_last_expected_call_target = frame.target;
        context->aot_last_return_matches_call =
            target == frame.fallthrough;
        if (context->aot_last_return_matches_call)
        {
            --context->aot_call_depth;
        }
    }
    const std::uint32_t trace_slot =
        context->aot_return_trace_count % kWin32AotReturnTraceCapacity;
    context->aot_return_trace[trace_slot] = {
        static_cast<std::uint32_t>(win32_context->Eip), target,
        context->aot_last_expected_return, win32_context->Esp,
        context->aot_last_return_matches_call};
    ++context->aot_return_trace_count;
    std::uint32_t cache_target = target;
    if (!ResolveAotTransferTarget(context, target, &cache_target))
    {
        return false;
    }
    if (IsAotInlineCacheMiss(context, context->aot_reentry_cache_address))
    {
        context->aot_inline_cache_patch_attempt_count.fetch_add(
            1, std::memory_order_relaxed);
        if (RequestAotInlineCachePatch(
                context, context->aot_reentry_cache_address,
                target, cache_target))
        {
            context->aot_inline_cache_patch_success_count.fetch_add(
                1, std::memory_order_relaxed);
        }
    }
    std::uint32_t pop_bytes = 4U;
    if (instruction[0] == 0xC2U)
    {
        pop_bytes += static_cast<std::uint32_t>(instruction[1]) |
                     (static_cast<std::uint32_t>(instruction[2]) << 8U);
    }
    win32_context->Esp += pop_bytes;
    win32_context->Eip = cache_target;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_return_dispatch_count.fetch_add(
        1, std::memory_order_relaxed);
    context->aot_reentry_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool HandleAotReentry(EXCEPTION_POINTERS* exception_info,
                      CONTEXT* win32_context,
                      ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr)
    {
        return false;
    }
    const DWORD code = exception_info->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT)
    {
        const std::uint32_t cache_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(
                exception_info->ExceptionRecord->ExceptionAddress));
        std::uint32_t guest_address = 0;
        if (!FindAotGuestAddress(*context->aot_placement, cache_address,
                                 &guest_address))
        {
            return false;
        }
        context->aot_reentry_cache_address = cache_address;
        if (IsWin32AotCacheAddressRetired(
                *context->aot_placement, cache_address))
        {
            context->aot_retired_entry_trap_count.fetch_add(
                1, std::memory_order_relaxed);
            std::uint32_t latest_cache_address = guest_address;
            if (ResolveAotTransferTarget(
                    context, guest_address, &latest_cache_address, true))
            {
                win32_context->Eip = latest_cache_address;
                win32_context->EFlags &= ~0x00000100U;
                context->aot_reentry_pending = false;
                context->aot_legacy_fallback = false;
                context->enable_single_step_trace = false;
                context->aot_reentry_count.fetch_add(
                    1, std::memory_order_relaxed);
                return true;
            }
        }
        win32_context->Eip = guest_address;
        RecordExecutionProbe(win32_context, context);
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->enable_single_step_trace = true;
        context->aot_boundary_count.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (code != EXCEPTION_SINGLE_STEP || !context->aot_reentry_pending)
    {
        return false;
    }
    const std::uint32_t current = static_cast<std::uint32_t>(win32_context->Eip);
    if (IsAotCacheAddress(context, current))
    {
        win32_context->EFlags &= ~0x00000100U;
        context->aot_reentry_pending = false;
        context->enable_single_step_trace = false;
        context->aot_reentry_count.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    if (IsWin32AotGuestPageQuarantined(
            *context->aot_placement, current))
    {
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->aot_legacy_fallback = false;
        context->enable_single_step_trace = true;
        return false;
    }
    std::uint32_t cache_address = current;
    if (ResolveAotTransferTarget(context, current, &cache_address))
    {
        win32_context->Eip = cache_address;
        win32_context->EFlags &= ~0x00000100U;
        context->aot_reentry_pending = false;
        context->aot_legacy_fallback = false;
        context->enable_single_step_trace = false;
        context->aot_reentry_count.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    if (IsWin32AotGuestPageQuarantined(
            *context->aot_placement, current))
    {
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->aot_legacy_fallback = false;
        context->enable_single_step_trace = true;
        return false;
    }
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = true;
    context->enable_single_step_trace = true;
    context->aot_legacy_fallback_count.fetch_add(
        1, std::memory_order_relaxed);
    context->aot_last_fallback_address.store(current,
                                              std::memory_order_relaxed);
    return false;
}

LONG WINAPI GuestStackVectoredExceptionHandler(
    EXCEPTION_POINTERS* exception_info)
{
    ThreadContext* context = g_active_thread_context;
    if (context == nullptr ||
        exception_info == nullptr || exception_info->ContextRecord == nullptr)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (context->use_guest_stack &&
        (context->active_call_state == nullptr ||
         context->active_call_state->host_esp == 0))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    CONTEXT* win32_context = exception_info->ContextRecord;
    const auto stop_for_aot_terminal_failure = [context, win32_context]() {
        if (!context->aot_terminal_failure.load(std::memory_order_acquire))
        {
            return false;
        }
        win32_context->EFlags &= ~0x00000100U;
        return true;
    };
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleAotGuestCodeWriteCompletion(
            exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleAotGuestCodeWriteFault(
            exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleAotReentry(exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleAotIndirectTransfer(exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleAotConditionalTransfer(exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleAotReturnTransfer(exception_info, win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (stop_for_aot_terminal_failure())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (context->native_fast_path.active)
    {
        const bool returned =
            exception_info->ExceptionRecord != nullptr &&
            exception_info->ExceptionRecord->ExceptionCode ==
                EXCEPTION_SINGLE_STEP &&
            win32_context->Eip ==
                context->native_fast_path.return_address &&
            (win32_context->Dr6 & 0x1U) != 0;
        detail::LeaveNativeFastPath(win32_context,
                                    &context->native_fast_path,
                                    returned);
    }
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_fs,
            static_cast<long>(g_recovery_host_fs));
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_ds,
            static_cast<long>(g_recovery_host_ds));
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_es,
            static_cast<long>(g_recovery_host_es));
        InterlockedExchange(
            &context->shared_live_telemetry->recovery_host_gs,
            static_cast<long>(g_recovery_host_gs));
    }
    constexpr DWORD kVisualCppThreadNameException = 0x406D1388U;
    constexpr DWORD kDebugPrintExceptionAnsi = 0x40010006U;
    constexpr DWORD kDebugPrintExceptionWide = 0x4001000AU;
    if (exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_SINGLE_STEP &&
        !IsGuestInstructionPointer(
            context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        win32_context->EFlags &= ~0x00000100U;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception_info->ExceptionRecord != nullptr &&
        (exception_info->ExceptionRecord->ExceptionCode ==
             kDebugPrintExceptionAnsi ||
         exception_info->ExceptionRecord->ExceptionCode ==
             kDebugPrintExceptionWide) &&
        !IsGuestInstructionPointer(
            context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            kVisualCppThreadNameException &&
        !IsGuestInstructionPointer(
            context, static_cast<std::uint32_t>(win32_context->Eip)))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (context->shared_live_telemetry != nullptr &&
        exception_info->ExceptionRecord != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->last_exception_code,
            static_cast<long>(
                exception_info->ExceptionRecord->ExceptionCode));
        if (IsGuestInstructionPointer(
                context,
                static_cast<std::uint32_t>(win32_context->Eip)))
        {
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_eip,
                static_cast<long>(win32_context->Eip));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_eax,
                static_cast<long>(win32_context->Eax));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_ebx,
                static_cast<long>(win32_context->Ebx));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_ecx,
                static_cast<long>(win32_context->Ecx));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_edx,
                static_cast<long>(win32_context->Edx));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_esi,
                static_cast<long>(win32_context->Esi));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_edi,
                static_cast<long>(win32_context->Edi));
            InterlockedExchange(
                &context->shared_live_telemetry->last_guest_esp,
                static_cast<long>(win32_context->Esp));
            InterlockedExchange(
                &context->shared_live_telemetry->guest_handler_phase,
                1);
        }
    }
    if (context->enable_single_step_trace)
    {
        win32_context->EFlags |= 0x00000100U;
    }
    ExceptionDispatchScope dispatch_scope(
        context,
        static_cast<std::uint32_t>(win32_context->Eip));
    RecordAllocatorControlFlowException(exception_info, context);
    if (HandleGlideGateBoundary(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (HandleLinexeFarTransferBoundary(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception_info->ExceptionRecord != nullptr &&
        (exception_info->ExceptionRecord->ExceptionCode ==
             EXCEPTION_SINGLE_STEP ||
         (context->aot_reentry_pending &&
          exception_info->ExceptionRecord->ExceptionCode ==
             EXCEPTION_BREAKPOINT)) &&
        HandleSingleStepTrace(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_privileged_trap_hle &&
        HandlePrivilegedTrapInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_privileged_trap_hle &&
        HandlePortIoInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_traced_dos_hle &&
        (HandleTracedDosInterrupt21(win32_context, context) ||
         HandleTracedDosInterrupt2F(win32_context, context) ||
         HandleTracedDpmiInterrupt31(win32_context, context) ||
         HandleTracedMouseInterrupt33(win32_context, context)))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentPopInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleRepStosdInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentStoreInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentOverrideMemoryLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentOverrideByteLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleFsSegmentWordLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentMemoryCompareInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleSegmentMemoryLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryLoadInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryAddInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryOrInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryCompareByteInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryStoreInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedMemoryTestInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleTracedFpuMemoryInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_dos_hle &&
        HandleDosHleInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_dos_hle &&
        exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_ACCESS_VIOLATION &&
        HandleDosMemoryAccess(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleRepMovsInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleRepCmpsbInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->enable_segment_load_hle &&
        HandleLodsbInstruction(win32_context, context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (context->aot_terminal_failure.load(std::memory_order_acquire))
    {
        win32_context->EFlags &= ~0x00000100U;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (HandleOriginalFatalBreakpoint(exception_info,
                                      win32_context,
                                      context))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (context->aot_reentry_pending &&
        exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode ==
            EXCEPTION_BREAKPOINT)
    {
        // Indirect transfers, returns, and LOOP-family instructions execute
        // once from the original image under TF, then re-enter the cache.
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (!context->use_guest_stack &&
        (*instruction == 0xCC || instruction[-1] == 0xCC))
    {
        const std::uint32_t byte_count = exception_info->ContextRecord->Ecx;
        const void* source = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(exception_info->ContextRecord->Edx));
        AppendConsoleOutput(context, source, byte_count);
        RecoverFromHleExit(exception_info->ContextRecord, context);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    const std::uint32_t exception_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    if (context->aot_placement != nullptr &&
        IsAotCacheAddress(context, exception_address) &&
        FindAotGuestAddress(*context->aot_placement,
                            exception_address,
                            &context->aot_exception_guest_address))
    {
        context->aot_exception_mapping_valid = true;
        context->aot_exception_cache_address = exception_address;
        const std::uint32_t cache_offset = exception_address -
            context->aot_placement->base_address;
        const std::uint32_t cache_bytes = std::min<std::uint32_t>(
            sizeof(context->aot_exception_cache_bytes),
            context->aot_placement->size - cache_offset);
        std::memcpy(
            context->aot_exception_cache_bytes,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(exception_address)),
            cache_bytes);
        const std::uint64_t guest_end =
            static_cast<std::uint64_t>(
                context->aot_exception_guest_address) +
            sizeof(context->aot_exception_guest_bytes);
        const std::uint64_t runtime_end =
            static_cast<std::uint64_t>(context->runtime_base) +
            context->runtime_size;
        if (context->aot_exception_guest_address >=
                context->runtime_base &&
            guest_end <= runtime_end)
        {
            std::memcpy(
                context->aot_exception_guest_bytes,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                    context->aot_exception_guest_address)),
                sizeof(context->aot_exception_guest_bytes));
        }
    }

    win32_context->EFlags &= ~0x00000100U;
    CaptureException(exception_info, context);
    context->guest_return_esp =
        static_cast<std::uint32_t>(exception_info->ContextRecord->Esp);

    if (context->use_guest_stack)
    {
        context->host_esp = context->active_call_state->host_esp;
        RecoverToHost(exception_info->ContextRecord, context);
    }
    else
    {
        exception_info->ContextRecord->Eip =
            reinterpret_cast<DWORD_PTR>(&RecoverHostStackException);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
#endif

DWORD WINAPI GuestEntryThreadProc(void* parameter)
{
    ThreadContext* context = static_cast<ThreadContext*>(parameter);
    if (context == nullptr)
    {
        return 1;
    }

    __try
    {
        if (context->use_guest_stack)
        {
#if defined(_MSC_VER) && defined(_M_IX86)
            StackSwitchCallState state;
            state.entry_address = context->entry_address;
            state.initial_esp = context->guest_initial_esp;
            state.enable_single_step_trace =
                context->enable_single_step_trace ? 1U : 0U;
            context->active_call_state = &state;
            g_active_thread_context = context;
            context->vectored_handler = AddVectoredExceptionHandler(
                1, GuestStackVectoredExceptionHandler);
            if (context->vectored_handler == nullptr)
            {
                g_active_thread_context = nullptr;
                context->active_call_state = nullptr;
                return 5;
            }

            CallGuestEntryWithStack(&state);

            context->glide_backend.Close();
            g_active_thread_context = nullptr;
            context->active_call_state = nullptr;
            context->host_esp = state.host_esp;
            if (context->guest_return_esp == 0)
            {
                context->guest_return_esp = state.guest_return_esp;
            }
            if (context->exception_caught)
            {
                return 2;
            }
            if (context->process_exit)
            {
                return 0;
            }
#else
            return 4;
#endif
        }
        else
        {
#if defined(_MSC_VER) && defined(_M_IX86)
            g_active_thread_context = context;
            context->vectored_handler = AddVectoredExceptionHandler(
                1, GuestStackVectoredExceptionHandler);
            if (context->vectored_handler == nullptr)
            {
                g_active_thread_context = nullptr;
                return 5;
            }
#endif
            using EntryFunction = void (*)();
            EntryFunction entry = reinterpret_cast<EntryFunction>(
                static_cast<std::uintptr_t>(context->entry_address));
            entry();
#if defined(_MSC_VER) && defined(_M_IX86)
            g_active_thread_context = nullptr;
            if (context->process_exit)
            {
                return 0;
            }
#endif
        }
        context->returned = true;
        return 0;
    }
    __except (CaptureException(GetExceptionInformation(), context))
    {
        return 2;
    }
}

#endif

}  // namespace

bool RunWin32ExecutionThread(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t guest_initial_esp,
    bool use_guest_stack,
    bool enable_privileged_trap_hle,
    bool enable_traced_dos_hle,
    bool enable_segment_load_hle,
    bool enable_dos_hle,
    bool enable_single_step_trace,
    const hle::DosVirtualFileSystemState* dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    Win32AotCodeCachePlacement* aot_placement,
    bool enable_dynamic_translation,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    *attempt = Win32MinimalExecutionAttempt{};
    attempt->entry_address = entry_address;
    attempt->supported = IsDirectX86ExecutionSupported();
    attempt->guest_stack_switch_supported = IsGuestStackSwitchSupported();
    attempt->guest_stack_initial_esp = guest_initial_esp;

    if (!attempt->supported)
    {
        attempt->valid = true;
#if defined(_WIN32)
        attempt->message =
            "minimal original entry execution requires a 32-bit host";
#else
        attempt->message =
            "minimal original entry execution requires Win32 host APIs";
#endif
        return true;
    }

    if (use_guest_stack && !attempt->guest_stack_switch_supported)
    {
        attempt->valid = true;
        attempt->message =
            "guest stack execution requires 32-bit MSVC Win32 support";
        return true;
    }

#if !defined(_WIN32)
    attempt->valid = true;
    attempt->message =
        "minimal original entry execution requires Win32 host APIs";
    return true;
#else
    if (!placement.valid || !placement.placed)
    {
        attempt->message = "relocated image is not placed";
        return false;
    }

    ThreadContext context;
    SharedTelemetryMapping shared_telemetry =
        OpenSharedTelemetryMapping();
    context.shared_live_telemetry = shared_telemetry.telemetry;
    if (context.shared_live_telemetry != nullptr)
    {
        InterlockedExchange(&context.shared_live_telemetry->host_phase, 1);
    }
    context.entry_address = aot_placement != nullptr
        ? aot_placement->entry_address : entry_address;
    context.runtime_base = placement.placed_base;
    context.runtime_size = placement.placed_size;
    context.guest_initial_esp = guest_initial_esp;
    context.use_guest_stack = use_guest_stack;
    context.enable_privileged_trap_hle = enable_privileged_trap_hle;
    context.enable_traced_dos_hle = enable_traced_dos_hle;
    context.enable_segment_load_hle = enable_segment_load_hle;
    context.enable_dos_hle = enable_dos_hle;
    context.enable_single_step_trace = enable_single_step_trace;
    context.aot_placement = aot_placement;
    context.aot_dynamic_translation_enabled = enable_dynamic_translation;
    char probe_offset_text[32] = {};
    const DWORD probe_offset_length = GetEnvironmentVariableA(
        "REPIU_EXECUTION_PROBE_OFFSET", probe_offset_text,
        static_cast<DWORD>(sizeof(probe_offset_text)));
    if (probe_offset_length > 0U &&
        probe_offset_length < sizeof(probe_offset_text))
    {
        char* end = nullptr;
        const unsigned long value = std::strtoul(
            probe_offset_text, &end, 0);
        if (end != probe_offset_text && *end == '\0' && value <= UINT32_MAX)
        {
            context.execution_probe_configured = true;
            context.execution_probe_offset =
                static_cast<std::uint32_t>(value);
        }
    }
    if (context.execution_probe_configured && aot_placement != nullptr &&
        !InstallWin32AotProbeSentinel(
            aot_placement,
            context.runtime_base + context.execution_probe_offset))
    {
        context.execution_probe_configured = false;
    }
    if (aot_placement != nullptr)
    {
        context.aot_cache_entry_count.store(1, std::memory_order_relaxed);
    }
    context.glide_state.texture_memory_bytes =
        repiu::hle::kPiuBansheeVirtualTextureMemoryBytes;
    if (glide_exports != nullptr)
    {
        context.glide_exports = *glide_exports;
    }
    if (cd_chd_path != nullptr && context.cd_image.Open(*cd_chd_path))
    {
        context.mscdex_available = true;
        context.cd_audio_available = context.cd_audio.Open(*cd_chd_path);
    }
    context.dos_environment_block = BuildDosEnvironmentBlock();
    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    repiu::runtime::InitializeSelectorAllocator(
        &context.dpmi_selector_allocator, 0x00A4U);
    repiu::runtime::InitializeDosLowMemory(&context.dos_low_memory);
    for (const repiu::runtime::RelocatedSelectorBinding& binding :
         placement.selector_bindings)
    {
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            repiu::runtime::GuestDescriptor{
                binding.selector,
                binding.relocated_base_address,
                binding.limit,
                0,
                true,
        });
    }
    const auto find_linexe_segment =
        [linexe_module](std::uint16_t selector)
            -> const exe::Dos16mBoundSegment* {
        if (linexe_module == nullptr)
        {
            return nullptr;
        }
        for (const exe::Dos16mBoundSegment& segment :
             linexe_module->segments)
        {
            if (segment.selector == selector)
            {
                return &segment;
            }
        }
        return nullptr;
    };
    const exe::Dos16mBoundSegment* extracted_code =
        find_linexe_segment(kDos4gwLinexeCodeSelector);
    const exe::Dos16mBoundSegment* extracted_bss =
        find_linexe_segment(0x0088U);
    const exe::Dos16mBoundSegment* extracted_data =
        find_linexe_segment(kDos4gwLinexeDataSelector);
    const bool extracted_linexe_valid = linexe_module != nullptr &&
        extracted_code != nullptr && extracted_bss != nullptr &&
        extracted_data != nullptr;
    if (repiu::hle::BuildLinexeCallGatePlan(&context.linexe_gate_plan) &&
        repiu::hle::BuildLinexeArenaLayout(
            placement.hle_reserve_base,
            placement.arena_end_address,
            extracted_linexe_valid
                ? static_cast<std::uint32_t>(extracted_code->image.size())
                : static_cast<std::uint32_t>(
                      context.linexe_gate_plan.gate_image.size()),
            extracted_linexe_valid
                ? static_cast<std::uint32_t>(extracted_bss->image.size())
                : 0U,
            extracted_linexe_valid
                ? static_cast<std::uint32_t>(extracted_data->image.size())
                : static_cast<std::uint32_t>(
                      context.linexe_gate_plan.private_data_image.size()),
            &context.linexe_arena_layout))
    {
        const auto address = [](std::uint32_t value) {
            return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
        };
        const bool glide_gate_fits =
            context.linexe_arena_layout.gate_code_size >
                kGlideFirstGateOffset &&
            repiu::hle::BuildGlideGatePlan(
                context.glide_exports,
                kGlideFirstGateOffset,
                kGlideGateStride,
                context.linexe_arena_layout.gate_code_size -
                    kGlideFirstGateOffset,
                &context.glide_gate_plan);
        const bool images_written =
            WriteGuestBytes(&context,
                            address(context.linexe_arena_layout.client_data_base),
                            context.linexe_gate_plan.client_data_image.data(),
                            context.linexe_gate_plan.client_data_image.size()) &&
            WriteGuestBytes(&context,
                            address(context.linexe_arena_layout.gate_code_base),
                            extracted_linexe_valid
                                ? extracted_code->image.data()
                                : context.linexe_gate_plan.gate_image.data(),
                            extracted_linexe_valid
                                ? extracted_code->image.size()
                                : context.linexe_gate_plan.gate_image.size()) &&
            glide_gate_fits &&
            WriteGuestBytes(
                &context,
                address(context.linexe_arena_layout.gate_code_base +
                        kGlideFirstGateOffset),
                context.glide_gate_plan.image.data(),
                context.glide_gate_plan.image.size()) &&
            (!extracted_linexe_valid || WriteGuestBytes(
                &context,
                address(context.linexe_arena_layout.bss_base),
                extracted_bss->image.data(),
                extracted_bss->image.size())) &&
            WriteGuestBytes(&context,
                            address(context.linexe_arena_layout.private_data_base),
                            extracted_linexe_valid
                                ? extracted_data->image.data()
                                : context.linexe_gate_plan.private_data_image.data(),
                            extracted_linexe_valid
                                ? extracted_data->image.size()
                                : context.linexe_gate_plan.private_data_image.size());
        const bool descriptors_registered = images_written &&
            repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {kDos4gwClientDataSelector,
                 context.linexe_arena_layout.client_data_base,
                 0x0FFFU, 0, true}) &&
            repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {kDos4gwLinexeDataSelector,
                 context.linexe_arena_layout.private_data_base,
                 extracted_linexe_valid ? extracted_data->limit : 0x0FFFU,
                 0, true}) &&
            repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {kDos4gwLinexeCodeSelector,
                 context.linexe_arena_layout.gate_code_base,
                 context.linexe_arena_layout.gate_code_size - 1U,
                 0, true}) &&
            (!extracted_linexe_valid || repiu::runtime::RegisterDescriptor(
                &context.selector_table,
                {0x0088U,
                 context.linexe_arena_layout.bss_base,
                 extracted_bss->limit,
                 0, true}));
        if (descriptors_registered)
        {
            DWORD ignored = 0;
            const bool client_protected = VirtualProtect(
                address(context.linexe_arena_layout.client_data_base),
                0x1000U, PAGE_READONLY, &ignored) != 0;
            const bool private_protected = VirtualProtect(
                address(context.linexe_arena_layout.private_data_base),
                context.linexe_arena_layout.private_data_size,
                extracted_linexe_valid ? PAGE_READWRITE : PAGE_READONLY,
                &ignored) != 0;
            const bool gates_protected = VirtualProtect(
                address(context.linexe_arena_layout.gate_code_base),
                context.linexe_arena_layout.gate_code_size,
                extracted_linexe_valid ? PAGE_READWRITE : PAGE_EXECUTE_READ,
                &ignored) != 0;
            const bool bss_protected = !extracted_linexe_valid ||
                VirtualProtect(address(context.linexe_arena_layout.bss_base),
                               context.linexe_arena_layout.bss_size,
                               PAGE_READWRITE, &ignored) != 0;
            context.linexe_environment_active =
                client_protected && private_protected && gates_protected &&
                bss_protected;
        }
    }
    if (context.linexe_environment_active)
    {
        context.aot_excluded_guest_ranges.push_back({
            context.linexe_arena_layout.gate_code_base,
            context.linexe_arena_layout.gate_code_size});
    }
    if (dos_file_system != nullptr)
    {
        context.dos_file_system = *dos_file_system;
    }

    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.create_thread == nullptr ||
        api.close_handle == nullptr ||
        api.get_last_error == nullptr)
    {
        attempt->message = "failed to resolve required Win32 thread APIs";
        return false;
    }

    const auto stop_translation_worker = [&context]() {
        if (context.aot_translation_thread != nullptr)
        {
            context.aot_translation_shutdown.store(
                true, std::memory_order_release);
            if (context.aot_translation_request_event == nullptr ||
                SetEvent(context.aot_translation_request_event) == 0 ||
                WaitForSingleObject(context.aot_translation_thread,
                                    INFINITE) != WAIT_OBJECT_0)
            {
                // Context ownership cannot be released while the worker could
                // still reference it. Treat an impossible join failure as a
                // process-local terminal failure rather than creating UAF.
                std::abort();
            }
            CloseHandle(context.aot_translation_thread);
            context.aot_translation_thread = nullptr;
        }
        if (context.aot_translation_request_event != nullptr)
        {
            CloseHandle(context.aot_translation_request_event);
            context.aot_translation_request_event = nullptr;
        }
        if (context.aot_translation_complete_event != nullptr)
        {
            CloseHandle(context.aot_translation_complete_event);
            context.aot_translation_complete_event = nullptr;
        }
    };
    if (context.aot_placement != nullptr)
    {
        context.aot_translation_request_event =
            CreateEventA(nullptr, FALSE, FALSE, nullptr);
        context.aot_translation_complete_event =
            CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (context.aot_translation_request_event == nullptr ||
            context.aot_translation_complete_event == nullptr)
        {
            stop_translation_worker();
            attempt->message = "failed to create AOT translation events";
            return false;
        }
        context.aot_translation_thread = api.create_thread(
            nullptr, 0, AotTranslationWorkerProc, &context, 0, nullptr);
        if (context.aot_translation_thread == nullptr)
        {
            stop_translation_worker();
            attempt->message = "failed to create AOT translation worker";
            return false;
        }
        if (!InstallWin32AotGuestPageWriteWatches(
                *context.aot_placement, nullptr,
                &context.aot_page_write_watch))
        {
            RestoreWin32AotGuestPageWriteWatches(
                &context.aot_page_write_watch);
            stop_translation_worker();
            attempt->message =
                "failed to install AOT guest code write watches";
            return false;
        }
    }

    HANDLE thread = api.create_thread(nullptr,
                                      0,
                                      GuestEntryThreadProc,
                                      &context,
                                      0,
                                      nullptr);
    if (thread == nullptr)
    {
        const DWORD error = api.get_last_error();
        std::ostringstream stream;
        stream << "CreateThread failed with error " << error;
        attempt->message = stream.str();
        RestoreWin32AotGuestPageWriteWatches(&context.aot_page_write_watch);
        stop_translation_worker();
        return false;
    }

    attempt->attempted = true;
    if (context.shared_live_telemetry != nullptr)
    {
        InterlockedExchange(&context.shared_live_telemetry->host_phase, 2);
    }
    attempt->guest_stack_switch_attempted = use_guest_stack;
    DWORD exit_code = 0;
    const DWORD wait_result = PollThreadUntilExit(
        thread,
        timeout_milliseconds,
        (enable_single_step_trace || aot_placement != nullptr)
            ? &context : nullptr,
        &exit_code);

    const auto remove_vectored_handler = [&context]() {
        if (context.vectored_handler != nullptr)
        {
            RemoveVectoredExceptionHandler(context.vectored_handler);
            context.vectored_handler = nullptr;
        }
    };

    if (wait_result == WAIT_TIMEOUT)
    {
        attempt->timed_out = true;
        attempt->thread_exit_code = 3;
        if (api.terminate_thread != nullptr)
        {
            api.terminate_thread(thread, 3);
            WaitForSingleObject(thread, 5000U);
        }
        remove_vectored_handler();
        stop_translation_worker();
        RestoreWin32AotGuestPageWriteWatches(&context.aot_page_write_watch);
        CopyThreadObservationToAttempt(context, attempt);
        attempt->valid = true;
        attempt->message = "minimal execution attempt timed out";
        api.close_handle(thread);
        return true;
    }

    remove_vectored_handler();
    stop_translation_worker();
    RestoreWin32AotGuestPageWriteWatches(&context.aot_page_write_watch);
    api.close_handle(thread);

    attempt->returned = context.returned;
    attempt->exception_caught = context.exception_caught;
    attempt->guest_stack_return_esp = context.guest_return_esp;
    attempt->seh_exception_code = context.exception_code;
    attempt->seh_exception_address = context.exception_address;
    attempt->exception_eax = context.exception_eax;
    attempt->exception_ebx = context.exception_ebx;
    attempt->exception_ecx = context.exception_ecx;
    attempt->exception_edx = context.exception_edx;
    attempt->exception_esi = context.exception_esi;
    attempt->exception_edi = context.exception_edi;
    attempt->exception_snapshot = context.exception_snapshot;
    CopyThreadObservationToAttempt(context, attempt);
    attempt->thread_exit_code = exit_code;
    attempt->hle_stdout_output.assign(
        context.hle_stdout_output,
        context.hle_stdout_output + context.hle_stdout_output_size);
    attempt->hle_stderr_output.assign(
        context.hle_stderr_output,
        context.hle_stderr_output + context.hle_stderr_output_size);
    attempt->valid = true;

    if (attempt->returned)
    {
        attempt->message = context.hle_message.empty()
                               ? "original entry returned to host trampoline"
                               : context.hle_message;
    }
    else if (attempt->exception_caught)
    {
        attempt->message = context.hle_message.empty()
                               ? "original entry raised a caught exception"
                               : context.hle_message;
    }
    else
    {
        attempt->message =
            "minimal execution attempt ended without return or exception";
    }

    return true;
#endif
}

bool AttemptWin32MinimalExecution(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    return RunWin32ExecutionThread(
        placement,
        entry_address,
        0,
        false,
        false,
        false,
        false,
        false,
        false,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        false,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    if (!stack_plan.valid)
    {
        *attempt = Win32MinimalExecutionAttempt{};
        attempt->entry_address = stack_plan.entry_eip;
        attempt->guest_stack_initial_esp = stack_plan.initial_esp;
        attempt->message = "guest stack switch plan is not valid";
        return false;
    }

    return RunWin32ExecutionThread(
        placement,
        stack_plan.entry_eip,
        stack_plan.initial_esp,
        true,
        false,
        false,
        false,
        false,
        false,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        false,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackTrapExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    const exe::Dos16mBoundModule* linexe_module,
    const std::vector<exe::LeResidentName>* glide_exports,
    const std::filesystem::path* cd_chd_path,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    if (!stack_plan.valid)
    {
        *attempt = Win32MinimalExecutionAttempt{};
        attempt->entry_address = stack_plan.entry_eip;
        attempt->guest_stack_initial_esp = stack_plan.initial_esp;
        attempt->message = "guest stack switch plan is not valid";
        return false;
    }

    return RunWin32ExecutionThread(
        placement,
        stack_plan.entry_eip,
        stack_plan.initial_esp,
        true,
        true,
        true,
        true,
        false,
        true,
        &dos_file_system,
        linexe_module,
        glide_exports,
        cd_chd_path,
        nullptr,
        false,
        timeout_milliseconds,
        attempt);
}

bool AttemptWin32GuestStackHleExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    const hle::DosVirtualFileSystemState& dos_file_system,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    if (!stack_plan.valid)
    {
        *attempt = Win32MinimalExecutionAttempt{};
        attempt->entry_address = stack_plan.entry_eip;
        attempt->guest_stack_initial_esp = stack_plan.initial_esp;
        attempt->message = "guest stack switch plan is not valid";
        return false;
    }

    return RunWin32ExecutionThread(
        placement,
        stack_plan.entry_eip,
        stack_plan.initial_esp,
        true,
        true,
        true,
        true,
        true,
        false,
        &dos_file_system,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        false,
        timeout_milliseconds,
        attempt);
}

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
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr || !stack_plan.valid || !aot_placement.placed)
    {
        if (attempt != nullptr)
        {
            *attempt = Win32MinimalExecutionAttempt{};
            attempt->message = !stack_plan.valid
                ? "guest stack switch plan is not valid"
                : "AOT code cache placement is not valid";
        }
        return false;
    }
    return RunWin32ExecutionThread(
        placement, stack_plan.entry_eip, stack_plan.initial_esp,
        true, true, true, true, false, false, &dos_file_system,
        linexe_module, glide_exports, cd_chd_path, &aot_placement,
        enable_dynamic_translation,
        timeout_milliseconds, attempt);
}

}  // namespace repiu::platform::win32
