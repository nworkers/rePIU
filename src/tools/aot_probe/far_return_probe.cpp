#include "far_return_probe.h"

#include "repiu/runtime/selector_table.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

constexpr std::uint16_t kCurrentSelector = 0x002CU;
constexpr std::uint16_t kTargetSelector = 0x0024U;
constexpr std::uint32_t kCurrentBase = 0x01100000U;
constexpr std::uint32_t kTargetBase = 0x01010000U;
constexpr std::uint32_t kCurrentLimit = 0x00000047U;
constexpr std::uint32_t kTargetLimit = 0x000EBBDFU;
constexpr std::uint32_t kTargetOffset = 0x010F0232U;
constexpr std::uint32_t kTargetRelativeOffset = 0x000E0232U;
constexpr std::uint32_t kTargetLinear = 0x010F0232U;

repiu::runtime::SelectorTable BuildTable(
    bool current_uses_32_bit_default,
    bool target_executable)
{
    repiu::runtime::SelectorTable table;
    if (!repiu::runtime::InitializeSelectorTable(&table))
    {
        return table;
    }

    const std::uint32_t current_flags =
        repiu::runtime::kLeObjectExecutable | 0x00001000U;
    const std::uint32_t target_flags =
        repiu::runtime::kLeObjectExecutable |
        repiu::runtime::kLeObjectBigDefault;
    repiu::runtime::RegisterDescriptor(
        &table,
        {kCurrentSelector,
         kCurrentBase,
         kCurrentLimit,
         0U,
         true,
         current_flags,
         true,
         current_uses_32_bit_default
             ? repiu::runtime::GuestCodeDefaultOperandSize::k32
             : repiu::runtime::GuestCodeDefaultOperandSize::k16});
    repiu::runtime::RegisterDescriptor(
        &table,
        {kTargetSelector,
         kTargetBase,
         kTargetLimit,
         0U,
         true,
         target_flags,
         target_executable,
         repiu::runtime::GuestCodeDefaultOperandSize::k32});
    return table;
}

}  // namespace

bool RunFarReturnProbe()
{
    const repiu::runtime::SelectorTable valid_table = BuildTable(false, true);
    repiu::runtime::GuestFarReturnResolution resolution;
    const bool valid =
        repiu::runtime::ResolveGuestFarReturn32Frame(
            valid_table,
            kCurrentSelector,
            kTargetOffset,
            0xABCD0024U,
            &resolution) &&
        resolution.valid && resolution.target_offset == kTargetOffset &&
        resolution.target_selector == kTargetSelector &&
        resolution.target_linear == kTargetLinear &&
        resolution.stack_bytes ==
            repiu::runtime::kGuestFarReturn32FrameBytes;

    repiu::runtime::GuestFarReturnResolution relative_resolution;
    const bool relative_offset_valid =
        repiu::runtime::ResolveGuestFarReturn32Frame(
            valid_table,
            kCurrentSelector,
            kTargetRelativeOffset,
            kTargetSelector,
            &relative_resolution) &&
        relative_resolution.target_linear == kTargetLinear &&
        relative_resolution.stack_bytes ==
            repiu::runtime::kGuestFarReturn32FrameBytes;

    repiu::runtime::GuestFarReturnResolution rejected_target;
    const bool non_executable_target =
        !repiu::runtime::ResolveGuestFarReturn32Frame(
            BuildTable(false, false),
            kCurrentSelector,
            kTargetOffset,
            kTargetSelector,
            &rejected_target);

    repiu::runtime::GuestFarReturnResolution out_of_limit;
    const bool offset_limit_rejected =
        !repiu::runtime::ResolveGuestFarReturn32Frame(
            valid_table,
            kCurrentSelector,
            kTargetLimit + 1U,
            kTargetSelector,
            &out_of_limit);

    repiu::runtime::GuestFarReturnResolution rejected_mode;
    const bool current_32_bit_rejected =
        !repiu::runtime::ResolveGuestFarReturn32Frame(
            BuildTable(true, true),
            kCurrentSelector,
            kTargetOffset,
            kTargetSelector,
            &rejected_mode);

    const bool all = valid && relative_offset_valid &&
        non_executable_target && offset_limit_rejected &&
        current_32_bit_rejected;
    std::cout << "far_return_frame=" << (valid ? "true" : "false")
              << ",offset=0x" << std::hex << resolution.target_offset
              << ",selector=0x" << resolution.target_selector
              << ",stack_bytes=" << std::dec << resolution.stack_bytes
              << ",target=0x" << std::hex << resolution.target_linear
              << "\nfar_return_refusals="
              << (non_executable_target ? "true" : "false")
              << ",relative_offset="
              << (relative_offset_valid ? "true" : "false")
              << ",offset_limit=" << (offset_limit_rejected ? "true" : "false")
              << ",current_32_bit="
              << (current_32_bit_rejected ? "true" : "false") << "\n"
              << "far_return_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
