#include "native_linear_span.h"

#include "execution/thread_context.h"
#include "aot/aot_runtime_dispatch.h"
#include "repiu/platform/win32/aot_page_coherence_win32.h"
#include "verified_region_analyzer.h"

#include <Zydis.h>

#include <algorithm>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace repiu::platform::win32
{

namespace
{

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
    char value[16] = {};
    const DWORD length = GetEnvironmentVariableA(
        "REPIU_NATIVE_LINEAR_SPAN", value, sizeof(value));
    if (length == 0)
    {
        return NativeLinearSpanSetting::kBackendDefault;
    }
    if (length >= sizeof(value))
    {
        return NativeLinearSpanSetting::kDisabled;
    }
    return ParseNativeLinearSpanSetting(
        std::string_view(value, length));
}

bool ReadNativeLinearSpanCacheSetting()
{
    char value[16] = {};
    const DWORD length = GetEnvironmentVariableA(
        "REPIU_NATIVE_LINEAR_SPAN_CACHE", value, sizeof(value));
    if (length == 0 || length >= sizeof(value))
    {
        return false;
    }
    const std::string_view setting(value, length);
    return setting == "1" || setting == "on" || setting == "true";
}

bool ReadNativeLinearSpanWritesSetting()
{
    char value[16] = {};
    const DWORD length = GetEnvironmentVariableA(
        "REPIU_NATIVE_LINEAR_SPAN_WRITES", value, sizeof(value));
    if (length == 0 || length >= sizeof(value))
    {
        return false;
    }
    return ResolveNativeLinearSpanWritesEnabled(
        std::string_view(value, length));
}

bool ReadNativeLinearSpanJumpsSetting()
{
    char value[16] = {};
    const DWORD length = GetEnvironmentVariableA(
        "REPIU_NATIVE_LINEAR_SPAN_JUMPS", value, sizeof(value));
    if (length == 0 || length >= sizeof(value))
    {
        return false;
    }
    return ResolveNativeLinearSpanJumpsEnabled(
        std::string_view(value, length));
}

bool ResolveNativeLinearSpanSetting(
    runtime::ExecutionBackend execution_backend,
    NativeLinearSpanSetting setting)
{
    if (setting == NativeLinearSpanSetting::kEnabled)
    {
        return true;
    }
    if (setting == NativeLinearSpanSetting::kDisabled)
    {
        return false;
    }
    return execution_backend == runtime::ExecutionBackend::kAotDbt;
}

bool NativeLinearSpanCacheEnabled()
{
    static const bool enabled = ReadNativeLinearSpanCacheSetting();
    return enabled;
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
    const CONTEXT* registers = nullptr;
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
    const CONTEXT& registers = *scan->registers;
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
                MEMORY_BASIC_INFORMATION information{};
                const bool queried = VirtualQuery(
                    reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(current)),
                    &information, sizeof(information)) != 0U;
                const DWORD protection = queried ? information.Protect : 0U;
                const DWORD base_protection = protection & 0xFFU;
                writable = queried && information.State == MEM_COMMIT &&
                    (protection & (PAGE_GUARD | PAGE_NOACCESS)) == 0U &&
                    (base_protection == PAGE_READWRITE ||
                     base_protection == PAGE_WRITECOPY ||
                     base_protection == PAGE_EXECUTE_READWRITE ||
                     base_protection == PAGE_EXECUTE_WRITECOPY);
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

void LeaveNativeLinearSpan(CONTEXT* win32_context,
                           ThreadContext* context,
                           bool reached_boundary,
                           bool write_fault_cancel,
                           std::uint32_t exception_code)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    if (!state->linear_span_active)
    {
        return;
    }
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
            static_cast<std::uint32_t>(win32_context->Eip),
            std::memory_order_relaxed);
    }
}

bool TryEnterNativeLinearSpan(CONTEXT* win32_context,
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

}  // namespace detail

}  // namespace repiu::platform::win32
