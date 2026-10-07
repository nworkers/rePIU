#include "native_linear_span.h"

#include "execution/thread_context.h"
#include "verified_region_analyzer.h"

#include <cstring>
#include <cstddef>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/host_environment.h"
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

bool NativeLinearSpanRejectCacheEnabled(
    runtime::ExecutionBackend execution_backend)
{
    return ResolveNativeLinearSpanRejectCacheEnabled(execution_backend);
}

}  // namespace

bool ResolveNativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend,
    std::string_view setting)
{
    return ResolveNativeLinearSpanSetting(
        execution_backend, ParseNativeLinearSpanSetting(setting));
}

// The negative cache (Task 304) is on wherever spans are on by default: the
// dynamic backend on a host with hardware debug registers. It has no setting.
bool ResolveNativeLinearSpanRejectCacheEnabled(
    runtime::ExecutionBackend execution_backend)
{
    return ResolveNativeLinearSpanSetting(
        execution_backend, NativeLinearSpanSetting::kBackendDefault);
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
    if (state->active || state->linear_span_active)
    {
        return false;
    }
    const std::uint32_t entry =
        static_cast<std::uint32_t>(win32_context->Eip);
    detail::NativeLinearSpan span;
    const bool reject_cache_enabled =
        NativeLinearSpanRejectCacheEnabled(context->execution_backend);
    if (reject_cache_enabled &&
        detail::LookupNativeLinearSpanRejectCache(state, entry))
    {
        state->linear_span_reject_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    if (!detail::ScanNativeLinearSpanWithZydis(
            entry, context->runtime_base, context->runtime_size, &span))
    {
        if (reject_cache_enabled)
        {
            detail::StoreNativeLinearSpanRejectCache(
                state, entry, span);
        }
        state->linear_span_reject_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
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

namespace detail
{

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
