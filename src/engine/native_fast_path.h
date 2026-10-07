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


namespace repiu::engine::detail
{

constexpr std::uint32_t kNativeLinearSpanRejectCacheMaxEntries = 65536;

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
    std::unordered_map<std::uint32_t, NativeLinearSpanRejectCacheEntry>
        linear_span_reject_cache;
    std::atomic<std::uint32_t> linear_span_reject_cache_hit_count{0};
    std::atomic<std::uint32_t> linear_span_reject_cache_miss_count{0};
    std::atomic<std::uint32_t> linear_span_reject_cache_stale_count{0};
    std::atomic<std::uint32_t> linear_span_reject_cache_store_count{0};
    std::atomic<std::uint32_t>
        linear_span_reject_cache_capacity_skip_count{0};
    // Task 288 added this for spans that crossed memory writes. Task i022
    // deleted that crossing, but a span still ends here when an implicit stack
    // write such as `push` faults on a watched page.
    std::atomic<std::uint32_t> linear_span_write_fault_cancel_count{0};
    std::atomic<std::uint32_t> linear_span_last_cancel_code{0};
    std::atomic<std::uint32_t> linear_span_last_cancel_eip{0};
};

bool TryEnterNativeFastPath(repiu::platform::GuestCpuContext* context,
                            NativeFastPathState* state,
                            std::uint32_t runtime_base,
                            std::uint32_t runtime_size);
void LeaveNativeFastPath(repiu::platform::GuestCpuContext* context,
                         NativeFastPathState* state,
                         bool returned);

}  // namespace repiu::engine::detail
