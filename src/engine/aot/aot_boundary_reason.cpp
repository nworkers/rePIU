#include "aot_boundary_reason.h"

namespace repiu::engine
{

AotBoundaryReason ClassifyAotBoundaryInstruction(const std::uint8_t* bytes,
                                                 std::size_t size)
{
    if (bytes == nullptr || size == 0)
    {
        return AotBoundaryReason::kOther;
    }
    const std::uint8_t opcode = bytes[0];
    switch (opcode)
    {
        case 0xC3U: // ret
        case 0xC2U: // ret imm16
        case 0xCBU: // retf
        case 0xCAU: // retf imm16
            return AotBoundaryReason::kReturn;
        case 0xE8U: // call rel32
        case 0xE9U: // jmp rel32
        case 0xEBU: // jmp rel8
        case 0x9AU: // call far ptr16:32
        case 0xEAU: // jmp far ptr16:32
            return AotBoundaryReason::kDirectBranch;
        case 0xE0U: // loopne rel8
        case 0xE1U: // loope rel8
        case 0xE2U: // loop rel8
        case 0xE3U: // jecxz rel8
            return AotBoundaryReason::kConditionalBranch;
        case 0x0FU:
            // 0F 80..8F = Jcc rel32.
            if (size >= 2 && bytes[1] >= 0x80U && bytes[1] <= 0x8FU)
            {
                return AotBoundaryReason::kConditionalBranch;
            }
            return AotBoundaryReason::kOther;
        case 0xFFU:
        {
            // /2 call r/m, /3 far call, /4 jmp r/m, /5 far jmp are the indirect
            // transfers; other /reg values (inc/dec/push) are not transfers.
            if (size < 2)
            {
                return AotBoundaryReason::kOther;
            }
            const std::uint8_t reg = (bytes[1] >> 3) & 0x07U;
            if (reg >= 2U && reg <= 5U)
            {
                return AotBoundaryReason::kIndirectBranch;
            }
            return AotBoundaryReason::kOther;
        }
        default:
            // 70..7F = Jcc rel8.
            if (opcode >= 0x70U && opcode <= 0x7FU)
            {
                return AotBoundaryReason::kConditionalBranch;
            }
            return AotBoundaryReason::kOther;
    }
}

} // namespace repiu::engine
