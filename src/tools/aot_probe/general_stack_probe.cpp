#include "general_stack_probe.h"

#include "execution_internal.h"
#include "instruction_emulation.h"
#include "repiu/platform/linux_x64_aot_dispatch.h"
#include "repiu/platform/virtual_memory.h"
#include "repiu/runtime/selector_table.h"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace repiu::tools
{

bool RunGeneralStackProbe()
{
    constexpr std::uintptr_t kRequestedBase = 0x18000000U;
    constexpr std::size_t kArenaSize = 0x1000U;
    constexpr std::uint32_t kCodeOffset = 0x100U;
    constexpr std::uint32_t kStackOffset = 0x900U;
    void* const requested = reinterpret_cast<void*>(kRequestedBase);
    const repiu::platform::MemoryReservation reservation =
        repiu::platform::ReserveMemory(
            requested,
            kArenaSize,
            true,
            repiu::platform::MemoryProtection::kExecuteReadWrite);
    if (!reservation.valid || reservation.base != requested)
    {
        if (reservation.valid && reservation.base != nullptr)
        {
            repiu::platform::ReleaseMemory(reservation.base, kArenaSize);
        }
        std::cout << "general_stack_arena=false\n";
        return false;
    }

    auto* const bytes = static_cast<std::uint8_t*>(reservation.base);
    auto set_opcode = [&](std::uint8_t opcode) {
        bytes[kCodeOffset] = opcode;
    };
    repiu::engine::ThreadContext context;
    context.runtime_base = static_cast<std::uint32_t>(kRequestedBase);
    context.runtime_size = static_cast<std::uint32_t>(kArenaSize);
    repiu::platform::GuestCpuContext cpu{};
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebx = 0x11223344U;

    const std::uint32_t cs_store_eip = cpu.Eip;
    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    const bool cs_descriptor_registered =
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            {0x24U, static_cast<std::uint32_t>(kRequestedBase),
             static_cast<std::uint32_t>(kArenaSize - 1U), 0U, true,
             repiu::runtime::kLeObjectExecutable, true,
             repiu::runtime::GuestCodeDefaultOperandSize::k32});
    const std::uint8_t cs_store[] = {0x8CU, 0xC8U};
    std::memcpy(bytes + kCodeOffset, cs_store, sizeof(cs_store));
    cpu.Eax = 0xABCD0000U;
    const bool cs_store_handled =
        repiu::engine::HandleSegmentStoreInstruction(&cpu, &context);
    const bool cs_source_store = cs_descriptor_registered && cs_store_handled &&
        cpu.Eax == 0xABCD0024U &&
        cpu.Eip == cs_store_eip + sizeof(cs_store);

    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    cpu.Eip = cs_store_eip;
    cpu.Eax = 0xABCD0000U;
    const bool cs_source_missing_refused =
        !repiu::engine::HandleSegmentStoreInstruction(&cpu, &context) &&
        cpu.Eax == 0xABCD0000U && cpu.Eip == cs_store_eip;

    const std::uint8_t enter_nonnested_bytes[] = {
        0xC8U, 0x04U, 0x00U, 0x00U};
    std::memcpy(bytes + kCodeOffset, enter_nonnested_bytes,
                sizeof(enter_nonnested_bytes));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebp = 0xCAFEBABEU;
    cpu.EFlags = 0x00000246U;
    const bool enter_nonnested_handled =
        repiu::engine::HandleEnterInstruction(&cpu, &context);
    std::uint32_t enter_saved_ebp = 0U;
    std::memcpy(&enter_saved_ebp, bytes + kStackOffset - 4U,
                sizeof(enter_saved_ebp));
    const bool enter_nonnested = enter_nonnested_handled &&
        enter_saved_ebp == 0xCAFEBABEU &&
        cpu.Ebp == kRequestedBase + kStackOffset - 4U &&
        cpu.Esp == kRequestedBase + kStackOffset - 8U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 4U &&
        cpu.EFlags == 0x00000246U;

    const std::uint32_t nested_old_ebp =
        static_cast<std::uint32_t>(kRequestedBase + 0x700U);
    const std::uint32_t nested_display_1 = 0xAABBCCDDU;
    const std::uint32_t nested_display_2 = 0x11223344U;
    std::memcpy(bytes + 0x6FCU, &nested_display_1,
                sizeof(nested_display_1));
    std::memcpy(bytes + 0x6F8U, &nested_display_2,
                sizeof(nested_display_2));
    const std::uint8_t enter_nested_bytes[] = {
        0xC8U, 0x0CU, 0x00U, 0x03U};
    std::memcpy(bytes + kCodeOffset, enter_nested_bytes,
                sizeof(enter_nested_bytes));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebp = nested_old_ebp;
    cpu.EFlags = 0x000002D7U;
    const bool enter_nested_handled =
        repiu::engine::HandleEnterInstruction(&cpu, &context);
    std::uint32_t nested_values[4] = {};
    std::memcpy(nested_values, bytes + kStackOffset - 16U,
                sizeof(nested_values));
    const bool enter_nested = enter_nested_handled &&
        nested_values[0] == kRequestedBase + kStackOffset - 4U &&
        nested_values[1] == nested_display_2 &&
        nested_values[2] == nested_display_1 &&
        nested_values[3] == nested_old_ebp &&
        cpu.Ebp == kRequestedBase + kStackOffset - 4U &&
        cpu.Esp == kRequestedBase + kStackOffset - 28U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 4U &&
        cpu.EFlags == 0x000002D7U;

    const std::uint8_t enter_rejected_bytes[] = {
        0xC8U, 0x00U, 0x00U, 0x00U};
    std::memcpy(bytes + kCodeOffset, enter_rejected_bytes,
                sizeof(enter_rejected_bytes));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + 2U);
    cpu.Ebp = 0x12345678U;
    cpu.EFlags = 0x00000246U;
    const bool enter_range_rejected =
        !repiu::engine::HandleEnterInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + kCodeOffset &&
        cpu.Esp == kRequestedBase + 2U &&
        cpu.Ebp == 0x12345678U &&
        cpu.EFlags == 0x00000246U;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebp = 0U;
    cpu.Ebx = 0x11223344U;
    set_opcode(0x53U);
    const bool push_ebx =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    std::uint32_t stored = 0U;
    std::memcpy(&stored, bytes + kStackOffset - 4U, sizeof(stored));
    const bool ordinary_push = push_ebx && stored == 0x11223344U &&
        cpu.Esp == kRequestedBase + kStackOffset - 4U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 1U;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    set_opcode(0x59U);
    const bool pop_ecx =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    const bool ordinary_pop = pop_ecx && cpu.Ecx == 0x11223344U &&
        cpu.Esp == kRequestedBase + kStackOffset &&
        cpu.Eip == kRequestedBase + kCodeOffset + 1U;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    set_opcode(0x54U);
    const bool push_esp =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    std::memcpy(&stored, bytes + kStackOffset - 4U, sizeof(stored));
    const bool push_esp_order = push_esp &&
        stored == kRequestedBase + kStackOffset &&
        cpu.Esp == kRequestedBase + kStackOffset - 4U;

    const std::uint32_t popped_esp =
        static_cast<std::uint32_t>(kRequestedBase + kStackOffset + 0x40U);
    std::memcpy(bytes + kStackOffset - 4U, &popped_esp, sizeof(popped_esp));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    set_opcode(0x5CU);
    const bool pop_esp =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    const bool pop_esp_order = pop_esp && cpu.Esp == popped_esp;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase);
    set_opcode(0x53U);
    const bool rejected =
        !repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context) &&
        cpu.Esp == kRequestedBase &&
        cpu.Eip == kRequestedBase + kCodeOffset;

    const std::uint8_t sequence[] = {
        0x53U, 0x06U, 0x0FU, 0xA0U, 0x83U, 0xECU, 0x04U, 0x90U};
    std::memcpy(bytes + kCodeOffset, sequence, sizeof(sequence));
    context.enable_segment_load_hle = true;
    context.guest_es = 0x0024U;
    context.guest_fs = 0x0034U;
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebx = 0x11223344U;
    const std::uint32_t sequence_count =
        repiu::engine::HandleConsecutiveLegacyStackInstructions(
            &cpu, &context, 16U);
    std::uint32_t sequence_values[3]{};
    std::memcpy(sequence_values, bytes + kStackOffset - 12U,
                sizeof(sequence_values));
    const bool mixed_sequence = sequence_count == 4U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 7U &&
        cpu.Esp == kRequestedBase + kStackOffset - 16U &&
        sequence_values[0] == 0x00000034U &&
        sequence_values[1] == 0x00000024U &&
        sequence_values[2] == 0x11223344U &&
        (cpu.EFlags & 0x00000001U) == 0U;

    const std::uint8_t bounded_sequence[] = {0x53U, 0x53U, 0x53U};
    std::memcpy(bytes + kCodeOffset, bounded_sequence,
                sizeof(bounded_sequence));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    const std::uint32_t bounded_count =
        repiu::engine::HandleConsecutiveLegacyStackInstructions(
            &cpu, &context, 2U);
    const bool bounded = bounded_count == 2U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 2U &&
        cpu.Esp == kRequestedBase + kStackOffset - 8U;

    bool legacy_direct_call = true;
    bool legacy_direct_call_range_rejected = true;
    bool legacy_resume_policy = true;
    bool legacy_resume_thunk = true;
    bool legacy_moffs_store = true;
    bool legacy_moffs_store_range_rejected = true;
#if defined(_M_X64) || defined(__x86_64__)
    constexpr std::uint32_t kCallTargetOffset = 0x180U;
    constexpr std::uint32_t kCallSize = 5U;
    bytes[kCodeOffset] = 0xE8U;
    const std::int32_t call_displacement =
        static_cast<std::int32_t>(kCallTargetOffset -
                                  (kCodeOffset + kCallSize));
    std::memcpy(bytes + kCodeOffset + 1U, &call_displacement,
                sizeof(call_displacement));
    context.aot_legacy_fallback = true;
    context.aot_call_depth = 0U;
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    const bool call_dispatched =
        repiu::engine::DispatchGuestHleInstruction(&cpu, &context);
    std::uint32_t call_return = 0U;
    std::memcpy(&call_return, bytes + kStackOffset - 4U,
                sizeof(call_return));
    legacy_direct_call = call_dispatched &&
        cpu.Eip == kRequestedBase + kCallTargetOffset &&
        cpu.Esp == kRequestedBase + kStackOffset - 4U &&
        call_return == kRequestedBase + kCodeOffset + kCallSize &&
        context.aot_call_depth == 1U &&
        context.aot_call_frames[0].source ==
            kRequestedBase + kCodeOffset &&
        context.aot_call_frames[0].target ==
            kRequestedBase + kCallTargetOffset &&
        context.aot_call_frames[0].fallthrough == call_return;

    const std::int32_t outside_displacement =
        static_cast<std::int32_t>(kArenaSize + 0x100U -
                                  (kCodeOffset + kCallSize));
    std::memcpy(bytes + kCodeOffset + 1U, &outside_displacement,
                sizeof(outside_displacement));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    legacy_direct_call_range_rejected =
        !repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + kCodeOffset &&
        cpu.Esp == kRequestedBase + kStackOffset;

    const std::uint8_t identical_instruction[] = {0x89U, 0xC2U};
    std::memcpy(bytes + kCodeOffset, identical_instruction,
                sizeof(identical_instruction));
    const bool identical_allowed =
        repiu::engine::CanResumeLinuxX64LegacyTarget(
            &context, kRequestedBase + kCodeOffset);
    bytes[kCodeOffset] = 0x53U;
    const bool stack_refused =
        !repiu::engine::CanResumeLinuxX64LegacyTarget(
            &context, kRequestedBase + kCodeOffset);
    legacy_resume_policy = identical_allowed && stack_refused;
    bool legacy_resume_mode_aware = true;
#if defined(__x86_64__)
    repiu::engine::AotCodeCachePlacement mode16_placement;
    mode16_placement.code_mode_ranges.push_back({
        static_cast<std::uint32_t>(kRequestedBase),
        static_cast<std::uint32_t>(kArenaSize),
        0U});
    context.aot_placement = &mode16_placement;
    const std::uint8_t mode16_test[] = {0x66U, 0x85U, 0xFFU};
    std::memcpy(bytes + kCodeOffset, mode16_test, sizeof(mode16_test));
    const bool mode16_range_refused =
        !repiu::engine::CanResumeLinuxX64LegacyTarget(
            &context, kRequestedBase + kCodeOffset);

    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    const bool mode16_selector_registered =
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            {0x24U, static_cast<std::uint32_t>(kRequestedBase),
             static_cast<std::uint32_t>(kArenaSize - 1U), 0U, true,
             repiu::runtime::kLeObjectExecutable, true,
             repiu::runtime::GuestCodeDefaultOperandSize::k16});
    context.aot_placement = nullptr;
    const bool mode16_selector_refused =
        !repiu::engine::CanResumeLinuxX64LegacyTarget(
            &context, kRequestedBase + kCodeOffset);
    legacy_resume_mode_aware = mode16_range_refused &&
        mode16_selector_registered && mode16_selector_refused;
    repiu::runtime::InitializeSelectorTable(&context.selector_table);
#endif
    context.aot_placement = nullptr;
#if defined(__x86_64__) && !defined(_WIN32)
    legacy_resume_thunk =
        repiu::platform::LinuxX64LegacyResumeThunkAddress() != 0U;
#endif

    constexpr std::uint32_t kMoffsDestinationOffset = 0x500U;
    const std::uint8_t moffs_store[] = {
        0xA3U,
        static_cast<std::uint8_t>(
            (kRequestedBase + kMoffsDestinationOffset) & 0xFFU),
        static_cast<std::uint8_t>(
            ((kRequestedBase + kMoffsDestinationOffset) >> 8U) & 0xFFU),
        static_cast<std::uint8_t>(
            ((kRequestedBase + kMoffsDestinationOffset) >> 16U) & 0xFFU),
        static_cast<std::uint8_t>(
            ((kRequestedBase + kMoffsDestinationOffset) >> 24U) & 0xFFU)};
    std::memcpy(bytes + kCodeOffset, moffs_store, sizeof(moffs_store));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Eax = 0xA1B2C3D4U;
    cpu.EFlags = 0x000008D5U;
    const bool moffs_dispatched =
        repiu::engine::HandleTracedMemoryStoreInstruction(&cpu, &context);
    std::uint32_t moffs_value = 0U;
    std::memcpy(&moffs_value, bytes + kMoffsDestinationOffset,
                sizeof(moffs_value));
    legacy_moffs_store = moffs_dispatched &&
        moffs_value == 0xA1B2C3D4U &&
        cpu.Eip == kRequestedBase + kCodeOffset + sizeof(moffs_store) &&
        cpu.EFlags == 0x000008D5U;

    const std::uint32_t outside_destination =
        static_cast<std::uint32_t>(kRequestedBase + kArenaSize + 0x100U);
    std::memcpy(bytes + kCodeOffset + 1U, &outside_destination,
                sizeof(outside_destination));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.EFlags = 0x000008D5U;
    legacy_moffs_store_range_rejected =
        !repiu::engine::HandleTracedMemoryStoreInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + kCodeOffset &&
        cpu.EFlags == 0x000008D5U;
#endif

    const std::uint8_t boundary_epilogue[] = {
        0x0FU, 0xA9U,  // pop gs
        0x0FU, 0xA1U,  // pop fs
        0x07U,         // pop es
        0x5FU,         // pop edi
        0x5EU,         // pop esi
        0x5AU,         // pop edx
        0x59U,         // pop ecx
        0x5BU,         // pop ebx
        0xC3U};        // ret
    std::memcpy(bytes + kCodeOffset, boundary_epilogue,
                sizeof(boundary_epilogue));
    const std::uint32_t boundary_values[] = {
        0U, 0U, 0U, 0x11111111U, 0x22222222U, 0x33333333U,
        0x44444444U, 0x55555555U};
    std::memcpy(bytes + kStackOffset, boundary_values,
                sizeof(boundary_values));
    context.aot_reentry_pending = false;
    context.aot_legacy_fallback = false;
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    const bool boundary_dispatched =
        repiu::engine::DispatchGuestHleInstruction(&cpu, &context);
    const bool boundary_epilogue_drained = boundary_dispatched &&
        cpu.Eip == kRequestedBase + kCodeOffset + 10U &&
        cpu.Esp == kRequestedBase + kStackOffset + 32U &&
        cpu.Edi == boundary_values[3] && cpu.Esi == boundary_values[4] &&
        cpu.Edx == boundary_values[5] && cpu.Ecx == boundary_values[6] &&
        cpu.Ebx == boundary_values[7];

    // AOT shared dispatch must offer the existing loader service before the
    // generic far jump enters the original mode16 bridge.
    const std::uint8_t loader_transfer[] = {0x66U, 0xEAU, 0x04U, 0U, 0x2CU, 0U};
    std::memcpy(bytes + kCodeOffset, loader_transfer, sizeof(loader_transfer));
    const char module_name[] = "glide2x.ovl";
    std::memcpy(bytes + 0x700U, module_name, sizeof(module_name));
    const std::uint32_t loader_values[] = {
        0U, 0U, 0U, 0x24U, 0x11223344U, 0x55667788U, 0x99AABBCCU,
        0x12345678U, static_cast<std::uint32_t>(kRequestedBase + 0x200U),
        static_cast<std::uint32_t>(kRequestedBase + 0x700U)};
    std::memcpy(bytes + kStackOffset, loader_values, sizeof(loader_values));
    context.linexe_environment_active = true;
    repiu::hle::BuildLinexeCallGatePlan(&context.linexe_gate_plan);
    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    repiu::runtime::RegisterDescriptor(&context.selector_table,
        {0x2CU, static_cast<std::uint32_t>(kRequestedBase + 0x300U),
         0xFFU, 0U, true});
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Edi = 0x00801B28U;
    const bool loader_dispatch =
        repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        context.linexe_virtual_module_load_count == 1U && cpu.Eax == 1U &&
        cpu.Eip == loader_values[8] &&
        cpu.Esp == kRequestedBase + kStackOffset + 36U &&
        context.guest_es == loader_values[3] && cpu.Ebx == loader_values[4] &&
        cpu.Esi == loader_values[5] && cpu.Edi == loader_values[6] &&
        cpu.Ebp == loader_values[7];
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Edi = 0x0080FFFFU;
    const bool loader_fallback =
        repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + 0x304U &&
        cpu.Esp == kRequestedBase + kStackOffset &&
        context.linexe_virtual_module_load_count == 1U;
    // A valid previous frame must not be replayed when the next is unreadable.
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kArenaSize - 4U);
    cpu.Edi = 0x00801B28U;
    const bool loader_bad_frame =
        repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + 0x304U &&
        cpu.Esp == kRequestedBase + kArenaSize - 4U &&
        context.linexe_virtual_module_load_count == 1U;
    std::cout << "linexe_shared_dispatch=" << loader_dispatch
              << ",fallback=" << loader_fallback
              << ",bad_frame=" << loader_bad_frame << "\n";

    // A bare mode16 RETF reads a word IP and word CS through SS.base and
    // advances the guest stack by four bytes.
    const std::uint8_t mode16_return[] = {0xCBU};
    std::memcpy(bytes + kCodeOffset, mode16_return, sizeof(mode16_return));
    std::uint16_t return_offset = 0x0020U;
    std::uint16_t return_selector = 0x0024U;
    std::memcpy(bytes + kStackOffset, &return_offset,
                sizeof(return_offset));
    std::memcpy(bytes + kStackOffset + 2U, &return_selector,
                sizeof(return_selector));
    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    const bool mode16_return_cs =
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            {0x002CU, static_cast<std::uint32_t>(kRequestedBase),
             static_cast<std::uint32_t>(kArenaSize - 1U), 0U, true,
             repiu::runtime::kLeObjectExecutable, true,
             repiu::runtime::GuestCodeDefaultOperandSize::k16});
    const bool mode16_return_target =
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            {0x0024U, static_cast<std::uint32_t>(kRequestedBase + 0x200U),
             0xFFU, 0U, true, repiu::runtime::kLeObjectExecutable, true,
             repiu::runtime::GuestCodeDefaultOperandSize::k32});
    const bool mode16_return_ss =
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            {0x00B4U, static_cast<std::uint32_t>(kRequestedBase),
             static_cast<std::uint32_t>(kArenaSize - 1U), 0x92U, true});
    context.guest_ss = 0x00B4U;
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.SegCs = 0x002CU;
    const bool mode16_return_handled =
        mode16_return_cs && mode16_return_target && mode16_return_ss &&
        repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + 0x220U && cpu.SegCs == 0x0024U &&
        cpu.Esp == kRequestedBase + kStackOffset + 4U;

    return_offset = 0x0020U;
    return_selector = 0x00FFU;
    std::memcpy(bytes + kStackOffset, &return_offset,
                sizeof(return_offset));
    std::memcpy(bytes + kStackOffset + 2U, &return_selector,
                sizeof(return_selector));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.SegCs = 0x002CU;
    const bool mode16_return_bad_selector =
        !repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + kCodeOffset &&
        cpu.Esp == kRequestedBase + kStackOffset && cpu.SegCs == 0x002CU;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kArenaSize - 2U);
    cpu.SegCs = 0x002CU;
    const bool mode16_return_bad_frame =
        !repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + kCodeOffset &&
        cpu.Esp == kRequestedBase + kArenaSize - 2U && cpu.SegCs == 0x002CU;

    repiu::runtime::InitializeSelectorTable(&context.selector_table);
    const bool mode32_return_cs =
        repiu::runtime::RegisterDescriptor(
            &context.selector_table,
            {0x002CU, static_cast<std::uint32_t>(kRequestedBase),
             static_cast<std::uint32_t>(kArenaSize - 1U), 0U, true,
             repiu::runtime::kLeObjectExecutable, true,
             repiu::runtime::GuestCodeDefaultOperandSize::k32});
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.SegCs = 0x002CU;
    const bool mode32_return_refused =
        mode32_return_cs &&
        !repiu::engine::DispatchGuestHleInstruction(&cpu, &context) &&
        cpu.Eip == kRequestedBase + kCodeOffset &&
        cpu.Esp == kRequestedBase + kStackOffset && cpu.SegCs == 0x002CU;
    std::cout << "mode16_far_return=" << mode16_return_handled
              << ",bad_selector=" << mode16_return_bad_selector
              << ",bad_frame=" << mode16_return_bad_frame
              << ",mode32_refused=" << mode32_return_refused << "\n";

    const bool released =
        repiu::platform::ReleaseMemory(reservation.base, kArenaSize);
    const bool all = ordinary_push && ordinary_pop && push_esp_order &&
        pop_esp_order && rejected && mixed_sequence && bounded &&
        enter_nonnested && enter_nested && enter_range_rejected &&
        legacy_direct_call && legacy_direct_call_range_rejected &&
        legacy_resume_policy && legacy_resume_mode_aware &&
        legacy_resume_thunk &&
        legacy_moffs_store && legacy_moffs_store_range_rejected &&
        cs_source_store && cs_source_missing_refused &&
        boundary_epilogue_drained && loader_dispatch && loader_fallback &&
        loader_bad_frame && mode16_return_handled &&
        mode16_return_bad_selector && mode16_return_bad_frame &&
        mode32_return_refused && released;
    std::cout << "general_stack_push=" << (ordinary_push ? "true" : "false")
              << ",pop=" << (ordinary_pop ? "true" : "false")
              << ",push_esp=" << (push_esp_order ? "true" : "false")
              << ",pop_esp=" << (pop_esp_order ? "true" : "false")
              << ",range_rejected=" << (rejected ? "true" : "false")
              << ",mixed_sequence=" << (mixed_sequence ? "true" : "false")
              << ",bounded=" << (bounded ? "true" : "false")
              << ",enter_nonnested="
              << (enter_nonnested ? "true" : "false")
              << ",enter_nested="
              << (enter_nested ? "true" : "false")
              << ",enter_range_rejected="
              << (enter_range_rejected ? "true" : "false")
              << ",legacy_direct_call="
              << (legacy_direct_call ? "true" : "false")
              << ",legacy_direct_call_range_rejected="
              << (legacy_direct_call_range_rejected ? "true" : "false")
              << ",legacy_resume_policy="
              << (legacy_resume_policy ? "true" : "false")
              << ",legacy_resume_mode_aware="
              << (legacy_resume_mode_aware ? "true" : "false")
              << ",legacy_resume_thunk="
              << (legacy_resume_thunk ? "true" : "false")
              << ",legacy_moffs_store="
              << (legacy_moffs_store ? "true" : "false")
              << ",legacy_moffs_store_range_rejected="
              << (legacy_moffs_store_range_rejected ? "true" : "false")
              << ",cs_source_store="
              << (cs_source_store ? "true" : "false")
              << ",cs_source_missing_refused="
              << (cs_source_missing_refused ? "true" : "false")
              << ",boundary_epilogue="
              << (boundary_epilogue_drained ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
