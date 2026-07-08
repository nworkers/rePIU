#include "repiu/platform/win32/execution_trampoline.h"

#include <sstream>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::platform::win32
{
namespace
{

struct ThreadContext
{
    std::uint32_t entry_address = 0;
    bool returned = false;
    bool exception_caught = false;
    std::uint32_t exception_code = 0;
    std::uint32_t exception_address = 0;
};

bool IsDirectX86ExecutionSupported()
{
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    return true;
#else
    return false;
#endif
}

#if defined(_WIN32)
int CaptureException(EXCEPTION_POINTERS* exception_info,
                     ThreadContext* context)
{
    if (exception_info != nullptr && context != nullptr)
    {
        context->exception_caught = true;
        context->exception_code =
            exception_info->ExceptionRecord->ExceptionCode;
        context->exception_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(
                exception_info->ExceptionRecord->ExceptionAddress));
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

DWORD WINAPI GuestEntryThreadProc(void* parameter)
{
    ThreadContext* context = static_cast<ThreadContext*>(parameter);
    if (context == nullptr)
    {
        return 1;
    }

    using EntryFunction = void (*)();
    EntryFunction entry = reinterpret_cast<EntryFunction>(
        static_cast<std::uintptr_t>(context->entry_address));

    __try
    {
        entry();
        context->returned = true;
        return 0;
    }
    __except (CaptureException(GetExceptionInformation(), context))
    {
        return 2;
    }
}

#endif

}  // namespace

bool AttemptWin32MinimalExecution(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return false;
    }

    *attempt = Win32MinimalExecutionAttempt{};
    attempt->entry_address = entry_address;
    attempt->supported = IsDirectX86ExecutionSupported();

    if (!attempt->supported)
    {
        attempt->valid = true;
#if defined(_WIN32)
        attempt->message =
            "minimal original entry execution requires a 32-bit host";
#else
        attempt->message =
            "minimal original entry execution requires Win32 host APIs";
#endif
        return true;
    }

#if !defined(_WIN32)
    attempt->valid = true;
    attempt->message =
        "minimal original entry execution requires Win32 host APIs";
    return true;
#else
    if (!placement.valid || !placement.placed)
    {
        attempt->message = "relocated image is not placed";
        return false;
    }

    ThreadContext context;
    context.entry_address = entry_address;

    HANDLE thread = CreateThread(nullptr,
                                 0,
                                 GuestEntryThreadProc,
                                 &context,
                                 0,
                                 nullptr);
    if (thread == nullptr)
    {
        const DWORD error = GetLastError();
        std::ostringstream stream;
        stream << "CreateThread failed with error " << error;
        attempt->message = stream.str();
        return false;
    }

    attempt->attempted = true;
    const DWORD wait_result = WaitForSingleObject(
        thread,
        timeout_milliseconds);

    if (wait_result == WAIT_TIMEOUT)
    {
        TerminateThread(thread, 3);
        attempt->timed_out = true;
        attempt->thread_exit_code = 3;
        attempt->valid = true;
        attempt->message = "minimal execution attempt timed out";
        CloseHandle(thread);
        return true;
    }

    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);

    attempt->returned = context.returned;
    attempt->exception_caught = context.exception_caught;
    attempt->seh_exception_code = context.exception_code;
    attempt->seh_exception_address = context.exception_address;
    attempt->thread_exit_code = exit_code;
    attempt->valid = true;

    if (attempt->returned)
    {
        attempt->message = "original entry returned to host trampoline";
    }
    else if (attempt->exception_caught)
    {
        attempt->message = "original entry raised a caught exception";
    }
    else
    {
        attempt->message =
            "minimal execution attempt ended without return or exception";
    }

    return true;
#endif
}

}  // namespace repiu::platform::win32
