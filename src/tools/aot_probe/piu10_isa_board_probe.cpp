#include "piu10_isa_board_probe.h"

#include "repiu/hle/piu10_isa_board.h"
#include "repiu/target/target_profile.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace repiu::tools
{

bool RunPiu10IsaBoardProbe()
{
    bool target_profiles_valid = true;
    for (const char* id : {"pumpit1", "pumpit2", "pumpit3"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        target_profiles_valid = target_profiles_valid && profile != nullptr &&
            !profile->enable_piu10_isa_board;
    }
    for (const char* id : {"pumpito", "pumpitc", "pumpitpc", "pumpite"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        target_profiles_valid = target_profiles_valid && profile != nullptr &&
            profile->enable_piu10_isa_board;
    }

    std::vector<std::uint8_t> flash(hle::Piu10IsaBoard::kFlashBytes, 0xFFU);
    flash[0x2468U] = 0x34U;
    flash[0x2469U] = 0x12U;
    flash[0x246AU] = 0x78U;
    flash[0x246BU] = 0x56U;
    const std::array<std::uint8_t, 8> transform = {
        0x5AU, 0xA5U, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU};

    hle::Piu10IsaBoard board;
    std::string message;
    if (!board.Initialize(std::move(flash), transform, &message))
    {
        std::cout << "piu10_isa_board_probe=false,message=" << message << "\n";
        return false;
    }

    std::uint16_t value = 0;
    const bool status_valid =
        board.Write16(0x02D4U, 0x0080U) &&
        board.Write16(0x02D6U, 0x0000U) &&
        board.Read16(0x02DAU, &value) && (value & 0x0007U) == 0x0007U;

    const bool flash_valid =
        board.Write16(0x02D0U, 0x0034U) &&
        board.Write16(0x02D2U, 0x0012U) &&
        board.Write16(0x02D4U, 0x0000U) &&
        board.Write16(0x02D6U, 0x0000U) &&
        board.Write16(0x02DCU, 0x0008U) &&
        board.Read16(0x02DAU, &value) && value == 0x1234U &&
        board.Read16(0x02DAU, &value) && value == 0x5678U &&
        board.address() == 0x1236U;

    board.Reset();
    const std::array<std::uint8_t, 8> expected_cat_bits = {
        1U, 0U, 1U, 0U, 1U, 0U, 1U, 0U};
    bool cat_sequence_valid =
        board.Write16(0x02D4U, 0x0100U) &&
        board.Write16(0x02D6U, 0x0001U) &&
        board.Write16(0x02DAU, 0x0000U);
    for (std::uint8_t expected : expected_cat_bits)
    {
        cat_sequence_valid = cat_sequence_valid &&
            board.Write16(0x02D4U, 0x0100U) &&
            board.Write16(0x02D6U, 0x0001U) &&
            board.Write16(0x02DAU, 0x0000U) &&
            board.Write16(0x02DAU, 0x0010U) &&
            board.Write16(0x02D4U, 0x0080U) &&
            board.Write16(0x02D6U, 0x0000U) &&
            board.Read16(0x02DAU, &value) &&
            ((value >> 5U) & 1U) == expected;
    }

    const bool valid = target_profiles_valid && status_valid && flash_valid &&
        cat_sequence_valid;
    std::cout << "piu10_target_profiles="
              << (target_profiles_valid ? "true" : "false") << "\n";
    std::cout << "piu10_isa_board_probe=" << (valid ? "true" : "false")
              << ",destination=0x" << std::hex << board.destination()
              << ",value=0x" << value << std::dec << "\n";
    return valid;
}

}  // namespace repiu::tools
