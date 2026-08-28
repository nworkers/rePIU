#include "native_linear_span.h"

#include "execution/thread_context.h"
#include "aot/aot_runtime_dispatch.h"
#include "repiu/engine/aot_page_coherence_win32.h"
#include "verified_region_analyzer.h"

#include <Zydis.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/atomic_ops.h"
#include "repiu/platform/host_environment.h"
#include "repiu/platform/virtual_memory.h"
#include "repiu/platform/fault_handler.h"

namespace repiu::engine
{

namespace
{

// The sixteen-byte buffer the Win32 reads used; the
// values looked for are all short words, and anything longer was
// treated as deliberately off.
constexpr std::size_t kSettingCapacity = 16U;

enum class NativeLinearSpanSetting
{
    kBackendDefault,
    kEnabled,
    kDisabled
};

NativeLinearSpanSetting ParseNativeLinearSpanSetting(
    std::string_view setting)
{
    if (setting.empty())
    {
        return NativeLinearSpanSetting::kBackendDefault;
    }
    if (setting == "1" || setting == "on" || setting == "true")
    {
        return NativeLinearSpanSetting::kEnabled;
    }
    return NativeLinearSpanSetting::kDisabled;
}

NativeLinearSpanSetting ReadNativeLinearSpanSetting()
{
    const auto setting = repiu::platform::ReadEnvironmentSetting(
        "REPIU_NATIVE_LINEAR_SPAN", kSettingCapacity);
    if (!setting.present)
    {
        return NativeLinearSpanSetting::kBackendDefault;
    }
    if (setting.too_long)
    {
        return NativeLinearSpanSetting::kDisabled;
    }
    return ParseNativeLinearSpanSetting(setting.value);
}

bool ReadNativeLinearSpanCacheSetting()
{
    const auto setting = repiu::platform::ReadEnvironmentSetting(
        "REPIU_NATIVE_LINEAR_SPAN_CACHE", kSettingCapacity);
    if (!setting.present || setting.too_long)
    {
        return false;
    }
    return setting.value == "1" || setting.value == "on" ||
        setting.value == "true";
}

NativeLinearSpanSetting ReadNativeLinearSpanRejectCacheSetting()
{
    const auto setting = repiu::platform::ReadEnvironmentSetting(
        "REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE", kSettingCapacity);
    if (!setting.present)
    {
        return NativeLinearSpanSetting::kBackendDefault;
    }
    if (setting.too_long)
    {
        return NativeLinearSpanSetting::kDisabled;
    }
    return ParseNativeLinearSpanSetting(setting.value);
}

NativeLinearSpanSetting ReadRetiredTrapNativeSpanSetting()
{
    const auto setting = repiu::platform::ReadEnvironmentSetting(
        "REPIU_AOT_RETIRED_SPAN_REENTRY", kSettingCapacity);
    if (!setting.present)
    {
        return NativeLinearSpanSetting::kBackendDefault;
    }
    if (setting.too_long)
    {
        return NativeLinearSpanSetting::kDisabled;
    }
    return ParseNativeLinearSpanSetting(setting.value);
}

bool ReadNativeLinearSpanWritesSetting()
{
    const auto setting = repiu::platform::ReadEnvironmentSetting(
        "REPIU_NATIVE_LINEAR_SPAN_WRITES", kSettingCapacity);
    if (!setting.present || setting.too_long)
    {
        return false;
    }
    return ResolveNativeLinearSpanWritesEnabled(setting.value);
}

bool ReadNativeLinearSpanJumpsSetting()
{
    const auto setting = repiu::platform::ReadEnvironmentSetting(
        "REPIU_NATIVE_LINEAR_SPAN_JUMPS", kSettingCapacity);
    if (!setting.present || setting.too_long)
    {
        return false;
    }
    return ResolveNativeLinearSpanJumpsEnabled(setting.value);
}

bool ResolveNativeLinearSpanSetting(
    runtime::ExecutionBackend execution_backend,
    NativeLinearSpanSetting setting)
{
    // Task 503d-23. Ahead of the explicit setting, because this one is not a
    // preference. A span is entered by arming a `Dr` breakpoint and clearing the
    // trap flag; where the arming is discarded the guest is released with
    // nothing to bring it back. `kEnabled` may turn this off-by-default feature
    // on, but not on a host where turning it on means losing the guest.
    if (!repiu::platform::HardwareDebugRegistersAvailable())
    {
        return false;
    }
    if (setting == NativeLinearSpanSetting::kEnabled)
    {
        return true;
    }
    if (setting == NativeLinearSpanSetting::kDisabled)
    {
        return false;
    }
    // Unset defaults to ON only on the dynamic backend; legacy requires an
    // explicit setting to turn it on.
    return runtime::ExecutionBackendUsesDynamicTranslation(execution_backend);
}

bool NativeLinearSpanCacheEnabled()
{
    static const bool enabled = ReadNativeLinearSpanCacheSetting();
    return enabled;
}

bool NativeLinearSpanRejectCacheEnabled(
    runtime::ExecutionBackend execution_backend)
{
    static const NativeLinearSpanSetting setting =
        ReadNativeLinearSpanRejectCacheSetting();
    return ResolveNativeLinearSpanSetting(execution_backend, setting);
}

bool RetiredTrapNativeSpanPolicyEnabled(
    runtime::ExecutionBackend execution_backend)
{
    static const NativeLinearSpanSetting setting =
        ReadRetiredTrapNativeSpanSetting();
    if (setting == NativeLinearSpanSetting::kBackendDefault)
    {
        return false;
    }
    return ResolveNativeLinearSpanSetting(execution_backend, setting);
}

bool NativeLinearSpanWritesEnabled()
{
    static const bool enabled = ReadNativeLinearSpanWritesSetting();
    return enabled;
}

bool NativeLinearSpanJumpsEnabled()
{
    static const bool enabled = ReadNativeLinearSpanJumpsSetting();
    return enabled;
}

struct NativeLinearSpanScanContext
{
    const ThreadContext* thread = nullptr;
    const repiu::platform::GuestCpuContext* registers = nullptr;
    detail::NativeFastPathState* state = nullptr;
};

bool IsNativeLinearSpanPageWriteGuarded(
    void* opaque_context,
    std::uint32_t guest_page)
{
    const auto* scan =
        static_cast<const NativeLinearSpanScanContext*>(opaque_context);
    return scan != nullptr && scan->thread != nullptr &&
        !HasPendingWin32AotGuestWrite(
            scan->thread->aot_page_write_watch) &&
        IsWin32AotGuestPageWriteWatched(
            scan->thread->aot_page_write_watch, guest_page);
}

bool ReadNativeLinearSpanRegister(
    void* opaque_context,
    std::uint32_t zydis_register,
    std::uint32_t* value)
{
    const auto* scan =
        static_cast<const NativeLinearSpanScanContext*>(opaque_context);
    if (scan == nullptr || scan->registers == nullptr || value == nullptr)
    {
        return false;
    }
    const repiu::platform::GuestCpuContext& registers = *scan->registers;
    switch (static_cast<ZydisRegister>(zydis_register))
    {
    case ZYDIS_REGISTER_EAX: *value = registers.Eax; return true;
    case ZYDIS_REGISTER_ECX: *value = registers.Ecx; return true;
    case ZYDIS_REGISTER_EDX: *value = registers.Edx; return true;
    case ZYDIS_REGISTER_EBX: *value = registers.Ebx; return true;
    case ZYDIS_REGISTER_ESP: *value = registers.Esp; return true;
    case ZYDIS_REGISTER_EBP: *value = registers.Ebp; return true;
    case ZYDIS_REGISTER_ESI: *value = registers.Esi; return true;
    case ZYDIS_REGISTER_EDI: *value = registers.Edi; return true;
    default: return false;
    }
}

bool IsNativeLinearSpanWriteTargetAllowed(
    void* opaque_context,
    std::uint32_t address,
    std::uint32_t byte_count)
{
    const auto* scan =
        static_cast<const NativeLinearSpanScanContext*>(opaque_context);
    const std::uint64_t begin = scan != nullptr && scan->thread != nullptr
        ? scan->thread->runtime_base
        : 0U;
    const std::uint64_t arena_end = scan != nullptr && scan->thread != nullptr
        ? begin + scan->thread->runtime_size
        : 0U;
    const std::uint64_t end = static_cast<std::uint64_t>(address) +
        byte_count;
    if (scan == nullptr || scan->thread == nullptr || scan->state == nullptr ||
        byte_count == 0U ||
        end > 0x100000000ULL || arena_end > 0x100000000ULL)
    {
        return false;
    }
    if (address < begin || end < address || end > arena_end)
    {
        return false;
    }
    std::uint64_t cursor = address;
    while (cursor < end)
    {
        const std::uint32_t current = static_cast<std::uint32_t>(cursor);
        const std::uint32_t page = Win32AotGuestPage(current);
        bool writable = IsWin32AotGuestPageWriteWatched(
            scan->thread->aot_page_write_watch, page);
        if (!writable)
        {
            const auto cached =
                scan->state->linear_span_write_target_page_cache.find(page);
            if (cached !=
                scan->state->linear_span_write_target_page_cache.end())
            {
                writable = cached->second;
            }
            else
            {
                // Another of the hand-written protection classifiers 3b's
                // QueryMemory was built to answer directly -- this one asking
                // about writing rather than reading.
                const repiu::platform::MemoryRegion region =
                    repiu::platform::QueryMemory(
                        reinterpret_cast<const void*>(
                            static_cast<std::uintptr_t>(current)));
                writable = region.valid && region.committed && region.writable;
                scan->state->linear_span_write_target_page_cache[page] =
                    writable;
            }
        }
        if (!writable)
        {
            return false;
        }
        cursor = std::min<std::uint64_t>(
            end, static_cast<std::uint64_t>(page) + 0x1000U);
    }
    return true;
}

bool IsNativeLinearSpanDirectJumpTargetAllowed(
    void* opaque_context,
    std::uint32_t target)
{
    const auto* scan =
        static_cast<const NativeLinearSpanScanContext*>(opaque_context);
    return scan != nullptr && scan->thread != nullptr &&
        scan->thread->aot_placement != nullptr &&
        !IsAotHleBoundaryAddress(scan->thread, target) &&
        !IsWin32AotGuestPageQuarantined(
            *scan->thread->aot_placement, target);
}

bool QueryNativeLinearSpanGeneration(
    const ThreadContext* context,
    std::uint32_t entry,
    std::uint32_t* generation)
{
    return context != nullptr &&
        context->aot_placement != nullptr &&
        IsWin32AotGuestPageWriteWatched(
            context->aot_page_write_watch, entry) &&
        QueryWin32AotActiveGuestPageGeneration(
            *context->aot_placement, entry, generation);
}

}  // namespace

bool ResolveNativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend,
    std::string_view setting)
{
    return ResolveNativeLinearSpanSetting(
        execution_backend, ParseNativeLinearSpanSetting(setting));
}

bool ResolveNativeLinearSpanCacheEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool ResolveNativeLinearSpanRejectCacheEnabled(
    runtime::ExecutionBackend execution_backend,
    std::string_view setting)
{
    return ResolveNativeLinearSpanSetting(
        execution_backend, ParseNativeLinearSpanSetting(setting));
}

bool ResolveRetiredTrapNativeSpanEnabled(
    runtime::ExecutionBackend,
    std::string_view setting)
{
    return ParseNativeLinearSpanSetting(setting) ==
        NativeLinearSpanSetting::kEnabled;
}

bool RetiredTrapNativeSpanEnabled(
    runtime::ExecutionBackend execution_backend)
{
    return RetiredTrapNativeSpanPolicyEnabled(execution_backend);
}

bool ResolveNativeLinearSpanWritesEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool ResolveNativeLinearSpanJumpsEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool NativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend)
{
    static const NativeLinearSpanSetting setting =
        ReadNativeLinearSpanSetting();
    return ResolveNativeLinearSpanSetting(execution_backend, setting);
}

void LeaveNativeLinearSpan(repiu::platform::GuestCpuContext* win32_context,
                           ThreadContext* context,
                           bool reached_boundary,
                           bool write_fault_cancel,
                           repiu::platform::FaultKind fault_kind,
                           std::uint32_t exception_code)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    if (!state->linear_span_active)
    {
        return;
    }
    const std::uint32_t debug_status =
        static_cast<std::uint32_t>(win32_context->Dr6);
    const std::uint32_t entry_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    win32_context->Dr0 = state->linear_span_saved_dr0;
    win32_context->Dr6 = state->linear_span_saved_dr6;
    win32_context->Dr7 = state->linear_span_saved_dr7;
    win32_context->EFlags |= 0x00000100U;
    state->linear_span_active = false;
    if (reached_boundary)
    {
        state->linear_span_boundary_count.fetch_add(
            1, std::memory_order_relaxed);
        state->linear_span_instruction_total.fetch_add(
            state->linear_span_instruction_count,
            std::memory_order_relaxed);
    }
    else if (write_fault_cancel)
    {
        state->linear_span_write_fault_cancel_count.fetch_add(
            1, std::memory_order_relaxed);
    }
    else
    {
        state->linear_span_cancel_count.fetch_add(
            1, std::memory_order_relaxed);
        state->linear_span_last_cancel_code.store(
            exception_code, std::memory_order_relaxed);
        state->linear_span_last_cancel_eip.store(
            entry_eip,
            std::memory_order_relaxed);
        if (fault_kind == repiu::platform::FaultKind::kSingleStep)
        {
            std::atomic<std::uint32_t>* count =
                &state->linear_span_cancel_other_db_count;
            std::atomic<std::uint32_t>* first_eip =
                &state->linear_span_cancel_other_db_first_eip;
            if ((debug_status & 0x1U) != 0U)
            {
                count = &state->linear_span_cancel_dr0_count;
                first_eip = &state->linear_span_cancel_dr0_first_eip;
            }
            else if ((debug_status & 0x2U) != 0U)
            {
                count = &state->linear_span_cancel_dr1_count;
                first_eip = &state->linear_span_cancel_dr1_first_eip;
            }
            else if ((debug_status & 0x4U) != 0U)
            {
                count = &state->linear_span_cancel_dr2_count;
                first_eip = &state->linear_span_cancel_dr2_first_eip;
            }
            else if ((debug_status & 0x8U) != 0U)
            {
                count = &state->linear_span_cancel_dr3_count;
                first_eip = &state->linear_span_cancel_dr3_first_eip;
            }
            else if ((debug_status & 0x4000U) != 0U)
            {
                count = &state->linear_span_cancel_tf_count;
                first_eip = &state->linear_span_cancel_tf_first_eip;
            }
            count->fetch_add(1, std::memory_order_relaxed);
            std::uint32_t expected = 0U;
            first_eip->compare_exchange_strong(
                expected, entry_eip, std::memory_order_relaxed);
        }
    }
}

bool TryEnterNativeLinearSpan(repiu::platform::GuestCpuContext* win32_context,
                              ThreadContext* context)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    if (state->active || state->region_active || state->linear_span_active)
    {
        return false;
    }
    const std::uint32_t entry =
        static_cast<std::uint32_t>(win32_context->Eip);
    detail::NativeLinearSpan span;
    const bool cache_enabled = NativeLinearSpanCacheEnabled();
    const bool writes_enabled = NativeLinearSpanWritesEnabled() &&
        !HasPendingWin32AotGuestWrite(context->aot_page_write_watch) &&
        IsWin32AotGuestPageWriteWatched(
            context->aot_page_write_watch, entry);
    const bool jumps_enabled = NativeLinearSpanJumpsEnabled();
    const bool reject_cache_enabled =
        NativeLinearSpanRejectCacheEnabled(context->execution_backend) &&
        !writes_enabled && !jumps_enabled;
    NativeLinearSpanScanContext scan_context = {
        context, win32_context, state};
    detail::NativeLinearSpanOptions scan_options;
    scan_options.allow_memory_writes = writes_enabled;
    scan_options.write_guard_query = writes_enabled
        ? &IsNativeLinearSpanPageWriteGuarded
        : nullptr;
    scan_options.register_query = writes_enabled
        ? &ReadNativeLinearSpanRegister
        : nullptr;
    scan_options.write_target_query = writes_enabled
        ? &IsNativeLinearSpanWriteTargetAllowed
        : nullptr;
    scan_options.chain_forward_direct_jumps = jumps_enabled;
    scan_options.direct_jump_target_query = jumps_enabled
        ? &IsNativeLinearSpanDirectJumpTargetAllowed
        : nullptr;
    scan_options.write_guard_context = &scan_context;
    std::uint32_t generation = 0;
    const bool cacheable_page = cache_enabled &&
        QueryNativeLinearSpanGeneration(context, entry, &generation);
    bool scan_succeeded = false;
    if (reject_cache_enabled &&
        detail::LookupNativeLinearSpanRejectCache(state, entry))
    {
        state->linear_span_reject_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    if (cacheable_page)
    {
        scan_succeeded = detail::LookupNativeLinearSpanScanCache(
            state, entry, Win32AotGuestPage(entry), generation, &span);
    }
    else if (cache_enabled)
    {
        state->linear_span_cache_miss_count.fetch_add(
            1, std::memory_order_relaxed);
    }
    if (!scan_succeeded)
    {
        scan_succeeded = detail::ScanNativeLinearSpanWithZydis(
            entry, context->runtime_base, context->runtime_size, &span,
            (writes_enabled || jumps_enabled) ? &scan_options : nullptr);
        if (!scan_succeeded && reject_cache_enabled)
        {
            detail::StoreNativeLinearSpanRejectCache(
                state, entry, span);
        }
        if (scan_succeeded && cacheable_page &&
            Win32AotGuestPage(span.boundary_address) ==
                Win32AotGuestPage(entry))
        {
            detail::StoreNativeLinearSpanScanCache(
                state, entry, Win32AotGuestPage(entry), generation, span);
        }
    }
    if (span.boundary_write_guard_uncovered)
    {
        state->linear_span_write_guard_uncovered_count.fetch_add(
            1, std::memory_order_relaxed);
    }
    if (span.boundary_backward_jump)
    {
        state->linear_span_backward_jump_stop_count.fetch_add(
            1, std::memory_order_relaxed);
    }
    if (!scan_succeeded)
    {
        state->linear_span_reject_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    state->linear_span_write_cross_count.fetch_add(
        span.crossed_memory_write_count, std::memory_order_relaxed);
    state->linear_span_direct_jump_chain_count.fetch_add(
        span.chained_direct_jump_count, std::memory_order_relaxed);
    state->linear_span_boundary = span.boundary_address;
    state->linear_span_instruction_count = span.instruction_count;
    state->linear_span_saved_dr0 =
        static_cast<std::uint32_t>(win32_context->Dr0);
    state->linear_span_saved_dr6 =
        static_cast<std::uint32_t>(win32_context->Dr6);
    state->linear_span_saved_dr7 =
        static_cast<std::uint32_t>(win32_context->Dr7);
    win32_context->Dr0 = span.boundary_address;
    win32_context->Dr6 = 0;
    // Override only slot zero. Dr1-Dr3 are left untouched and the entire Dr7
    // value is restored at the boundary or on any unexpected exception.
    win32_context->Dr7 =
        (static_cast<std::uint32_t>(win32_context->Dr7) & ~0x000F0003U) |
        0x1U;
    win32_context->EFlags &= ~0x00000100U;
    state->linear_span_active = true;
    state->linear_span_entry_count.fetch_add(
        1, std::memory_order_relaxed);
    return true;
}

bool TryEnterRetiredTrapNativeSpan(repiu::platform::GuestCpuContext* win32_context,
                                   ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return false;
    }
    context->aot_retired_span_attempt_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry
                 ->aot_retired_span_attempt_count);
    }
    if (!TryEnterNativeLinearSpan(win32_context, context))
    {
        return false;
    }
    context->aot_retired_span_success_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry
                 ->aot_retired_span_success_count);
    }
    // Preserve the retired fallback's pending-reentry and trace policy. The
    // span clears TF only until Dr0 reaches its boundary; that boundary must
    // resume the exact existing AOT/HLE single-step chain.
    return true;
}

namespace detail
{

bool LookupNativeLinearSpanScanCache(
    NativeFastPathState* state,
    std::uint32_t entry,
    std::uint32_t guest_page,
    std::uint32_t generation,
    NativeLinearSpan* span)
{
    if (state == nullptr || span == nullptr)
    {
        return false;
    }
    const auto cached = state->linear_span_scan_cache.find(entry);
    if (cached == state->linear_span_scan_cache.end())
    {
        state->linear_span_cache_miss_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    if (cached->second.guest_page != guest_page ||
        cached->second.generation != generation)
    {
        state->linear_span_scan_cache.erase(cached);
        state->linear_span_cache_miss_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    *span = cached->second.span;
    state->linear_span_cache_hit_count.fetch_add(
        1, std::memory_order_relaxed);
    return true;
}

void StoreNativeLinearSpanScanCache(
    NativeFastPathState* state,
    std::uint32_t entry,
    std::uint32_t guest_page,
    std::uint32_t generation,
    const NativeLinearSpan& span)
{
    if (state == nullptr)
    {
        return;
    }
    state->linear_span_scan_cache[entry] = {
        guest_page, generation, span};
}

bool LookupNativeLinearSpanRejectCache(
    NativeFastPathState* state,
    std::uint32_t entry)
{
    if (state == nullptr)
    {
        return false;
    }
    const auto cached = state->linear_span_reject_cache.find(entry);
    if (cached == state->linear_span_reject_cache.end())
    {
        state->linear_span_reject_cache_miss_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    const std::uint32_t byte_count = cached->second.byte_count;
    const auto* current = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(entry));
    if (byte_count == 0U ||
        byte_count > kNativeLinearSpanRejectSnapshotCapacity ||
        std::memcmp(cached->second.bytes.data(), current, byte_count) != 0)
    {
        state->linear_span_reject_cache.erase(cached);
        state->linear_span_reject_cache_stale_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    state->linear_span_reject_cache_hit_count.fetch_add(
        1, std::memory_order_relaxed);
    return true;
}

void StoreNativeLinearSpanRejectCache(
    NativeFastPathState* state,
    std::uint32_t entry,
    const NativeLinearSpan& span)
{
    const std::uint32_t byte_count =
        span.cacheable_rejection_byte_count;
    if (state == nullptr || byte_count == 0U ||
        byte_count > kNativeLinearSpanRejectSnapshotCapacity)
    {
        return;
    }
    auto cached = state->linear_span_reject_cache.find(entry);
    if (cached == state->linear_span_reject_cache.end())
    {
        if (state->linear_span_reject_cache.size() >=
            kNativeLinearSpanRejectCacheMaxEntries)
        {
            state->linear_span_reject_cache_capacity_skip_count.fetch_add(
                1, std::memory_order_relaxed);
            return;
        }
        cached = state->linear_span_reject_cache.emplace(
            entry, NativeLinearSpanRejectCacheEntry{}).first;
    }
    cached->second.byte_count = byte_count;
    const auto* source = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(entry));
    std::memcpy(cached->second.bytes.data(), source, byte_count);
    state->linear_span_reject_cache_store_count.fetch_add(
        1, std::memory_order_relaxed);
}

}  // namespace detail

}  // namespace repiu::engine
