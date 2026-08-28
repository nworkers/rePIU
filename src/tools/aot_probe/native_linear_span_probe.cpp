#include "native_linear_span_probe.h"

#include "native_fast_path.h"
#include "native_linear_span.h"
#include "execution/thread_context.h"
#include "verified_region_analyzer.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::tools
{
namespace
{

bool MatchGuardedProbePage(void* context, std::uint32_t guest_page)
{
    return context != nullptr &&
        *static_cast<const std::uint32_t*>(context) == guest_page;
}

bool ReadProbeRegister(
    void*, std::uint32_t, std::uint32_t* value)
{
    if (value == nullptr)
    {
        return false;
    }
    *value = 0x1000U;
    return true;
}

bool AllowProbeWriteTarget(void*, std::uint32_t, std::uint32_t)
{
    return true;
}

bool RejectProbeWriteTarget(void*, std::uint32_t, std::uint32_t)
{
    return false;
}

bool AllowProbeDirectJumpTarget(void*, std::uint32_t)
{
    return true;
}

bool RejectProbeDirectJumpTarget(void*, std::uint32_t)
{
    return false;
}

}  // namespace

bool RunNativeLinearSpanProbe()
{
#if !defined(_WIN32)
    return true;
#else
    constexpr std::uint32_t kPageSize = 4096;
    auto* memory = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,
        kPageSize * 2U,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE));
    if (memory == nullptr)
    {
        std::cout << "linear_span_all=false\n";
        return false;
    }
    std::memset(memory, 0x90, kPageSize * 2U);

    const std::uint8_t control_bytes[] = {
        0x8B, 0xC1,              // mov eax, ecx
        0x83, 0xC0, 0x01,        // add eax, 1
        0x75, 0x00};             // jne next
    const std::uint8_t sensitive_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0x64, 0xA1, 0, 0, 0, 0  // mov eax, fs:[0]
    };
    const std::uint8_t write_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0x89, 0x01};             // mov [ecx], eax
    const std::uint8_t short_bytes[] = {
        0x90,                    // nop
        0x75, 0x00};             // jne next
    const std::uint8_t write_cross_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0x89, 0x01,              // mov [ecx], eax
        0x83, 0xC2, 0x01,        // add edx, 1
        0x75, 0x00};             // jne next
    const std::uint8_t cross_page_write_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0x89, 0x01,              // mov [ecx], eax
        0x90};                   // next page nop
    const std::uint8_t entry_write_bytes[] = {
        0x89, 0x01,              // mov [ecx], eax
        0x90,                    // nop
        0x75, 0x00};             // jne next
    const std::uint8_t modified_address_write_bytes[] = {
        0xBA, 0x00, 0x10, 0x00, 0x00,  // mov edx, 0x1000
        0x90,                          // nop
        0x89, 0x02,                    // mov [edx], eax
        0x75, 0x00};                   // jne next
    const std::uint8_t rejected_target_write_bytes[] = {
        0x90,                    // nop
        0x89, 0x01,              // mov [ecx], eax
        0x75, 0x00};             // jne next
    const std::uint8_t forward_jump_bytes[] = {
        0x90,                    // nop
        0xEB, 0x05,              // jmp forward target
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90,                    // target: nop
        0x40,                    // inc eax
        0x75, 0x00};             // jne next
    const std::uint8_t backward_jump_bytes[] = {
        0x90,                    // target: nop
        0x40,                    // inc eax
        0xEB, 0xFC};             // jmp target
    const std::uint8_t rejected_jump_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0xEB, 0x04,              // jmp rejected target
        0x90, 0x90, 0x90, 0x90,
        0x90,                    // target: nop
        0x40,                    // inc eax
        0x75, 0x00};             // jne next
    std::memcpy(memory, control_bytes, sizeof(control_bytes));
    std::memcpy(memory + 16, sensitive_bytes, sizeof(sensitive_bytes));
    std::memcpy(memory + 32, write_bytes, sizeof(write_bytes));
    std::memcpy(memory + 48, short_bytes, sizeof(short_bytes));
    std::memcpy(memory + 64, write_cross_bytes,
                sizeof(write_cross_bytes));
    std::memcpy(memory + kPageSize - 4U, cross_page_write_bytes,
                sizeof(cross_page_write_bytes));
    std::memcpy(memory + 80U, entry_write_bytes,
                sizeof(entry_write_bytes));
    std::memcpy(memory + 96U, modified_address_write_bytes,
                sizeof(modified_address_write_bytes));
    std::memcpy(memory + 112U, rejected_target_write_bytes,
                sizeof(rejected_target_write_bytes));
    std::memcpy(memory + 128U, forward_jump_bytes,
                sizeof(forward_jump_bytes));
    std::memcpy(memory + 160U, backward_jump_bytes,
                sizeof(backward_jump_bytes));
    std::memcpy(memory + 176U, rejected_jump_bytes,
                sizeof(rejected_jump_bytes));

    DWORD old_protection = 0;
    const bool protected_rx =
        VirtualProtect(memory, kPageSize * 2U, PAGE_EXECUTE_READ,
                       &old_protection) != FALSE;
    const std::uint32_t base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(memory));
    engine::detail::NativeLinearSpan control;
    engine::detail::NativeLinearSpan sensitive;
    engine::detail::NativeLinearSpan write;
    engine::detail::NativeLinearSpan short_span;
    engine::detail::NativeLinearSpan guarded_write;
    engine::detail::NativeLinearSpan unguarded_write;
    engine::detail::NativeLinearSpan uncovered_page;
    engine::detail::NativeLinearSpan entry_write;
    engine::detail::NativeLinearSpan modified_address_write;
    engine::detail::NativeLinearSpan rejected_target_write;
    engine::detail::NativeLinearSpan forward_jump;
    engine::detail::NativeLinearSpan forward_jump_disabled;
    engine::detail::NativeLinearSpan backward_jump;
    engine::detail::NativeLinearSpan rejected_jump;
    const bool control_ok = protected_rx &&
        engine::detail::ScanNativeLinearSpanWithZydis(
            base, base, kPageSize, &control) &&
        control.boundary_address == base + 5U &&
        control.instruction_count == 2U &&
        !control.boundary_sensitive &&
        !control.boundary_memory_write;
    const bool sensitive_ok =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 16U, base, kPageSize, &sensitive) &&
        sensitive.boundary_address == base + 18U &&
        sensitive.instruction_count == 2U &&
        sensitive.boundary_sensitive;
    const bool write_ok =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 32U, base, kPageSize, &write) &&
        write.boundary_address == base + 34U &&
        write.instruction_count == 2U &&
        write.boundary_memory_write;
    const bool short_rejected =
        !engine::detail::ScanNativeLinearSpanWithZydis(
            base + 48U, base, kPageSize, &short_span) &&
        short_span.cacheable_rejection_byte_count ==
            sizeof(short_bytes);
    std::uint32_t guarded_page = base & 0xFFFFF000U;
    engine::detail::NativeLinearSpanOptions write_options;
    write_options.allow_memory_writes = true;
    write_options.write_guard_query = &MatchGuardedProbePage;
    write_options.register_query = &ReadProbeRegister;
    write_options.write_target_query = &AllowProbeWriteTarget;
    write_options.write_guard_context = &guarded_page;
    engine::detail::NativeLinearSpanOptions
        rejected_target_options = write_options;
    rejected_target_options.write_target_query = &RejectProbeWriteTarget;
    engine::detail::NativeLinearSpanOptions jump_options;
    jump_options.chain_forward_direct_jumps = true;
    jump_options.direct_jump_target_query =
        &AllowProbeDirectJumpTarget;
    jump_options.write_guard_context = &guarded_page;
    engine::detail::NativeLinearSpanOptions
        rejected_jump_options = jump_options;
    rejected_jump_options.direct_jump_target_query =
        &RejectProbeDirectJumpTarget;
    const bool guarded_write_crossed =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 64U, base, kPageSize, &guarded_write,
            &write_options) &&
        guarded_write.boundary_address == base + 71U &&
        guarded_write.instruction_count == 4U &&
        guarded_write.crossed_memory_write_count == 1U &&
        !guarded_write.boundary_memory_write &&
        !guarded_write.boundary_write_guard_uncovered;
    const bool unguarded_write_stopped =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 64U, base, kPageSize, &unguarded_write) &&
        unguarded_write.boundary_address == base + 66U &&
        unguarded_write.instruction_count == 2U &&
        unguarded_write.crossed_memory_write_count == 0U &&
        unguarded_write.boundary_memory_write;
    const bool uncovered_page_stopped =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + kPageSize - 4U, base, kPageSize * 2U,
            &uncovered_page, &write_options) &&
        uncovered_page.boundary_address == base + kPageSize &&
        uncovered_page.instruction_count == 3U &&
        uncovered_page.crossed_memory_write_count == 1U &&
        uncovered_page.boundary_write_guard_uncovered;
    const bool entry_write_stopped =
        !engine::detail::ScanNativeLinearSpanWithZydis(
            base + 80U, base, kPageSize, &entry_write,
            &write_options) &&
        entry_write.boundary_address == base + 80U &&
        entry_write.instruction_count == 0U &&
        entry_write.boundary_memory_write;
    const bool modified_address_write_stopped =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 96U, base, kPageSize, &modified_address_write,
            &write_options) &&
        modified_address_write.boundary_address == base + 102U &&
        modified_address_write.instruction_count == 2U &&
        modified_address_write.crossed_memory_write_count == 0U &&
        modified_address_write.boundary_memory_write;
    const bool rejected_target_write_stopped =
        !engine::detail::ScanNativeLinearSpanWithZydis(
            base + 112U, base, kPageSize, &rejected_target_write,
            &rejected_target_options) &&
        rejected_target_write.boundary_address == base + 113U &&
        rejected_target_write.instruction_count == 1U &&
        rejected_target_write.crossed_memory_write_count == 0U &&
        rejected_target_write.boundary_memory_write;
    const bool forward_jump_chained =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 128U, base, kPageSize, &forward_jump,
            &jump_options) &&
        forward_jump.boundary_address == base + 138U &&
        forward_jump.instruction_count == 4U &&
        forward_jump.chained_direct_jump_count == 1U;
    const bool forward_jump_disabled_ok =
        !engine::detail::ScanNativeLinearSpanWithZydis(
            base + 128U, base, kPageSize, &forward_jump_disabled);
    const bool backward_jump_stopped =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 160U, base, kPageSize, &backward_jump,
            &jump_options) &&
        backward_jump.boundary_address == base + 162U &&
        backward_jump.instruction_count == 2U &&
        backward_jump.chained_direct_jump_count == 0U &&
        backward_jump.boundary_backward_jump;
    const bool rejected_jump_stopped =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 176U, base, kPageSize, &rejected_jump,
            &rejected_jump_options) &&
        rejected_jump.boundary_address == base + 178U &&
        rejected_jump.instruction_count == 2U &&
        rejected_jump.chained_direct_jump_count == 0U;
    engine::detail::NativeFastPathState cache_state;
    engine::detail::NativeLinearSpan cached_span;
    const bool cache_initial_miss =
        !engine::detail::LookupNativeLinearSpanScanCache(
            &cache_state, base, base, 1U, &cached_span);
    engine::detail::StoreNativeLinearSpanScanCache(
        &cache_state, base, base, 1U, control);
    const bool cache_same_generation_hit =
        engine::detail::LookupNativeLinearSpanScanCache(
            &cache_state, base, base, 1U, &cached_span) &&
        cached_span.boundary_address == control.boundary_address &&
        cached_span.instruction_count == control.instruction_count;
    const bool cache_new_generation_miss =
        !engine::detail::LookupNativeLinearSpanScanCache(
            &cache_state, base, base, 2U, &cached_span);
    const bool cache_generation_ok =
        cache_initial_miss && cache_same_generation_hit &&
        cache_new_generation_miss &&
        cache_state.linear_span_scan_cache.empty() &&
        cache_state.linear_span_cache_hit_count.load(
            std::memory_order_relaxed) == 1U &&
        cache_state.linear_span_cache_miss_count.load(
            std::memory_order_relaxed) == 2U;
    engine::detail::NativeFastPathState reject_cache_state;
    const bool reject_cache_initial_miss =
        !engine::detail::LookupNativeLinearSpanRejectCache(
            &reject_cache_state, base + 48U);
    engine::detail::StoreNativeLinearSpanRejectCache(
        &reject_cache_state, base + 48U, short_span);
    const bool reject_cache_same_bytes_hit =
        engine::detail::LookupNativeLinearSpanRejectCache(
            &reject_cache_state, base + 48U);
    DWORD reject_cache_old_protection = 0;
    const bool reject_cache_write_enabled = VirtualProtect(
        memory, kPageSize, PAGE_EXECUTE_READWRITE,
        &reject_cache_old_protection) != FALSE;
    if (reject_cache_write_enabled)
    {
        memory[48U] = 0x40;
    }
    const bool reject_cache_changed_bytes_stale =
        reject_cache_write_enabled &&
        !engine::detail::LookupNativeLinearSpanRejectCache(
            &reject_cache_state, base + 48U) &&
        reject_cache_state.linear_span_reject_cache.empty();
    if (reject_cache_write_enabled)
    {
        memory[48U] = short_bytes[0];
        DWORD ignored_protection = 0;
        VirtualProtect(memory, kPageSize, reject_cache_old_protection,
                       &ignored_protection);
    }
    const bool reject_cache_behavior_ok =
        reject_cache_initial_miss && reject_cache_same_bytes_hit &&
        reject_cache_changed_bytes_stale &&
        reject_cache_state.linear_span_reject_cache_hit_count.load(
            std::memory_order_relaxed) == 1U &&
        reject_cache_state.linear_span_reject_cache_miss_count.load(
            std::memory_order_relaxed) == 1U &&
        reject_cache_state.linear_span_reject_cache_stale_count.load(
            std::memory_order_relaxed) == 1U &&
        reject_cache_state.linear_span_reject_cache_store_count.load(
            std::memory_order_relaxed) == 1U;
    engine::detail::NativeFastPathState capacity_state;
    capacity_state.linear_span_reject_cache.reserve(
        engine::detail::kNativeLinearSpanRejectCacheMaxEntries);
    for (std::uint32_t index = 0;
         index <
             engine::detail::kNativeLinearSpanRejectCacheMaxEntries;
         ++index)
    {
        capacity_state.linear_span_reject_cache.emplace(
            index,
            engine::detail::NativeLinearSpanRejectCacheEntry{});
    }
    engine::detail::StoreNativeLinearSpanRejectCache(
        &capacity_state, base + 48U, short_span);
    const bool reject_cache_capacity_ok =
        capacity_state.linear_span_reject_cache.size() ==
            engine::detail::
                kNativeLinearSpanRejectCacheMaxEntries &&
        capacity_state.linear_span_reject_cache.find(base + 48U) ==
            capacity_state.linear_span_reject_cache.end() &&
        capacity_state.linear_span_reject_cache_capacity_skip_count.load(
            std::memory_order_relaxed) == 1U;
    auto retired_span_context =
        std::make_unique<engine::ThreadContext>();
    retired_span_context->runtime_base = base;
    retired_span_context->runtime_size = kPageSize;
    retired_span_context->execution_backend =
        runtime::ExecutionBackend::kDynamic;
    retired_span_context->aot_reentry_pending = true;
    retired_span_context->enable_single_step_trace = true;
    CONTEXT retired_span_registers{};
    retired_span_registers.Eip = base;
    retired_span_registers.EFlags = 0x00000100U;
    const bool retired_span_entered =
        engine::TryEnterRetiredTrapNativeSpan(
            &retired_span_registers, retired_span_context.get()) &&
        retired_span_context->native_fast_path.linear_span_active &&
        retired_span_context->aot_reentry_pending &&
        retired_span_context->enable_single_step_trace &&
        (retired_span_registers.EFlags & 0x00000100U) == 0U;
    retired_span_registers.Eip =
        retired_span_context->native_fast_path.linear_span_boundary;
    // Task 503d-9 added the fault kind. It only decides which cancel counter
    // moves, and this call reaches the boundary rather than cancelling, so it
    // names the kind a #DB at the boundary arrives as and nothing reads it.
    engine::LeaveNativeLinearSpan(
        &retired_span_registers, retired_span_context.get(), true, false,
        repiu::platform::FaultKind::kSingleStep, 0U);
    auto retired_reject_context =
        std::make_unique<engine::ThreadContext>();
    retired_reject_context->runtime_base = base;
    retired_reject_context->runtime_size = kPageSize;
    retired_reject_context->execution_backend =
        runtime::ExecutionBackend::kDynamic;
    CONTEXT retired_reject_registers{};
    retired_reject_registers.Eip = base + 48U;
    retired_reject_registers.EFlags = 0x00000100U;
    const bool retired_span_rejected =
        !engine::TryEnterRetiredTrapNativeSpan(
            &retired_reject_registers, retired_reject_context.get()) &&
        !retired_reject_context->native_fast_path.linear_span_active;
    const bool retired_span_behavior_ok =
        retired_span_entered && retired_span_rejected &&
        retired_span_context->aot_retired_span_attempt_count.load(
            std::memory_order_relaxed) == 1U &&
        retired_span_context->aot_retired_span_success_count.load(
            std::memory_order_relaxed) == 1U &&
        retired_reject_context->aot_retired_span_attempt_count.load(
            std::memory_order_relaxed) == 1U &&
        retired_reject_context->aot_retired_span_success_count.load(
            std::memory_order_relaxed) == 0U;
    const bool policy_ok =
        engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "") &&
        // Task 425: legacy is the counterexample backend. The property that a
        // non-dynamic backend is OFF when unset and ON when set explicitly is
        // unchanged, and all three affirmative spellings are checked on legacy.
        !engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kLegacy, "") &&
        engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kLegacy, "1") &&
        engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kLegacy, "on") &&
        engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kLegacy, "true") &&
        !engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "0") &&
        !engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "off") &&
        !engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "false") &&
        !engine::ResolveNativeLinearSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "invalid") &&
        !engine::ResolveNativeLinearSpanCacheEnabled("") &&
        engine::ResolveNativeLinearSpanCacheEnabled("1") &&
        engine::ResolveNativeLinearSpanCacheEnabled("on") &&
        engine::ResolveNativeLinearSpanCacheEnabled("true") &&
        !engine::ResolveNativeLinearSpanCacheEnabled("0") &&
        !engine::ResolveNativeLinearSpanCacheEnabled("invalid") &&
        engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kDynamic, "") &&
        !engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kLegacy, "") &&
        engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kDynamic, "1") &&
        engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kDynamic, "on") &&
        engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kLegacy, "true") &&
        !engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kDynamic, "0") &&
        !engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kDynamic, "invalid") &&
        !engine::ResolveRetiredTrapNativeSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "") &&
        engine::ResolveRetiredTrapNativeSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "1") &&
        engine::ResolveRetiredTrapNativeSpanEnabled(
            runtime::ExecutionBackend::kLegacy, "on") &&
        !engine::ResolveRetiredTrapNativeSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "0") &&
        !engine::ResolveRetiredTrapNativeSpanEnabled(
            runtime::ExecutionBackend::kDynamic, "invalid") &&
        !engine::ResolveNativeLinearSpanWritesEnabled("") &&
        engine::ResolveNativeLinearSpanWritesEnabled("1") &&
        engine::ResolveNativeLinearSpanWritesEnabled("on") &&
        engine::ResolveNativeLinearSpanWritesEnabled("true") &&
        !engine::ResolveNativeLinearSpanWritesEnabled("0") &&
        !engine::ResolveNativeLinearSpanWritesEnabled("invalid") &&
        !engine::ResolveNativeLinearSpanJumpsEnabled("") &&
        engine::ResolveNativeLinearSpanJumpsEnabled("1") &&
        engine::ResolveNativeLinearSpanJumpsEnabled("on") &&
        engine::ResolveNativeLinearSpanJumpsEnabled("true") &&
        !engine::ResolveNativeLinearSpanJumpsEnabled("0") &&
        !engine::ResolveNativeLinearSpanJumpsEnabled("invalid");
    VirtualFree(memory, 0, MEM_RELEASE);

    const bool all =
        control_ok && sensitive_ok && write_ok && short_rejected &&
        guarded_write_crossed && unguarded_write_stopped &&
        uncovered_page_stopped && entry_write_stopped &&
        modified_address_write_stopped && rejected_target_write_stopped &&
        forward_jump_chained && forward_jump_disabled_ok &&
        backward_jump_stopped && rejected_jump_stopped &&
        cache_generation_ok && reject_cache_behavior_ok &&
        reject_cache_capacity_ok && retired_span_behavior_ok && policy_ok;
    std::cout << "linear_span_control_boundary="
              << (control_ok ? "true" : "false")
              << "\nlinear_span_sensitive_boundary="
              << (sensitive_ok ? "true" : "false")
              << "\nlinear_span_write_boundary="
              << (write_ok ? "true" : "false")
              << "\nlinear_span_short_rejected="
              << (short_rejected ? "true" : "false")
              << "\nlinear_span_guarded_write_crossed="
              << (guarded_write_crossed ? "true" : "false")
              << "\nlinear_span_unguarded_write_stopped="
              << (unguarded_write_stopped ? "true" : "false")
              << "\nlinear_span_uncovered_page_stopped="
              << (uncovered_page_stopped ? "true" : "false")
              << "\nlinear_span_entry_write_stopped="
              << (entry_write_stopped ? "true" : "false")
              << "\nlinear_span_modified_address_write_stopped="
              << (modified_address_write_stopped ? "true" : "false")
              << "\nlinear_span_rejected_target_write_stopped="
              << (rejected_target_write_stopped ? "true" : "false")
              << "\nlinear_span_forward_jump_chained="
              << (forward_jump_chained ? "true" : "false")
              << "\nlinear_span_forward_jump_disabled="
              << (forward_jump_disabled_ok ? "true" : "false")
              << "\nlinear_span_backward_jump_stopped="
              << (backward_jump_stopped ? "true" : "false")
              << "\nlinear_span_rejected_jump_stopped="
              << (rejected_jump_stopped ? "true" : "false")
              << "\nlinear_span_cache_generation="
              << (cache_generation_ok ? "true" : "false")
              << "\nlinear_span_reject_cache_behavior="
              << (reject_cache_behavior_ok ? "true" : "false")
              << "\nlinear_span_reject_cache_capacity="
              << (reject_cache_capacity_ok ? "true" : "false")
              << "\nlinear_span_retired_reentry="
              << (retired_span_behavior_ok ? "true" : "false")
              << "\nlinear_span_policy="
              << (policy_ok ? "true" : "false")
              << "\nlinear_span_all=" << (all ? "true" : "false")
              << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
