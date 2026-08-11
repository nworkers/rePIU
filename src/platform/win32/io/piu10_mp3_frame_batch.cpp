#include "piu10_mp3_frame_batch.h"

#include "cpu_emul/guest_memory_access.h"
#include "execution/thread_context.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint32_t kMaximumMpegFrameBytes = 2048U;
constexpr std::uint32_t kMaximumLoopBackBytes = 512U;
constexpr std::size_t kCodePrefixBytes = 35U;
constexpr std::size_t kCodeSuffixBytes = 21U;
constexpr std::size_t kWrappedPrefixBytes = 31U;
constexpr std::size_t kWrappedSuffixBytes = 17U;
constexpr std::size_t kOutputWrapperBytes = 10U;

struct FeederLayout
{
    std::uint32_t source_cursor_address = 0U;
    std::uint32_t available_end_address = 0U;
    std::uint32_t frame_target_address = 0U;
    std::uint32_t frame_count_address = 0U;
    std::uint32_t source_buffer_address = 0U;
    std::uint32_t service_cursor_threshold = 0U;
    std::uint32_t service_counter_limit = 0U;
};

struct AbsoluteStore
{
    std::uint32_t address = 0U;
    std::uint8_t source_register = 0U;
    std::size_t position = 0U;
};

enum class BatchRejection : std::uint32_t
{
    kDisabled = 1U << 0U,
    kShape = 1U << 1U,
    kStateRange = 1U << 2U,
    kFrameState = 1U << 3U,
    kSourceAddress = 1U << 4U,
    kSourceRange = 1U << 5U,
    kCommit = 1U << 6U,
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

bool IsSafeTemporaryRegister(std::uint8_t register_id)
{
    constexpr std::uint8_t kEax = 0U;
    constexpr std::uint8_t kEcx = 1U;
    constexpr std::uint8_t kEsp = 4U;
    constexpr std::uint8_t kEsi = 6U;
    return register_id != kEax && register_id != kEcx &&
        register_id != kEsp && register_id != kEsi;
}

bool DecodeFeederPrefix(const std::uint8_t* begin,
                        const std::uint8_t* end,
                        FeederLayout* candidate)
{
    if (begin == nullptr || end == nullptr || candidate == nullptr ||
        end - begin != static_cast<std::ptrdiff_t>(kCodePrefixBytes) ||
        begin[0] != 0xA1U)
    {
        return false;
    }

    candidate->source_cursor_address = ReadU32(begin + 1U);
    std::array<AbsoluteStore, 2> stores = {};
    std::size_t store_count = 0U;
    std::size_t position = 5U;
    std::size_t cursor_increment_position = SIZE_MAX;
    std::size_t cursor_store_position = SIZE_MAX;
    std::size_t frame_load_position = SIZE_MAX;
    std::size_t frame_increment_position = SIZE_MAX;
    std::size_t frame_store_position = SIZE_MAX;
    std::size_t byte_load_position = SIZE_MAX;
    std::size_t port_restore_position = SIZE_MAX;
    std::uint8_t cursor_register = 0U;
    std::uint8_t frame_register = 0U;
    std::uint8_t increment_register = 0U;

    while (position < kCodePrefixBytes)
    {
        const std::size_t remaining = kCodePrefixBytes - position;
        const std::uint8_t* instruction = begin + position;
        if (remaining >= 3U && instruction[0] == 0x8DU &&
            (instruction[1] & 0xC7U) == 0x40U &&
            instruction[2] == 0x01U &&
            cursor_increment_position == SIZE_MAX)
        {
            cursor_register = (instruction[1] >> 3U) & 0x07U;
            cursor_increment_position = position;
            position += 3U;
            continue;
        }
        if (remaining >= 6U && instruction[0] == 0x8BU &&
            (instruction[1] & 0xC7U) == 0x05U &&
            frame_load_position == SIZE_MAX)
        {
            frame_register = (instruction[1] >> 3U) & 0x07U;
            candidate->frame_count_address = ReadU32(instruction + 2U);
            frame_load_position = position;
            position += 6U;
            continue;
        }
        if (instruction[0] >= 0x40U && instruction[0] <= 0x47U &&
            frame_increment_position == SIZE_MAX)
        {
            increment_register = instruction[0] - 0x40U;
            frame_increment_position = position;
            ++position;
            continue;
        }
        if (remaining >= 6U && instruction[0] == 0x89U &&
            (instruction[1] & 0xC7U) == 0x05U &&
            store_count < stores.size())
        {
            stores[store_count].source_register =
                (instruction[1] >> 3U) & 0x07U;
            stores[store_count].address = ReadU32(instruction + 2U);
            stores[store_count].position = position;
            ++store_count;
            position += 6U;
            continue;
        }
        if (remaining >= 6U && instruction[0] == 0x8AU &&
            instruction[1] == 0x80U && byte_load_position == SIZE_MAX)
        {
            candidate->source_buffer_address = ReadU32(instruction + 2U);
            byte_load_position = position;
            position += 6U;
            continue;
        }
        if (remaining >= 2U && instruction[0] == 0x89U &&
            instruction[1] == 0xF2U && port_restore_position == SIZE_MAX)
        {
            port_restore_position = position;
            position += 2U;
            continue;
        }
        return false;
    }

    if (store_count != stores.size() ||
        cursor_increment_position == SIZE_MAX ||
        frame_load_position == SIZE_MAX ||
        frame_increment_position == SIZE_MAX ||
        byte_load_position == SIZE_MAX || port_restore_position == SIZE_MAX ||
        !IsSafeTemporaryRegister(cursor_register) ||
        !IsSafeTemporaryRegister(frame_register) ||
        increment_register != frame_register)
    {
        return false;
    }

    for (const AbsoluteStore& store : stores)
    {
        if (store.address == candidate->source_cursor_address &&
            store.source_register == cursor_register &&
            cursor_store_position == SIZE_MAX)
        {
            cursor_store_position = store.position;
        }
        else if (store.address == candidate->frame_count_address &&
                 store.source_register == frame_register &&
                 frame_store_position == SIZE_MAX)
        {
            frame_store_position = store.position;
        }
        else
        {
            return false;
        }
    }

    return candidate->source_cursor_address !=
            candidate->frame_count_address &&
        cursor_store_position != SIZE_MAX && frame_store_position != SIZE_MAX &&
        cursor_increment_position < cursor_store_position &&
        cursor_increment_position < byte_load_position &&
        frame_load_position < frame_increment_position &&
        frame_increment_position < frame_store_position &&
        byte_load_position < port_restore_position &&
        cursor_store_position < port_restore_position &&
        (cursor_register != frame_register ||
         cursor_store_position < frame_load_position) &&
        (frame_register != 2U ||
         frame_store_position < port_restore_position);
}

bool DecodeFeederSuffix(const std::uint8_t* out,
                        FeederLayout* candidate,
                        std::int32_t* loop_displacement)
{
    if (out == nullptr || candidate == nullptr ||
        loop_displacement == nullptr || out[0] != 0xEEU ||
        out[1] != 0xA1U || out[6] != 0x8BU ||
        (out[7] & 0xC7U) != 0x05U || out[12] != 0x41U ||
        out[13] != 0x39U || (out[14] & 0xC7U) != 0xC0U ||
        out[15] != 0x0FU || out[16] != 0x85U)
    {
        return false;
    }

    const std::uint8_t target_register = (out[7] >> 3U) & 0x07U;
    const std::uint8_t compare_register = (out[14] >> 3U) & 0x07U;
    if (!IsSafeTemporaryRegister(target_register) ||
        compare_register != target_register)
    {
        return false;
    }
    if (ReadU32(out + 2U) != candidate->frame_count_address)
    {
        return false;
    }

    candidate->frame_target_address = ReadU32(out + 8U);
    std::memcpy(loop_displacement, out + 17U,
                sizeof(*loop_displacement));
    return true;
}

void LimitPlanToTransferControlBoundary(
    Piu10Mp3FrameBatchPlan* plan, std::uint32_t guest_ecx)
{
    if (plan == nullptr || plan->source_cursor == nullptr ||
        plan->service_counter_limit == 0U)
    {
        return;
    }
    const std::uint32_t cursor = *plan->source_cursor;
    const std::uint32_t cursor_distance =
        cursor < plan->service_cursor_threshold
        ? plan->service_cursor_threshold - cursor : 0U;
    const std::uint32_t count_distance =
        guest_ecx < plan->service_counter_limit - 1U
        ? plan->service_counter_limit - 1U - guest_ecx : 0U;
    const std::size_t boundary_distance =
        std::max(cursor_distance, count_distance);
    plan->bytes = plan->bytes.first(
        std::min(plan->bytes.size(), boundary_distance));
}

void LimitPlanToDemandBoundary(ThreadContext* context,
                               Piu10Mp3FrameBatchPlan* plan)
{
    if (context == nullptr || plan == nullptr)
    {
        return;
    }
    const std::size_t inflight =
        context->piu10_mp3_audio.stats().inflight_bytes;
    const std::size_t headroom =
        inflight < Piu10Mp3AudioOut::kCompressedFifoBytes
        ? Piu10Mp3AudioOut::kCompressedFifoBytes - inflight : 0U;
    plan->bytes = plan->bytes.first(
        std::min(plan->bytes.size(), headroom));
}

bool AddSignedAddress(std::uint32_t base, std::int64_t displacement,
                      std::uint32_t* result)
{
    const std::int64_t value = static_cast<std::int64_t>(base) + displacement;
    if (result == nullptr || value < 0 || value > UINT32_MAX)
    {
        return false;
    }
    *result = static_cast<std::uint32_t>(value);
    return true;
}

bool DecodeDirectFeederLayout(ThreadContext* context,
                              std::uint32_t guest_source,
                              FeederLayout* layout)
{
    if (context == nullptr || layout == nullptr ||
        guest_source < kCodePrefixBytes)
    {
        return false;
    }
    const auto* out = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(guest_source));
    const auto* code = out - kCodePrefixBytes;
    if (!IsGuestRangeReadable(
            context, code,
            static_cast<std::uint32_t>(
                kCodePrefixBytes + kCodeSuffixBytes)))
    {
        return false;
    }

    FeederLayout candidate;
    std::int32_t loop_displacement = 0;
    if (!DecodeFeederPrefix(code, out, &candidate) ||
        !DecodeFeederSuffix(out, &candidate, &loop_displacement))
    {
        return false;
    }

    std::uint32_t loop_address = 0U;
    if (!AddSignedAddress(
            guest_source,
            static_cast<std::int64_t>(kCodeSuffixBytes) + loop_displacement,
            &loop_address) ||
        loop_address >= guest_source ||
        guest_source - loop_address > kMaximumLoopBackBytes)
    {
        return false;
    }
    const auto* loop = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(loop_address));
    if (!IsGuestRangeReadable(context, loop, 27U) ||
        loop[0] != 0x8BU || loop[1] != 0x15U ||
        loop[6] != 0xA1U || loop[11] != 0x39U || loop[12] != 0xD0U ||
        loop[13] != 0x7CU ||
        ReadU32(loop + 7) != candidate.source_cursor_address)
    {
        return false;
    }
    candidate.available_end_address = ReadU32(loop + 2);

    std::uint32_t service_address = 0U;
    if (!AddSignedAddress(
            loop_address + 15U, static_cast<std::int8_t>(loop[14]),
            &service_address))
    {
        return false;
    }
    const auto* service = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(service_address));
    if (service_address <= loop_address || service_address >= guest_source ||
        !IsGuestRangeReadable(context, service, 12U) ||
        service[0] != 0x83U || service[1] != 0xF9U ||
        service[3] != 0x7CU || service[5] != 0x3DU ||
        service[10] != 0x7CU || service[2] == 0U ||
        (service[2] & 0x80U) != 0U)
    {
        return false;
    }
    std::uint32_t first_target = 0U;
    std::uint32_t second_target = 0U;
    if (!AddSignedAddress(
            service_address + 5U, static_cast<std::int8_t>(service[4]),
            &first_target) ||
        !AddSignedAddress(
            service_address + 12U, static_cast<std::int8_t>(service[11]),
            &second_target) ||
        first_target != second_target || first_target <= service_address ||
        first_target >= guest_source - kCodePrefixBytes)
    {
        return false;
    }
    candidate.service_counter_limit = service[2];
    candidate.service_cursor_threshold = ReadU32(service + 6);
    *layout = candidate;
    return true;
}

bool DecodeWrappedFeederLayout(ThreadContext* context,
                               std::uint32_t guest_source,
                               std::uint32_t guest_stack_pointer,
                               FeederLayout* layout)
{
    if (context == nullptr || layout == nullptr || guest_source < 7U ||
        guest_stack_pointer == 0U)
    {
        return false;
    }

    const std::uint32_t wrapper_address = guest_source - 7U;
    const auto* wrapper = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(wrapper_address));
    constexpr std::array<std::uint8_t, kOutputWrapperBytes> kOutputWrapper = {
        0x53U, 0x89U, 0xC3U, 0x88U, 0xD0U,
        0x89U, 0xDAU, 0xEEU, 0x5BU, 0xC3U};
    if (!IsGuestRangeReadable(context, wrapper, kOutputWrapperBytes) ||
        !std::equal(kOutputWrapper.begin(), kOutputWrapper.end(), wrapper))
    {
        return false;
    }

    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(guest_stack_pointer));
    if (!IsGuestRangeReadable(context, stack, 2U * sizeof(*stack)))
    {
        return false;
    }
    const std::uint32_t return_address = stack[1];
    if (return_address < 5U + kWrappedPrefixBytes)
    {
        return false;
    }
    const std::uint32_t call_address = return_address - 5U;
    const auto* call = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(call_address));
    const auto* prefix = call - kWrappedPrefixBytes;
    const auto* suffix = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(return_address));
    if (!IsGuestRangeReadable(
            context, prefix,
            static_cast<std::uint32_t>(
                kWrappedPrefixBytes + 5U + kWrappedSuffixBytes)) ||
        call[0] != 0xE8U)
    {
        return false;
    }
    std::int32_t call_displacement = 0;
    std::memcpy(&call_displacement, call + 1U, sizeof(call_displacement));
    std::uint32_t call_target = 0U;
    if (!AddSignedAddress(return_address, call_displacement, &call_target) ||
        call_target != wrapper_address)
    {
        return false;
    }

    FeederLayout candidate;
    if (prefix[0] != 0xA1U || prefix[5] != 0x31U ||
        prefix[6] != 0xD2U || prefix[7] != 0x8AU ||
        prefix[8] != 0x90U || prefix[13] != 0x40U ||
        prefix[14] != 0xA3U || prefix[19] != 0xFFU ||
        prefix[20] != 0x05U || prefix[25] != 0x41U ||
        prefix[26] != 0xB8U || ReadU32(prefix + 27U) != 0x02DAU)
    {
        return false;
    }
    candidate.source_cursor_address = ReadU32(prefix + 1U);
    candidate.source_buffer_address = ReadU32(prefix + 9U);
    if (ReadU32(prefix + 15U) != candidate.source_cursor_address)
    {
        return false;
    }
    candidate.frame_count_address = ReadU32(prefix + 21U);
    if (candidate.source_cursor_address == candidate.frame_count_address ||
        suffix[0] != 0xA1U ||
        ReadU32(suffix + 1U) != candidate.frame_count_address ||
        suffix[5] != 0x3BU || suffix[6] != 0x05U ||
        suffix[11] != 0x0FU || suffix[12] != 0x85U)
    {
        return false;
    }
    candidate.frame_target_address = ReadU32(suffix + 7U);
    std::int32_t loop_displacement = 0;
    std::memcpy(&loop_displacement, suffix + 13U,
                sizeof(loop_displacement));
    std::uint32_t branch_end_address = 0U;
    std::uint32_t loop_address = 0U;
    if (!AddSignedAddress(
            return_address, kWrappedSuffixBytes, &branch_end_address) ||
        !AddSignedAddress(
            branch_end_address, loop_displacement, &loop_address) ||
        loop_address >= call_address ||
        call_address - loop_address > kMaximumLoopBackBytes)
    {
        return false;
    }

    const auto* loop = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(loop_address));
    if (!IsGuestRangeReadable(context, loop, 13U) ||
        loop[0] != 0xA1U ||
        ReadU32(loop + 1U) != candidate.source_cursor_address ||
        loop[5] != 0x3BU || loop[6] != 0x05U || loop[11] != 0x7CU)
    {
        return false;
    }
    candidate.available_end_address = ReadU32(loop + 7U);
    std::uint32_t service_address = 0U;
    if (!AddSignedAddress(
            loop_address + 13U, static_cast<std::int8_t>(loop[12]),
            &service_address))
    {
        return false;
    }
    const auto* service = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(service_address));
    if (service_address <= loop_address || service_address >= call_address ||
        !IsGuestRangeReadable(context, service, 12U) ||
        service[0] != 0x83U || service[1] != 0xF9U ||
        service[3] != 0x7CU || service[5] != 0x3DU ||
        service[10] != 0x7CU || service[2] == 0U ||
        (service[2] & 0x80U) != 0U)
    {
        return false;
    }
    std::uint32_t first_target = 0U;
    std::uint32_t second_target = 0U;
    if (!AddSignedAddress(
            service_address + 5U, static_cast<std::int8_t>(service[4]),
            &first_target) ||
        !AddSignedAddress(
            service_address + 12U, static_cast<std::int8_t>(service[11]),
            &second_target) ||
        first_target != second_target || first_target <= service_address ||
        first_target >= call_address - kWrappedPrefixBytes)
    {
        return false;
    }
    candidate.service_counter_limit = service[2];
    candidate.service_cursor_threshold = ReadU32(service + 6U);
    *layout = candidate;
    return true;
}

bool DecodeFeederLayout(ThreadContext* context, std::uint32_t guest_source,
                        std::uint32_t guest_stack_pointer,
                        FeederLayout* layout)
{
    return DecodeDirectFeederLayout(context, guest_source, layout) ||
        DecodeWrappedFeederLayout(
            context, guest_source, guest_stack_pointer, layout);
}

bool AuditPiu10Mp3FrameTail(
    ThreadContext* context, std::uint32_t guest_source,
    std::uint32_t guest_stack_pointer, std::uint8_t current_byte,
    std::uint32_t* guest_ecx)
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
        FeederLayout layout;
        if (!DecodeFeederLayout(
                context, guest_source, guest_stack_pointer, &layout))
        {
            return true;
        }
        const auto* cursor = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(layout.source_cursor_address));
        const auto* count = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(layout.frame_count_address));
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
                context, guest_source, guest_stack_pointer,
                context->piu10_mp3_frame_batch_audit_bytes.size(), &plan))
        {
            LimitPlanToTransferControlBoundary(&plan, *guest_ecx);
            LimitPlanToDemandBoundary(context, &plan);
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
    std::uint32_t guest_stack_pointer, std::size_t maximum_bytes,
    Piu10Mp3FrameBatchPlan* plan)
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

    FeederLayout layout;
    if (!DecodeFeederLayout(
            context, guest_source, guest_stack_pointer, &layout))
    {
        ReportRejection(context, BatchRejection::kShape, "shape",
                        guest_source, context->runtime_base,
                        context->runtime_size);
        return false;
    }

    auto* source_cursor = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uintptr_t>(layout.source_cursor_address));
    auto* frame_count = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uintptr_t>(layout.frame_count_address));
    const auto* available_end = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(layout.available_end_address));
    const auto* frame_target = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(layout.frame_target_address));
    if (!IsGuestRangeWritable(context, source_cursor, sizeof(*source_cursor)) ||
        !IsGuestRangeWritable(context, frame_count, sizeof(*frame_count)) ||
        !IsGuestRangeReadable(context, available_end, sizeof(*available_end)) ||
        !IsGuestRangeReadable(context, frame_target, sizeof(*frame_target)))
    {
        ReportRejection(context, BatchRejection::kStateRange, "state-range",
                        guest_source, layout.source_cursor_address,
                        layout.available_end_address,
                        layout.frame_target_address,
                        layout.frame_count_address);
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
            layout.source_buffer_address)
    {
        ReportRejection(context, BatchRejection::kSourceAddress,
                        "source-address", guest_source, cursor,
                        layout.source_buffer_address,
                        static_cast<std::uint32_t>(batch_bytes),
                        static_cast<std::uint32_t>(maximum_bytes));
        return false;
    }
    const auto* source = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(layout.source_buffer_address + cursor));
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
    plan->service_cursor_threshold = layout.service_cursor_threshold;
    plan->service_counter_limit = layout.service_counter_limit;
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
    std::uint32_t guest_stack_pointer, std::uint8_t current_byte,
    std::uint32_t* guest_ecx)
{
    if (AuditPiu10Mp3FrameTail(
            context, guest_source, guest_stack_pointer, current_byte,
            guest_ecx))
    {
        return 0U;
    }
    Piu10Mp3FrameBatchPlan plan;
    if (guest_ecx == nullptr ||
        !BuildPiu10Mp3FrameBatchPlan(
            context, guest_source, guest_stack_pointer,
            kMaximumMpegFrameBytes, &plan))
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
