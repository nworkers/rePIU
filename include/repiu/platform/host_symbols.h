#ifndef REPIU_PLATFORM_HOST_SYMBOLS_H_
#define REPIU_PLATFORM_HOST_SYMBOLS_H_

#include <cstdint>
#include <string>

// Task 759. Names for addresses in this process, for reports. Win32 answers
// through the module list and dbghelp; Linux resolves nothing and a report
// keeps the raw address.

namespace repiu::platform
{

// Resolves one address to its module file name and offset. False when the
// address belongs to no loaded module, which is normal for guest addresses.
bool ResolveHostModule(std::uint32_t address,
                       std::string* module_name,
                       std::uint32_t* module_offset);

// Symbol resolution is best-effort: without debug information beside the
// binary this answers nothing and the caller falls back to module and offset.
class HostSymbolSession
{
public:
    HostSymbolSession();
    ~HostSymbolSession();

    HostSymbolSession(const HostSymbolSession&) = delete;
    HostSymbolSession& operator=(const HostSymbolSession&) = delete;

    bool Resolve(std::uint32_t address, std::string* name) const;

private:
    bool initialised_ = false;
};

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_SYMBOLS_H_
