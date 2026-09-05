#include "far_jump_probe.h"

#include "repiu/runtime/selector_table.h"

#include <iostream>

namespace repiu::tools
{

bool RunFarJumpProbe()
{
    repiu::runtime::SelectorTable table;
    const bool initialized = repiu::runtime::InitializeSelectorTable(&table);
    const bool registered = initialized && repiu::runtime::RegisterDescriptor(
        &table, {0x002CU, 0x01100000U, 0x00000047U, 0U, true});

    std::uint32_t target = 0;
    const bool valid = registered &&
        repiu::runtime::TranslateSelectorOffset(
            table, 0x002CU, 0x0004U, 1U, &target) &&
        target == 0x01100004U;
    const bool missing = registered &&
        !repiu::runtime::TranslateSelectorOffset(
            table, 0x0034U, 0U, 1U, &target);
    const bool out_of_limit = registered &&
        !repiu::runtime::TranslateSelectorOffset(
            table, 0x002CU, 0x0048U, 1U, &target);
    const bool all = valid && missing && out_of_limit;

    std::cout << "far_jump_selector_translation=true,valid="
              << (valid ? "true" : "false")
              << ",missing=" << (missing ? "true" : "false")
              << ",out_of_limit="
              << (out_of_limit ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
