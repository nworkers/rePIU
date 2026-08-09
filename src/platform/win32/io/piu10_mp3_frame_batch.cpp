#include "piu10_mp3_frame_batch.h"

#include "cpu_emul/guest_memory_access.h"
#include "execution/thread_context.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint32_t kPumpitoMp3OutOffset = 0x000212FDU;
constexpr std::uint32_t kSourceCursorOffset = 0x00343420U;
constexpr std::uint32_t kAvailableEndOffset = 0x00343424U;
constexpr std::uint32_t kFrameByteTargetOffset = 0x00343418U;
constexpr std::uint32_t kFrameByteCountOffset = 0x0034341CU;
constexpr std::uint32_t kSourceBufferOffset = 0x00343438U;
constexpr std::uint32_t kMaximumMpegFrameBytes = 2048U;
constexpr std::uint32_t kTransferControlCursor = 0x0000076CU;
constexpr std::uint32_t kTransferControlCount = 100U;
constexpr std::size_t kCodePrefixBytes = 20U;
constexpr std::size_t kCodeSuffixBytes = 21U;

enum class BatchRejection : std::uint32_t
{
    kDisabled = 1U << 0U,
    kGuestSource = 1U << 1U,
    kCodeRange = 1U << 2U,
    kSignature = 1U << 3U,
    kRelocation = 1U << 4U,
    kStateRange = 1U << 5U,
    kFrameState = 1U << 6U,
    kSourceAddress = 1U << 7U,
    kSourceRange = 1U << 8U,
    kCommit = 1U << 9U,
};

void ReportRejection(ThreadContext* context, BatchRejection rejection,
                     const char* reason, std::uint32_t guest_source,
                     std::uint32_t value0 = 0U,
                     std::uint32_t value1 = 0U,
                     std::uint32_t value2 = 0U,
                     std::uint32_t value3 = 0U)
{
    if (context == nullptr)
    {
        return;
    }
    const std::uint32_t bit = static_cast<std::uint32_t>(rejection);
    if ((context->piu10_mp3_frame_batch_rejection_mask & bit) != 0U)
    {
        return;
    }
    context->piu10_mp3_frame_batch_rejection_mask |= bit;
    std::fprintf(
        stderr,
        "[repiu-piu10-mp3] frame-tail batch rejected: %s "
        "eip=0x%08X values=0x%08X/0x%08X/0x%08X/0x%08X\n",
        reason, guest_source, value0, value1, value2, value3);
}

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    std::uint32_t value = 0U;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool AddAddress(std::uint32_t base, std::uint32_t offset,
                std::uint32_t* result)
{
    if (result == nullptr ||
        offset > std::numeric_limits<std::uint32_t>::max() - base)
    {
        return false;
    }
    *result = base + offset;
    return true;
}

void LimitPlanToTransferControlBoundary(
    Piu10Mp3FrameBatchPlan* plan, std::uint32_t guest_ecx)
{
    if (plan == nullptr || plan->source_cursor == nullptr)
    {
        return;
    }
    const std::uint32_t cursor = *plan->source_cursor;
    const std::uint32_t cursor_distance =
        cursor < kTransferControlCursor
        ? kTransferControlCursor - cursor : 0U;
    const std::uint32_t count_distance =
        guest_ecx < kTransferControlCount - 1U
        ? kTransferControlCount - 1U - guest_ecx : 0U;
    const std::size_t boundary_distance =
        std::max(cursor_distance, count_distance);
    plan->bytes = plan->bytes.first(
        std::min(plan->bytes.size(), boundary_distance));
}

bool AuditPiu10Mp3FrameTail(
    ThreadContext* context, std::uint32_t guest_source,
    std::uint8_t current_byte, std::uint32_t* guest_ecx)
{
    if (context == nullptr || !context->piu10_mp3_frame_batch_audit_enabled)
    {
        return false;
    }
    if (guest_ecx == nullptr)
    {
        return true;
    }
    if (context->piu10_mp3_frame_batch_audit_mismatches != 0U)
    {
        return true;
    }

    if (context->piu10_mp3_frame_batch_audit_active)
    {
        std::uint32_t cursor_address = 0U;
        std::uint32_t count_address = 0U;
        if (!AddAddress(context->piu10_mp3_data_object_base,
                        kSourceCursorOffset, &cursor_address) ||
            !AddAddress(context->piu10_mp3_data_object_base,
                        kFrameByteCountOffset, &count_address))
        {
            return true;
        }
        const auto* cursor = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(cursor_address));
        const auto* count = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(count_address));
        const std::uint32_t index =
            context->piu10_mp3_frame_batch_audit_index;
        const std::uint32_t expected_cursor =
            context->piu10_mp3_frame_batch_audit_cursor + index + 1U;
        const std::uint32_t expected_count =
            context->piu10_mp3_frame_batch_audit_count + index + 1U;
        const std::uint32_t expected_ecx =
            context->piu10_mp3_frame_batch_audit_ecx + index + 1U;
        const std::uint8_t expected_byte =
            context->piu10_mp3_frame_batch_audit_bytes[index];
        const bool state_readable =
            IsGuestRangeReadable(context, cursor, sizeof(*cursor)) &&
            IsGuestRangeReadable(context, count, sizeof(*count));
        const bool matches = state_readable && current_byte == expected_byte &&
            *cursor == expected_cursor && *count == expected_count &&
            *guest_ecx == expected_ecx;
        if (!matches)
        {
            ++context->piu10_mp3_frame_batch_audit_mismatches;
            std::fprintf(
                stderr,
                "[repiu-piu10-mp3] frame-tail audit mismatch "
                "frame=%llu offset=%u byte=%02X/%02X "
                "cursor=%u/%u count=%u/%u ecx=%u/%u readable=%u\n",
                static_cast<unsigned long long>(
                    context->piu10_mp3_frame_batch_audit_passed_frames + 1U),
                index, expected_byte, current_byte, expected_cursor,
                state_readable ? *cursor : 0U, expected_count,
                state_readable ? *count : 0U, expected_ecx, *guest_ecx,
                state_readable ? 1U : 0U);
            context->piu10_mp3_frame_batch_audit_active = false;
            return true;
        }

        context->piu10_mp3_frame_batch_audit_index = index + 1U;
        if (context->piu10_mp3_frame_batch_audit_index ==
            context->piu10_mp3_frame_batch_audit_size)
        {
            context->piu10_mp3_frame_batch_audit_active = false;
            const std::uint64_t passed =
                ++context->piu10_mp3_frame_batch_audit_passed_frames;
            if (passed == 1U || (passed % 100U) == 0U)
            {
                std::fprintf(
                    stderr,
                    "[repiu-piu10-mp3] frame-tail audit passed "
                    "segments=%llu\n",
                    static_cast<unsigned long long>(passed));
            }
        }
    }

    if (!context->piu10_mp3_frame_batch_audit_active)
    {
        Piu10Mp3FrameBatchPlan plan;
        if (BuildPiu10Mp3FrameBatchPlan(
                context, guest_source,
                context->piu10_mp3_frame_batch_audit_bytes.size(), &plan))
        {
            LimitPlanToTransferControlBoundary(&plan, *guest_ecx);
            if (plan.bytes.empty())
            {
                return true;
            }
            std::copy(plan.bytes.begin(), plan.bytes.end(),
                      context->piu10_mp3_frame_batch_audit_bytes.begin());
            context->piu10_mp3_frame_batch_audit_size =
                static_cast<std::uint32_t>(plan.bytes.size());
            context->piu10_mp3_frame_batch_audit_index = 0U;
            context->piu10_mp3_frame_batch_audit_cursor =
                *plan.source_cursor;
            context->piu10_mp3_frame_batch_audit_count =
                *plan.frame_byte_count;
            context->piu10_mp3_frame_batch_audit_ecx = *guest_ecx;
            context->piu10_mp3_frame_batch_audit_active = true;
        }
    }
    return true;
}

}  // namespace

bool BuildPiu10Mp3FrameBatchPlan(
    ThreadContext* context, std::uint32_t guest_source,
    std::size_t maximum_bytes, Piu10Mp3FrameBatchPlan* plan)
{
    if (context == nullptr || plan == nullptr || maximum_bytes == 0U)
    {
        return false;
    }
    if (!context->piu10_mp3_frame_batch_enabled)
    {
        ReportRejection(context, BatchRejection::kDisabled, "disabled",
                        guest_source);
        return false;
    }
    *plan = Piu10Mp3FrameBatchPlan{};

    std::uint32_t expected_source = 0U;
    if (!AddAddress(context->runtime_base, kPumpitoMp3OutOffset,
                    &expected_source) ||
        guest_source != expected_source || guest_source < kCodePrefixBytes)
    {
        ReportRejection(context, BatchRejection::kGuestSource,
                        "guest-source", guest_source, expected_source,
                        context->runtime_base, kPumpitoMp3OutOffset);
        return false;
    }

    const auto* code = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(guest_source - kCodePrefixBytes));
    if (!IsGuestRangeReadable(
            context, code,
            static_cast<std::uint32_t>(kCodePrefixBytes + kCodeSuffixBytes)))
    {
        ReportRejection(context, BatchRejection::kCodeRange, "code-range",
                        guest_source,
                        static_cast<std::uint32_t>(
                            reinterpret_cast<std::uintptr_t>(code)),
                        context->runtime_base, context->runtime_size);
        return false;
    }
    const std::uint8_t* out = code + kCodePrefixBytes;
    const bool signature_valid =
        out[-20] == 0x89U && out[-19] == 0x15U &&
        out[-14] == 0x8AU && out[-13] == 0x80U &&
        out[-8] == 0x89U && out[-7] == 0xF2U &&
        out[-6] == 0x89U && out[-5] == 0x2DU && out[0] == 0xEEU &&
        out[1] == 0xA1U && out[6] == 0x8BU && out[7] == 0x15U &&
        out[12] == 0x41U && out[13] == 0x39U && out[14] == 0xD0U &&
        out[15] == 0x0FU && out[16] == 0x85U &&
        out[17] == 0xDAU && out[18] == 0xFEU &&
        out[19] == 0xFFU && out[20] == 0xFFU;
    if (!signature_valid)
    {
        ReportRejection(context, BatchRejection::kSignature, "signature",
                        guest_source, ReadU32(out - 20), ReadU32(out - 4),
                        ReadU32(out), ReadU32(out + 12));
        return false;
    }

    std::uint32_t source_cursor_address = 0U;
    std::uint32_t available_end_address = 0U;
    std::uint32_t frame_target_address = 0U;
    std::uint32_t frame_count_address = 0U;
    std::uint32_t source_buffer_address = 0U;
    if (!AddAddress(context->piu10_mp3_data_object_base,
                    kSourceCursorOffset,
                    &source_cursor_address) ||
        !AddAddress(context->piu10_mp3_data_object_base,
                    kAvailableEndOffset,
                    &available_end_address) ||
        !AddAddress(context->piu10_mp3_data_object_base,
                    kFrameByteTargetOffset,
                    &frame_target_address) ||
        !AddAddress(context->piu10_mp3_data_object_base,
                    kFrameByteCountOffset,
                    &frame_count_address) ||
        !AddAddress(context->piu10_mp3_data_object_base,
                    kSourceBufferOffset,
                    &source_buffer_address) ||
        ReadU32(out - 18) != source_cursor_address ||
        ReadU32(out - 12) != source_buffer_address ||
        ReadU32(out - 4) != frame_count_address ||
        ReadU32(out + 2) != frame_count_address ||
        ReadU32(out + 8) != frame_target_address)
    {
        ReportRejection(context, BatchRejection::kRelocation, "relocation",
                        guest_source, ReadU32(out - 18),
                        source_cursor_address, ReadU32(out - 12),
                        source_buffer_address);
        return false;
    }

    auto* source_cursor = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uintptr_t>(source_cursor_address));
    auto* frame_count = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uintptr_t>(frame_count_address));
    const auto* available_end = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(available_end_address));
    const auto* frame_target = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(frame_target_address));
    if (!IsGuestRangeWritable(context, source_cursor, sizeof(*source_cursor)) ||
        !IsGuestRangeWritable(context, frame_count, sizeof(*frame_count)) ||
        !IsGuestRangeReadable(context, available_end, sizeof(*available_end)) ||
        !IsGuestRangeReadable(context, frame_target, sizeof(*frame_target)))
    {
        ReportRejection(context, BatchRejection::kStateRange, "state-range",
                        guest_source, source_cursor_address,
                        available_end_address, frame_target_address,
                        frame_count_address);
        return false;
    }

    const std::uint32_t cursor = *source_cursor;
    const std::uint32_t end = *available_end;
    const std::uint32_t count = *frame_count;
    const std::uint32_t target = *frame_target;
    if (cursor > end || count > target || target > kMaximumMpegFrameBytes)
    {
        ReportRejection(context, BatchRejection::kFrameState, "frame-state",
                        guest_source, cursor, end, count, target);
        return false;
    }
    if (cursor == end || count == target)
    {
        return false;
    }
    const std::size_t batch_bytes = std::min({
        maximum_bytes, static_cast<std::size_t>(end - cursor),
        static_cast<std::size_t>(target - count)});
    if (batch_bytes == 0U ||
        cursor > std::numeric_limits<std::uint32_t>::max() -
            source_buffer_address)
    {
        ReportRejection(context, BatchRejection::kSourceAddress,
                        "source-address", guest_source, cursor,
                        source_buffer_address,
                        static_cast<std::uint32_t>(batch_bytes),
                        static_cast<std::uint32_t>(maximum_bytes));
        return false;
    }
    const auto* source = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(source_buffer_address + cursor));
    if (!IsGuestRangeReadable(
            context, source, static_cast<std::uint32_t>(batch_bytes)))
    {
        ReportRejection(context, BatchRejection::kSourceRange, "source-range",
                        guest_source,
                        static_cast<std::uint32_t>(
                            reinterpret_cast<std::uintptr_t>(source)),
                        static_cast<std::uint32_t>(batch_bytes),
                        context->runtime_base, context->runtime_size);
        return false;
    }

    plan->bytes = std::span<const std::uint8_t>(source, batch_bytes);
    plan->source_cursor = source_cursor;
    plan->frame_byte_count = frame_count;
    return true;
}

bool CommitPiu10Mp3FrameBatch(
    const Piu10Mp3FrameBatchPlan& plan, std::size_t accepted_bytes,
    std::uint32_t* guest_ecx)
{
    if (accepted_bytes == 0U || accepted_bytes > plan.bytes.size() ||
        plan.source_cursor == nullptr || plan.frame_byte_count == nullptr ||
        guest_ecx == nullptr ||
        accepted_bytes > std::numeric_limits<std::uint32_t>::max() -
            *plan.source_cursor ||
        accepted_bytes > std::numeric_limits<std::uint32_t>::max() -
            *plan.frame_byte_count ||
        accepted_bytes > std::numeric_limits<std::uint32_t>::max() -
            *guest_ecx)
    {
        return false;
    }
    const std::uint32_t count = static_cast<std::uint32_t>(accepted_bytes);
    *plan.source_cursor += count;
    *plan.frame_byte_count += count;
    *guest_ecx += count;
    return true;
}

std::size_t TransferPiu10Mp3FrameTail(
    ThreadContext* context, std::uint32_t guest_source,
    std::uint8_t current_byte, std::uint32_t* guest_ecx)
{
    if (AuditPiu10Mp3FrameTail(
            context, guest_source, current_byte, guest_ecx))
    {
        return 0U;
    }
    Piu10Mp3FrameBatchPlan plan;
    if (guest_ecx == nullptr ||
        !BuildPiu10Mp3FrameBatchPlan(
            context, guest_source, kMaximumMpegFrameBytes, &plan))
    {
        return 0U;
    }
    LimitPlanToTransferControlBoundary(&plan, *guest_ecx);
    if (plan.bytes.empty() || plan.bytes.size() >
        std::numeric_limits<std::uint32_t>::max() - *guest_ecx)
    {
        return 0U;
    }
    const std::size_t accepted =
        context->piu10_mp3_audio.WriteBytes(plan.bytes);
    if (accepted == 0U)
    {
        return 0U;
    }
    if (!CommitPiu10Mp3FrameBatch(plan, accepted, guest_ecx))
    {
        ReportRejection(context, BatchRejection::kCommit, "commit",
            guest_source,
            static_cast<std::uint32_t>(plan.bytes.size()),
            static_cast<std::uint32_t>(accepted), *guest_ecx,
            static_cast<std::uint32_t>(
                context->piu10_mp3_audio.stats().ring_bytes));
        return 0U;
    }
    const std::uint64_t previous =
        context->piu10_mp3_frame_batch_byte_count.fetch_add(
            accepted, std::memory_order_relaxed);
    if (previous == 0U)
    {
        std::fprintf(stderr,
                     "[repiu-piu10-mp3] verified frame-tail batch active\n");
    }
    constexpr std::uint64_t kDecodeCheckpointBytes = 65536U;
    if (previous < kDecodeCheckpointBytes &&
        previous + accepted >= kDecodeCheckpointBytes)
    {
        const Piu10Mp3AudioStats stats = context->piu10_mp3_audio.stats();
        std::fprintf(
            stderr,
            "[repiu-piu10-mp3] batch checkpoint "
            "received/decoded/batched=%llu/%llu/%llu\n",
            static_cast<unsigned long long>(stats.received_bytes),
            static_cast<unsigned long long>(stats.decoded_frames),
            static_cast<unsigned long long>(stats.batched_bytes));
    }
    return accepted;
}

}  // namespace repiu::platform::win32
