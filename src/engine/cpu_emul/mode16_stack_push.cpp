#include "mode16_stack_push.h"

#include "execution_internal.h"
#include "guest_memory_access.h"
#include "instruction_emulation.h"
#include "repiu/runtime/guest_stack_access.h"

#include <Zydis.h>

namespace repiu::engine
{

std::optional<bool> HandleMode16StackPush(
    repiu::platform::GuestCpuContext* registers, ThreadContext* context)
{
    if (registers == nullptr || context == nullptr)
    {
        return std::nullopt;
    }
    std::uint16_t cs = 0;
    if (!runtime::FindSelectorForLinearAddress(
            context->selector_table, registers->Eip, &cs))
    {
        return std::nullopt;
    }
    const auto* code = runtime::FindDescriptor(context->selector_table, cs);
    if (code == nullptr || !code->executable ||
        code->code_default_operand_size != runtime::GuestCodeDefaultOperandSize::k16)
    {
        return std::nullopt;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(registers->Eip));
    if (!IsGuestRangeReadable(context, bytes, 15U))
    {
        return false;
    }
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_16, ZYDIS_STACK_WIDTH_16);
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, bytes, 15U, &instruction, operands)))
    {
        return false;
    }
    if (instruction.mnemonic != ZYDIS_MNEMONIC_PUSH)
    {
        return std::nullopt;
    }
    if (instruction.raw.prefix_count > 1U ||
        (instruction.raw.prefix_count == 1U && bytes[0] != 0x66U) ||
        instruction.operand_count_visible != 1U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER)
    {
        return false;
    }
    std::uint32_t value = 0;
    if (instruction.opcode >= 0x50U && instruction.opcode <= 0x57U)
    {
        value = ReadGeneralRegister32(registers, instruction.opcode - 0x50U);
    }
    else
    {
        switch (operands[0].reg.value)
        {
            case ZYDIS_REGISTER_CS: value = cs; break;
            case ZYDIS_REGISTER_SS: value = context->guest_ss; break;
            case ZYDIS_REGISTER_DS: value = context->guest_ds; break;
            case ZYDIS_REGISTER_ES: value = context->guest_es; break;
            case ZYDIS_REGISTER_FS: value = context->guest_fs; break;
            case ZYDIS_REGISTER_GS: value = context->guest_gs; break;
            default: return false;
        }
    }
    const auto* stack = runtime::FindDescriptor(
        context->selector_table, context->guest_ss);
    runtime::GuestStackPushAccess access;
    const std::uint32_t operand_bytes = instruction.operand_width / 8U;
    if (stack == nullptr || !runtime::ResolveGuestStackPushAccess(
            *stack, registers->Esp, operand_bytes, &access))
    {
        return false;
    }
    auto* destination = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(access.linear_address));
    if (!IsGuestRangeWritable(context, destination, operand_bytes))
    {
        return false;
    }
    const bool written = operand_bytes == 2U
        ? WriteGuestUInt16(context, destination, static_cast<std::uint16_t>(value), registers)
        : WriteGuestUInt32(context, destination, value, registers);
    if (!written)
    {
        return false;
    }
    registers->Esp = access.next_esp;
    registers->Eip += instruction.length;
    return true;
}

}  // namespace repiu::engine
