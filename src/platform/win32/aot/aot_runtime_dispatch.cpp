#include "aot_runtime_dispatch.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "instruction_emulation.h"

#include <Zydis.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace repiu::platform::win32
{

void BumpAotBoundaryCount(ThreadContext* context)
{
    context->aot_boundary_count.fetch_add(1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_boundary_count);
    }
}

void BumpAotReentryCount(ThreadContext* context)
{
    context->aot_reentry_count.fetch_add(1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_reentry_count);
    }
}

// Live-mirrored the same way as BumpAotBoundaryCount/BumpAotReentryCount so
// a stuck aot_boundary_guest_eip can be cross-checked against repeated
// page retire/re-resolve activity while dispatch is silent (Task 217).
void BumpAotPageRetireAttemptCount(ThreadContext* context)
{
    context->aot_page_retire_attempt_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_page_retire_attempt_count);
    }
}

void BumpAotPageRetireSuccessCount(ThreadContext* context)
{
    context->aot_page_retire_success_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_page_retire_success_count);
    }
}

void BumpAotRetiredEntryTrapCount(ThreadContext* context)
{
    context->aot_retired_entry_trap_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_retired_entry_trap_count);
    }
}

void BumpAotQuarantineCount(ThreadContext* context)
{
    context->aot_quarantine_count.fetch_add(1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_quarantine_count);
    }
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
                              bool force_generation)
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
                BumpAotQuarantineCount(context);
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
    BumpAotReentryCount(context);
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
    BumpAotReentryCount(context);
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
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->aot_last_return_source,
            static_cast<long>(win32_context->Eip));
        InterlockedExchange(
            &context->shared_live_telemetry->aot_last_return_target,
            static_cast<long>(target));
        InterlockedExchange(
            &context->shared_live_telemetry->aot_last_expected_return,
            static_cast<long>(context->aot_last_expected_return));
        InterlockedExchange(
            &context->shared_live_telemetry->aot_last_return_matches_call,
            context->aot_last_return_matches_call ? 1L : 0L);
    }
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
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_return_dispatch_count);
    }
    BumpAotReentryCount(context);
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
        // A tracked execution-trace sentinel byte can stop being hit again on
        // later calls to the same guest address for reasons that go beyond
        // formal cache-entry retirement (empirically, the retirement check
        // below never fired for a sentinel that still stopped re-triggering —
        // see docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md
        // §9). Re-installing the sentinel at whatever cache address is
        // *currently* resolved for the guest address, on every hit, is a
        // strictly more robust fix: it self-heals regardless of the exact
        // underlying mechanism (retranslation, alternate cache copy, etc.).
        const bool is_tracked_trace_address =
            context->execution_trace_configured &&
            (guest_address == context->runtime_base +
                                   context->execution_trace_start_offset ||
             (context->execution_trace_sentinel2_configured &&
              guest_address ==
                  context->runtime_base +
                      context->execution_trace_sentinel2_offset));
        if (IsWin32AotCacheAddressRetired(
                *context->aot_placement, cache_address))
        {
            BumpAotRetiredEntryTrapCount(context);
            if (!is_tracked_trace_address)
            {
                std::uint32_t latest_cache_address = guest_address;
                if (ResolveAotTransferTarget(
                        context, guest_address, &latest_cache_address, true))
                {
                    win32_context->Eip = latest_cache_address;
                    win32_context->EFlags &= ~0x00000100U;
                    context->aot_reentry_pending = false;
                    context->aot_legacy_fallback = false;
                    context->enable_single_step_trace = false;
                    BumpAotReentryCount(context);
                    return true;
                }
            }
        }
        win32_context->Eip = guest_address;
        RecordExecutionProbe(win32_context, context);
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->enable_single_step_trace = true;
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->aot_boundary_guest_eip,
                static_cast<long>(guest_address));
        }
        BumpAotBoundaryCount(context);
        if (is_tracked_trace_address)
        {
            if (InstallWin32AotProbeSentinel(
                    context->aot_placement, guest_address))
            {
                ++context->execution_trace_sentinel_rearm_count;
            }
        }
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
        BumpAotReentryCount(context);
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
        BumpAotReentryCount(context);
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
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->aot_legacy_fallback_count);
        InterlockedExchange(
            &context->shared_live_telemetry->aot_last_fallback_address,
            static_cast<long>(current));
    }
    return false;
}

} // namespace repiu::platform::win32
