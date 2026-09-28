#include "repiu/platform/host_symbols.h"

// Task 759. Linux resolves no names; a report keeps the raw addresses.

namespace repiu::platform
{

bool ResolveHostModule(std::uint32_t address,
                       std::string* module_name,
                       std::uint32_t* module_offset)
{
    (void)address;
    (void)module_name;
    (void)module_offset;
    return false;
}

HostSymbolSession::HostSymbolSession() = default;

HostSymbolSession::~HostSymbolSession() = default;

bool HostSymbolSession::Resolve(std::uint32_t address, std::string* name) const
{
    (void)address;
    (void)name;
    (void)initialised_;
    return false;
}

}  // namespace repiu::platform
