#include "dbt_call_step_probe.h"

#include <iostream>

#if defined(_WIN32)
#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "../../platform/win32/aot/aot_dbt_call_step_probe.h"
#include "../../platform/win32/execution/thread_context.h"

#include <cstdint>
#include <memory>
#endif

namespace repiu::tools
{

bool RunAotDbtCallStepProbe()
{
#if !defined(_WIN32)
    std::cout << "dbt_call_step_probe_skipped=true\n";
    return true;
#else
    using platform::win32::ConfigureAotDbtCallStepProbe;
    using platform::win32::HandleAotDbtCallStepProbe;
    using platform::win32::MaybeArmAotDbtCallStepProbe;
    using platform::win32::ThreadContext;
    using platform::win32::Win32AotCallStepProbeEventKind;
    using platform::win32::Win32AotCallStepProbePhase;
    using platform::win32::Win32AotTransferOrigin;

    auto context = std::make_unique<ThreadContext>();
    context->aot_dbt_call_return_trace_configured = true;
    ConfigureAotDbtCallStepProbe(context.get(), "1,4");
    const bool configured =
        context->aot_dbt_call_step_probe_configured &&
        context->aot_dbt_call_step_probe_target_count == 2U &&
        context->aot_dbt_call_step_probe_targets[0] == 1U &&
        context->aot_dbt_call_step_probe_targets[1] == 4U;

    runtime::AotDbtIndirectDispatchSite site;
    site.guest_source = 0x1000U;
    site.success_cache_offset = 0x200U;
    site.is_call = true;
    platform::win32::Win32AotCodeCachePlacement placement;
    placement.placed = true;
    placement.base_address = 0x500000U;
    runtime::AotAddressMapEntry return_map;
    return_map.guest_address = 0x1002U;
    return_map.cache_offset = 0x300U;
    return_map.emitted_length = 1U;
    placement.address_map.push_back(return_map);
    context->aot_placement = &placement;

    std::uint32_t stack[8] = {};
    const std::uint32_t stack_base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(stack));
    const std::uint32_t entry_esp = stack_base + 8U;
    stack[0] = 0x500400U;
    stack[1] = 0x1002U;
    context->runtime_base = stack_base;
    context->runtime_size = sizeof(stack);

    std::uint32_t saved_eflags = 0x202U;
    const bool disabled_origin =
        !MaybeArmAotDbtCallStepProbe(
            context.get(), Win32AotTransferOrigin::kVeh, site, 1U,
            0x2000U, 0x500400U, 0x1002U, entry_esp, &saved_eflags);
    const bool armed = MaybeArmAotDbtCallStepProbe(
        context.get(), Win32AotTransferOrigin::kHost, site, 1U,
        0x2000U, 0x500400U, 0x1002U, entry_esp, &saved_eflags);

    EXCEPTION_RECORD record{};
    record.ExceptionCode = EXCEPTION_SINGLE_STEP;
    CONTEXT registers{};
    EXCEPTION_POINTERS pointers{&record, &registers};

    registers.Eip = placement.base_address + site.success_cache_offset;
    registers.Esp = entry_esp - 8U;
    registers.EFlags = saved_eflags;
    const bool pre = HandleAotDbtCallStepProbe(
        &pointers, &registers, context.get());
    registers.Eip = 0x500400U;
    registers.Esp = entry_esp - 4U;
    registers.EFlags |= 0x100U;
    const bool post = HandleAotDbtCallStepProbe(
        &pointers, &registers, context.get());
    const bool watch =
        context->aot_dbt_call_step_probe_phase ==
            Win32AotCallStepProbePhase::kAwaitReturnTarget &&
        registers.Dr0 == 0x500300U &&
        registers.Dr1 == 0x1002U &&
        (registers.Dr7 & 0x5U) == 0x5U;
    registers.Eip = 0x500300U;
    registers.Esp = entry_esp;
    registers.Dr6 = 0x1U;
    const bool returned = HandleAotDbtCallStepProbe(
        &pointers, &registers, context.get());
    const bool completed =
        context->aot_dbt_call_step_probe_phase ==
            Win32AotCallStepProbePhase::kIdle &&
        context->aot_dbt_call_step_probe_complete_count == 1U &&
        context->aot_dbt_call_step_probe_trace_count == 3U &&
        context->aot_dbt_call_step_probe_trace[0].kind ==
            Win32AotCallStepProbeEventKind::kPreC3 &&
        context->aot_dbt_call_step_probe_trace[0].eip_matches &&
        context->aot_dbt_call_step_probe_trace[0].esp_matches &&
        context->aot_dbt_call_step_probe_trace[1].kind ==
            Win32AotCallStepProbeEventKind::kPostC3 &&
        context->aot_dbt_call_step_probe_trace[1].eip_matches &&
        context->aot_dbt_call_step_probe_trace[1].esp_matches &&
        context->aot_dbt_call_step_probe_trace[2].kind ==
            Win32AotCallStepProbeEventKind::kReturnTarget &&
        context->aot_dbt_call_step_probe_trace[2].esp_matches &&
        registers.Dr0 == 0U && registers.Dr1 == 0U &&
        registers.Dr7 == 0U && (registers.EFlags & 0x100U) == 0U;

    auto conflict_context = std::make_unique<ThreadContext>();
    conflict_context->aot_dbt_call_return_trace_configured = true;
    ConfigureAotDbtCallStepProbe(conflict_context.get(), "1");
    conflict_context->aot_placement = &placement;
    conflict_context->runtime_base = stack_base;
    conflict_context->runtime_size = sizeof(stack);
    saved_eflags = 0x202U;
    const bool conflict_armed = MaybeArmAotDbtCallStepProbe(
        conflict_context.get(), Win32AotTransferOrigin::kHost, site, 1U,
        0x2000U, 0x500400U, 0x1002U, entry_esp, &saved_eflags);
    registers = {};
    registers.Eip = placement.base_address + site.success_cache_offset;
    registers.Esp = entry_esp - 8U;
    registers.EFlags = saved_eflags;
    const bool conflict_pre = HandleAotDbtCallStepProbe(
        &pointers, &registers, conflict_context.get());
    registers.Eip = 0x500400U;
    registers.Esp = entry_esp - 4U;
    registers.EFlags |= 0x100U;
    registers.Dr7 = 0x1U;
    const bool conflict_post = HandleAotDbtCallStepProbe(
        &pointers, &registers, conflict_context.get());
    const bool conflict =
        conflict_context->aot_dbt_call_step_probe_conflict_count == 1U &&
        conflict_context->aot_dbt_call_step_probe_phase ==
            Win32AotCallStepProbePhase::kIdle &&
        registers.Dr7 == 0x1U;

    auto disabled_context = std::make_unique<ThreadContext>();
    ConfigureAotDbtCallStepProbe(disabled_context.get(), "1");
    const bool trace_required =
        !disabled_context->aot_dbt_call_step_probe_configured;

    const bool passed =
        configured && disabled_origin && armed && pre && post && watch &&
        returned && completed && conflict_armed && conflict_pre &&
        conflict_post && conflict && trace_required;
    std::cout << "dbt_call_step_probe="
              << (passed ? "true" : "false") << "\n";
    return passed;
#endif
}

}  // namespace repiu::tools
