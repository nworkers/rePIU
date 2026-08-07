#include "host_crash_report.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>
#endif

namespace repiu::platform::win32
{
#if defined(_WIN32)
namespace
{

constexpr int kMaxReportedFrames = 48;

const char* ExceptionName(DWORD code)
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            return "ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:
            return "STACK_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_PRIV_INSTRUCTION:
            return "PRIV_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:
            return "IN_PAGE_ERROR";
        case 0xE06D7363U:
            return "CXX_EXCEPTION";
        default:
            return "UNKNOWN";
    }
}

void ReportFrame(HANDLE process, int index, DWORD64 address)
{
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;
    DWORD64 symbol_displacement = 0;
    const bool named =
        SymFromAddr(process, address, &symbol_displacement, symbol) != 0;

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);
    DWORD line_displacement = 0;
    const bool located =
        SymGetLineFromAddr64(process, address, &line_displacement, &line) != 0;

    // The module and offset are printed even when nothing resolves, because a
    // report that names no symbol is still enough to place the fault.
    IMAGEHLP_MODULE64 module = {};
    module.SizeOfStruct = sizeof(module);
    const bool in_module = SymGetModuleInfo64(process, address, &module) != 0;

    std::fprintf(stderr,
                 "[repiu-host-crash] frame %02d 0x%08llX %s%s",
                 index,
                 static_cast<unsigned long long>(address),
                 in_module ? module.ModuleName : "?",
                 named ? "!" : "");
    if (named)
    {
        std::fprintf(stderr, "%s+0x%llX", symbol->Name,
                     static_cast<unsigned long long>(symbol_displacement));
    }
    if (located)
    {
        std::fprintf(stderr, " (%s:%lu)", line.FileName, line.LineNumber);
    }
    std::fprintf(stderr, "\n");
}

LONG WINAPI ReportUnhandledException(EXCEPTION_POINTERS* pointers)
{
    if (pointers == nullptr || pointers->ExceptionRecord == nullptr ||
        pointers->ContextRecord == nullptr)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const EXCEPTION_RECORD& record = *pointers->ExceptionRecord;
    std::fprintf(stderr,
                 "[repiu-host-crash] code=0x%08lX (%s) address=0x%08llX"
                 " thread=%lu\n",
                 record.ExceptionCode, ExceptionName(record.ExceptionCode),
                 reinterpret_cast<unsigned long long>(record.ExceptionAddress),
                 GetCurrentThreadId());
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        record.NumberParameters >= 2U)
    {
        // Parameter 0 is the access kind, parameter 1 the address touched --
        // which is usually the fact that names the bug.
        const ULONG_PTR kind = record.ExceptionInformation[0];
        std::fprintf(stderr,
                     "[repiu-host-crash] access=%s target=0x%08llX\n",
                     kind == 0U   ? "read"
                     : kind == 1U ? "write"
                                  : "execute",
                     static_cast<unsigned long long>(
                         record.ExceptionInformation[1]));
    }

    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    const bool symbols = SymInitialize(process, nullptr, TRUE) != 0;

    // A copy, because StackWalk64 modifies the context it walks.
    CONTEXT context = *pointers->ContextRecord;
    STACKFRAME64 frame = {};
#if defined(_M_IX86)
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
#else
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int index = 0; index < kMaxReportedFrames; ++index)
    {
        if (!StackWalk64(machine, process, GetCurrentThread(), &frame, &context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                         nullptr))
        {
            break;
        }
        if (frame.AddrPC.Offset == 0U)
        {
            break;
        }
        ReportFrame(process, index, frame.AddrPC.Offset);
    }
    if (symbols)
    {
        SymCleanup(process);
    }
    std::fflush(stderr);
    // The process was going to die on this exception either way; the report is
    // the only thing added.
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace
#endif

void InstallWin32HostCrashReporter()
{
#if defined(_WIN32)
    SetUnhandledExceptionFilter(&ReportUnhandledException);
#endif
}

}  // namespace repiu::platform::win32
