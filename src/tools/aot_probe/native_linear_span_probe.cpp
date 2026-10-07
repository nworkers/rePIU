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
    const std::uint8_t forward_jump_bytes[] = {
        0x90,                    // nop
        0xEB, 0x05,              // jmp forward target
        0x90, 0x90, 0x90, 0x90, 0x90,
        0x90,                    // target: nop
        0x40,                    // inc eax
        0x75, 0x00};             // jne next
    std::memcpy(memory, control_bytes, sizeof(control_bytes));
    std::memcpy(memory + 16, sensitive_bytes, sizeof(sensitive_bytes));
    std::memcpy(memory + 32, write_bytes, sizeof(write_bytes));
    std::memcpy(memory + 48, short_bytes, sizeof(short_bytes));
    std::memcpy(memory + 64, write_cross_bytes,
                sizeof(write_cross_bytes));
    std::memcpy(memory + 128U, forward_jump_bytes,
                sizeof(forward_jump_bytes));

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
    engine::detail::NativeLinearSpan unguarded_write;
    engine::detail::NativeLinearSpan forward_jump_disabled;
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
    // Task i022 deleted the opt-in write crossing and forward-jump chaining;
    // a span still stops before an explicit memory write and a jump.
    const bool unguarded_write_stopped =
        engine::detail::ScanNativeLinearSpanWithZydis(
            base + 64U, base, kPageSize, &unguarded_write) &&
        unguarded_write.boundary_address == base + 66U &&
        unguarded_write.instruction_count == 2U &&
        unguarded_write.boundary_memory_write;
    const bool forward_jump_disabled_ok =
        !engine::detail::ScanNativeLinearSpanWithZydis(
            base + 128U, base, kPageSize, &forward_jump_disabled);
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
    // Task i022. This used to go through the retired-trap wrapper, which was
    // deleted with its switch; it stays as the end-to-end check of span entry
    // and leave on a ThreadContext.
    auto span_context = std::make_unique<engine::ThreadContext>();
    span_context->runtime_base = base;
    span_context->runtime_size = kPageSize;
    span_context->execution_backend = runtime::ExecutionBackend::kDynamic;
    span_context->aot_reentry_pending = true;
    span_context->enable_single_step_trace = true;
    CONTEXT span_registers{};
    span_registers.Eip = base;
    span_registers.EFlags = 0x00000100U;
    const bool span_entered =
        engine::TryEnterNativeLinearSpan(
            &span_registers, span_context.get()) &&
        span_context->native_fast_path.linear_span_active &&
        span_context->aot_reentry_pending &&
        span_context->enable_single_step_trace &&
        (span_registers.EFlags & 0x00000100U) == 0U;
    span_registers.Eip =
        span_context->native_fast_path.linear_span_boundary;
    // Task 503d-9 added the fault kind. It only decides which cancel counter
    // moves, and this call reaches the boundary rather than cancelling, so it
    // names the kind a #DB at the boundary arrives as and nothing reads it.
    engine::LeaveNativeLinearSpan(
        &span_registers, span_context.get(), true, false,
        repiu::platform::FaultKind::kSingleStep, 0U);
    const bool span_left =
        !span_context->native_fast_path.linear_span_active &&
        (span_registers.EFlags & 0x00000100U) != 0U &&
        span_context->native_fast_path.linear_span_boundary_count.load(
            std::memory_order_relaxed) == 1U;
    auto span_reject_context = std::make_unique<engine::ThreadContext>();
    span_reject_context->runtime_base = base;
    span_reject_context->runtime_size = kPageSize;
    span_reject_context->execution_backend =
        runtime::ExecutionBackend::kDynamic;
    CONTEXT span_reject_registers{};
    span_reject_registers.Eip = base + 48U;
    span_reject_registers.EFlags = 0x00000100U;
    const bool span_rejected =
        !engine::TryEnterNativeLinearSpan(
            &span_reject_registers, span_reject_context.get()) &&
        !span_reject_context->native_fast_path.linear_span_active &&
        span_reject_context->native_fast_path.linear_span_reject_count.load(
            std::memory_order_relaxed) == 1U;
    const bool span_entry_behavior_ok =
        span_entered && span_left && span_rejected;
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
        engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kDynamic) &&
        !engine::ResolveNativeLinearSpanRejectCacheEnabled(
            runtime::ExecutionBackend::kLegacy);
    VirtualFree(memory, 0, MEM_RELEASE);

    const bool all =
        control_ok && sensitive_ok && write_ok && short_rejected &&
        unguarded_write_stopped && forward_jump_disabled_ok &&
        reject_cache_behavior_ok && reject_cache_capacity_ok &&
        span_entry_behavior_ok && policy_ok;
    std::cout << "linear_span_control_boundary="
              << (control_ok ? "true" : "false")
              << "\nlinear_span_sensitive_boundary="
              << (sensitive_ok ? "true" : "false")
              << "\nlinear_span_write_boundary="
              << (write_ok ? "true" : "false")
              << "\nlinear_span_short_rejected="
              << (short_rejected ? "true" : "false")
              << "\nlinear_span_unguarded_write_stopped="
              << (unguarded_write_stopped ? "true" : "false")
              << "\nlinear_span_forward_jump_disabled="
              << (forward_jump_disabled_ok ? "true" : "false")
              << "\nlinear_span_reject_cache_behavior="
              << (reject_cache_behavior_ok ? "true" : "false")
              << "\nlinear_span_reject_cache_capacity="
              << (reject_cache_capacity_ok ? "true" : "false")
              << "\nlinear_span_entry_behavior="
              << (span_entry_behavior_ok ? "true" : "false")
              << "\nlinear_span_policy="
              << (policy_ok ? "true" : "false")
              << "\nlinear_span_all=" << (all ? "true" : "false")
              << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
