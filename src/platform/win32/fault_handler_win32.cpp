#include "repiu/platform/fault_handler.h"

#if defined(_WIN32)

#include <windows.h>

namespace repiu::platform
{
namespace
{

FaultCallback g_callback = nullptr;
void* g_user_data = nullptr;
PVOID g_handle = nullptr;

FaultKind ClassifyException(const DWORD code)
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            return FaultKind::kAccessViolation;
        case EXCEPTION_SINGLE_STEP:
            return FaultKind::kSingleStep;
        case EXCEPTION_BREAKPOINT:
            return FaultKind::kBreakpoint;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return FaultKind::kIllegalInstruction;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return FaultKind::kIntegerDivideByZero;
        case EXCEPTION_PRIV_INSTRUCTION:
            return FaultKind::kPrivilegedInstruction;
        default:
            return FaultKind::kOther;
    }
}

GuestFaultInfo ReadAccessInfo(const EXCEPTION_RECORD& record)
{
    GuestFaultInfo info;
    if (record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        record.NumberParameters < 2U)
    {
        return info;
    }
    // Parameter 0 is the access: 0 read, 1 write, 8 an execution attempt on a
    // no-execute page. The three are distinct; an execute fault is neither a
    // read nor a write.
    info.write_access = record.ExceptionInformation[0] == 1U;
    info.execute_access = record.ExceptionInformation[0] == 8U;
    info.fault_address =
        static_cast<std::uint32_t>(record.ExceptionInformation[1]);
    info.valid = true;
    return info;
}

FaultEvent BuildFaultEvent(EXCEPTION_POINTERS* exception_info)
{
    FaultEvent event;
    if (exception_info == nullptr ||
        exception_info->ExceptionRecord == nullptr ||
        exception_info->ContextRecord == nullptr)
    {
        return event;
    }
    event.kind = ClassifyException(
        exception_info->ExceptionRecord->ExceptionCode);
    event.host_code = static_cast<std::uint32_t>(
        exception_info->ExceptionRecord->ExceptionCode);
    event.access = ReadAccessInfo(*exception_info->ExceptionRecord);
    event.instruction_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(
            exception_info->ExceptionRecord->ExceptionAddress));
    event.registers = exception_info->ContextRecord;
    return event;
}

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* exception_info)
{
    if (g_callback == nullptr || exception_info == nullptr ||
        exception_info->ExceptionRecord == nullptr ||
        exception_info->ContextRecord == nullptr)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    // GuestCpuContext is CONTEXT here, so the callback edits the kernel's own
    // structure and nothing has to be copied back. The instruction address is
    // taken from the record rather than from Eip, so the probe's comparison of
    // the two is a real measurement and not a tautology.
    FaultEvent event = BuildFaultEvent(exception_info);

    const FaultDisposition disposition = g_callback(&event, g_user_data);
    return disposition == FaultDisposition::kResume
        ? EXCEPTION_CONTINUE_EXECUTION
        : EXCEPTION_CONTINUE_SEARCH;
}


}  // namespace

FaultEvent MakeFaultEventFromWin32(struct _EXCEPTION_POINTERS* exception_info)
{
    return BuildFaultEvent(exception_info);
}

bool InstallFaultHandler(FaultCallback callback, void* user_data)
{
    if (callback == nullptr || g_handle != nullptr)
    {
        return false;
    }
    g_callback = callback;
    g_user_data = user_data;
    // First in the chain, so the engine sees a guest fault before anything else
    // can claim it.
    g_handle = AddVectoredExceptionHandler(1, VectoredHandler);
    if (g_handle == nullptr)
    {
        g_callback = nullptr;
        g_user_data = nullptr;
        return false;
    }
    return true;
}

bool RemoveFaultHandler()
{
    if (g_handle == nullptr)
    {
        return false;
    }
    const ULONG removed = RemoveVectoredExceptionHandler(g_handle);
    g_handle = nullptr;
    g_callback = nullptr;
    g_user_data = nullptr;
    return removed != 0;
}

}  // namespace repiu::platform

#endif
