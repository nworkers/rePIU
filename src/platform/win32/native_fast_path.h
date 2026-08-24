#pragma once

#include "verified_region_analyzer.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "repiu/platform/guest_cpu_context.h"


namespace repiu::platform::win32::detail
{

constexpr std::uint32_t kNativeLinearSpanRejectCacheMaxEntries = 65536;

struct NativeLinearSpanCacheEntry
{
    std::uint32_t guest_page = 0;
    std::uint32_t generation = 0;
    NativeLinearSpan span;
};

struct NativeLinearSpanRejectCacheEntry
{
    std::array<std::uint8_t, kNativeLinearSpanRejectSnapshotCapacity>
        bytes{};
    std::uint32_t byte_count = 0;
};

struct NativeFastPathState
{
    bool active = false;
    std::uint32_t return_address = 0;
    std::uint32_t saved_dr0 = 0;
    std::uint32_t saved_dr6 = 0;
    std::uint32_t saved_dr7 = 0;
    std::atomic<std::uint32_t> entry_count{0};
    std::atomic<std::uint32_t> return_count{0};
    std::atomic<std::uint32_t> cancel_count{0};
    std::uint32_t last_entry = 0;
    std::uint32_t last_return = 0;
    std::uint32_t previous_eip = 0;
    std::unordered_map<std::uint32_t, std::int8_t> verification_cache;
    std::atomic<std::uint32_t> verified_count{0};
    std::atomic<std::uint32_t> rejected_count{0};
    std::atomic<std::uint32_t> last_rejected_instruction{0};
    std::atomic<std::uint32_t> last_rejected_opcode{0};
    std::atomic<std::uint32_t> last_rejected_candidate{0};
    std::atomic<std::uint32_t> last_rejected_bytes_low{0};
    std::atomic<std::uint32_t> last_rejected_bytes_high{0};

    // Route A region execution (Task 266). Unlike the clean-function fast path
    // above, a region may contain up to three HLE-sensitive instructions. They
    // are trapped with hardware execution breakpoints (Dr1-Dr3) rather than code
    // patches, so no guest byte is modified; Dr0 breakpoints the caller return
    // address to bound the region. On a Dr1-Dr3 fault the sensitive instruction
    // is HLE-emulated (it has not executed yet) and native execution resumes.
    static constexpr std::uint32_t kMaxRegionSensitive = 3;
    bool region_active = false;
    std::uint32_t region_return_address = 0;
    std::uint32_t region_sensitive_addr[kMaxRegionSensitive] = {0, 0, 0};
    std::uint32_t region_sensitive_slots = 0;
    std::uint32_t region_saved_dr0 = 0;
    std::uint32_t region_saved_dr1 = 0;
    std::uint32_t region_saved_dr2 = 0;
    std::uint32_t region_saved_dr3 = 0;
    std::uint32_t region_saved_dr6 = 0;
    std::uint32_t region_saved_dr7 = 0;
    std::unordered_map<std::uint32_t, std::int8_t> region_analyzable_cache;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>
        region_sensitive_cache;
    std::atomic<std::uint32_t> region_entry_count{0};
    std::atomic<std::uint32_t> region_sensitive_hit_count{0};
    std::atomic<std::uint32_t> region_reject_count{0};
    std::atomic<std::uint32_t> region_return_count{0};
    std::atomic<std::uint32_t> region_cancel_count{0};
    std::atomic<std::uint32_t> region_stray_heal_count{0};

    // Task 275 general-entry straight-line spans. Dr0 guards the first
    // sensitive/control/store boundary while TF is clear. The boundary remains
    // on the existing single-step path; no guest byte is modified.
    bool linear_span_active = false;
    std::uint32_t linear_span_boundary = 0;
    std::uint32_t linear_span_instruction_count = 0;
    std::uint32_t linear_span_saved_dr0 = 0;
    std::uint32_t linear_span_saved_dr6 = 0;
    std::uint32_t linear_span_saved_dr7 = 0;
    std::atomic<std::uint32_t> linear_span_entry_count{0};
    std::atomic<std::uint32_t> linear_span_boundary_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_tf_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr0_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr1_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr2_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr3_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_other_db_count{0};
    std::atomic<std::uint32_t> linear_span_cancel_tf_first_eip{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr0_first_eip{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr1_first_eip{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr2_first_eip{0};
    std::atomic<std::uint32_t> linear_span_cancel_dr3_first_eip{0};
    std::atomic<std::uint32_t> linear_span_cancel_other_db_first_eip{0};
    std::atomic<std::uint32_t> linear_span_instruction_total{0};
    std::atomic<std::uint32_t> linear_span_reject_count{0};
    std::unordered_map<std::uint32_t, NativeLinearSpanCacheEntry>
        linear_span_scan_cache;
    std::atomic<std::uint32_t> linear_span_cache_hit_count{0};
    std::atomic<std::uint32_t> linear_span_cache_miss_count{0};
    std::unordered_map<std::uint32_t, NativeLinearSpanRejectCacheEntry>
        linear_span_reject_cache;
    std::atomic<std::uint32_t> linear_span_reject_cache_hit_count{0};
    std::atomic<std::uint32_t> linear_span_reject_cache_miss_count{0};
    std::atomic<std::uint32_t> linear_span_reject_cache_stale_count{0};
    std::atomic<std::uint32_t> linear_span_reject_cache_store_count{0};
    std::atomic<std::uint32_t>
        linear_span_reject_cache_capacity_skip_count{0};
    std::atomic<std::uint32_t> linear_span_write_cross_count{0};
    std::atomic<std::uint32_t> linear_span_write_guard_uncovered_count{0};
    std::atomic<std::uint32_t> linear_span_write_fault_cancel_count{0};
    std::atomic<std::uint32_t> linear_span_last_cancel_code{0};
    std::atomic<std::uint32_t> linear_span_last_cancel_eip{0};
    std::unordered_map<std::uint32_t, bool>
        linear_span_write_target_page_cache;
    std::atomic<std::uint32_t> linear_span_direct_jump_chain_count{0};
    std::atomic<std::uint32_t> linear_span_backward_jump_stop_count{0};
};

bool TryEnterNativeFastPath(repiu::platform::GuestCpuContext* context,
                            NativeFastPathState* state,
                            std::uint32_t runtime_base,
                            std::uint32_t runtime_size);
void LeaveNativeFastPath(repiu::platform::GuestCpuContext* context,
                         NativeFastPathState* state,
                         bool returned);

}  // namespace repiu::platform::win32::detail
