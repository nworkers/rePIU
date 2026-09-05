#include "segment_push_probe.h"

#include "repiu/runtime/selector_table.h"

#include <iostream>

namespace repiu::tools
{

bool RunSegmentPushProbe()
{
    repiu::runtime::SelectorTable table;
    const bool initialized = repiu::runtime::InitializeSelectorTable(&table);
    const bool descriptors = initialized &&
        repiu::runtime::RegisterDescriptor(
            &table, {0x0024U, 0x01010000U, 0x000EBBDFU, 0U, true}) &&
        repiu::runtime::RegisterDescriptor(
            &table, {0x0080U, 0x09000000U, 0x0000914FU, 0U, true});

    std::uint16_t selector = 0;
    const bool unique = descriptors &&
        repiu::runtime::FindSelectorForLinearAddress(
            table, 0x010F0117U, &selector) && selector == 0x0024U;
    const bool absent = descriptors &&
        !repiu::runtime::FindSelectorForLinearAddress(
            table, 0x08000000U, &selector);
    const bool overlap_registered = descriptors &&
        repiu::runtime::RegisterDescriptor(
            &table, {0x0090U, 0x010F0000U, 0x000001FFU, 0U, true});
    const bool overlap_rejected = overlap_registered &&
        !repiu::runtime::FindSelectorForLinearAddress(
            table, 0x010F0117U, &selector);
    const bool all = unique && absent && overlap_rejected;

    std::cout << "segment_push_code_selector=true,unique="
              << (unique ? "true" : "false")
              << ",absent=" << (absent ? "true" : "false")
              << ",overlap_rejected="
              << (overlap_rejected ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
