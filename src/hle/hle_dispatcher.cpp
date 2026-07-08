#include "repiu/hle/hle_dispatcher.h"

namespace repiu::hle
{

bool BuildInitialHleDispatcherTable(HleDispatcherTable* table)
{
    if (table == nullptr)
    {
        return false;
    }

    *table = HleDispatcherTable{};
    table->traps.push_back(
        {HleTrapKind::kInterrupt, 0x21, "DOS INT 21h", false});
    table->traps.push_back(
        {HleTrapKind::kInterrupt, 0x31, "DPMI INT 31h", false});
    table->traps.push_back(
        {HleTrapKind::kException, 0xC0000096,
         "Win32 privileged instruction exception", false});
    table->valid = true;
    table->message = "initial HLE dispatcher table is ready";
    return true;
}

const HleTrapDescriptor* FindHleTrap(const HleDispatcherTable& table,
                                     HleTrapKind kind,
                                     std::uint32_t vector)
{
    if (!table.valid)
    {
        return nullptr;
    }

    for (const HleTrapDescriptor& trap : table.traps)
    {
        if (trap.kind == kind && trap.vector == vector)
        {
            return &trap;
        }
    }

    return nullptr;
}

}  // namespace repiu::hle
