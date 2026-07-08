#ifndef REPIU_HLE_HLE_DISPATCHER_H_
#define REPIU_HLE_HLE_DISPATCHER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::hle
{

enum class HleTrapKind
{
    kInterrupt,
    kException,
    kIoPort,
};

struct HleTrapDescriptor
{
    HleTrapKind kind = HleTrapKind::kInterrupt;
    std::uint32_t vector = 0;
    std::string name;
    bool implemented = false;
};

struct HleDispatcherTable
{
    bool valid = false;
    std::vector<HleTrapDescriptor> traps;
    std::string message;
};

bool BuildInitialHleDispatcherTable(HleDispatcherTable* table);

const HleTrapDescriptor* FindHleTrap(const HleDispatcherTable& table,
                                     HleTrapKind kind,
                                     std::uint32_t vector);

}  // namespace repiu::hle

#endif  // REPIU_HLE_HLE_DISPATCHER_H_
