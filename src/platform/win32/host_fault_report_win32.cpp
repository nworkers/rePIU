#include "repiu/platform/host_fault_report.h"

// Task 759. What Windows adds to a fault report. The bodies are what the
// engine's exception capture did inline.
#if !defined(_M_IX86)
#error "the Win32 host is built as x86 only"
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <cstdio>

namespace repiu::platform
{

static_assert(kFaultFilterRunsHandler == EXCEPTION_EXECUTE_HANDLER);

void ReportHostLanguageException(std::uint32_t host_code,
                                 const void* instruction_address)
{
    // Task 503d-15. 0xE06D7363 is the exception code MSVC raises a C++
    // throw with, so this can only fire on Windows, and what it prints -- a
    // host stack walk and the loaded module list -- is the operating system's
    // to answer. A guest fault never reaches it.
    if (host_code != 0xe06d7363U)
    {
        return;
    }
    fprintf(stderr, "[repiu-live-debug] Caught C++ Exception (0xe06d7363) at address 0x%p\n",
            instruction_address);
    void* stack[64];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    fprintf(stderr, "[repiu-live-debug] Host Stack trace (%d frames):\n", frames);
    for (USHORT i = 0; i < frames; ++i)
    {
        fprintf(stderr, "  [%d] 0x%p\n", i, stack[i]);
    }
    HMODULE modules[256];
    DWORD cbNeeded;
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &cbNeeded))
    {
        fprintf(stderr, "[repiu-live-debug] Loaded Modules:\n");
        for (size_t i = 0; i < cbNeeded / sizeof(HMODULE); ++i)
        {
            char name[MAX_PATH];
            if (GetModuleFileNameA(modules[i], name, sizeof(name)))
            {
                MODULEINFO info;
                if (GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info)))
                {
                    fprintf(stderr, "  base=0x%p size=0x%X path=%s\n",
                            info.lpBaseOfDll, info.SizeOfImage, name);
                }
            }
        }
    }
}

bool QueryHostMemoryNumbers(const void* address,
                            std::uint32_t* state,
                            std::uint32_t* protect)
{
    MEMORY_BASIC_INFORMATION raw = {};
    if (VirtualQuery(address, &raw, sizeof(raw)) != sizeof(raw))
    {
        return false;
    }
    *state = raw.State;
    *protect = raw.Protect;
    return true;
}

bool FillsFaultReportDetail()
{
    return true;
}

bool ReadMemoryForFaultReport(const void* source,
                              void* destination,
                              std::size_t size,
                              std::size_t* copied)
{
    SIZE_T read = 0;
    const bool ok = ReadProcessMemory(GetCurrentProcess(), source, destination,
                                      size, &read) != 0;
    if (copied != nullptr)
    {
        *copied = read;
    }
    return ok;
}

}  // namespace repiu::platform
