#include "repiu/platform/host_symbols.h"

// Task 759. Win32 names for addresses: module and offset through
// GetModuleHandleEx, and the function through dbghelp.

#include <cstdio>
#include <cstring>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

namespace repiu::platform
{

bool ResolveHostModule(std::uint32_t address,
                       std::string* module_name,
                       std::uint32_t* module_offset)
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(static_cast<std::uintptr_t>(address)),
            &module) ||
        module == nullptr)
    {
        return false;
    }
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
    if (length == 0U)
    {
        return false;
    }
    const char* leaf = std::strrchr(path, '\\');
    *module_name = leaf != nullptr ? leaf + 1 : path;
    *module_offset = address -
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    return true;
}

HostSymbolSession::HostSymbolSession()
{
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    initialised_ = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != 0;
}

HostSymbolSession::~HostSymbolSession()
{
    if (initialised_)
    {
        SymCleanup(GetCurrentProcess());
    }
}

bool HostSymbolSession::Resolve(std::uint32_t address, std::string* name) const
{
    if (!initialised_)
    {
        return false;
    }
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;
    DWORD64 displacement = 0;
    if (!SymFromAddr(GetCurrentProcess(),
                     static_cast<DWORD64>(address),
                     &displacement,
                     symbol))
    {
        return false;
    }
    *name = symbol->Name;
    if (displacement != 0)
    {
        *name += "+0x";
        char digits[32] = {};
        std::snprintf(digits, sizeof(digits), "%llX",
                      static_cast<unsigned long long>(displacement));
        *name += digits;
    }
    return true;
}

}  // namespace repiu::platform
