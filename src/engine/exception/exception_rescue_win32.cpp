#include "repiu/engine/exception_rescue_win32.h"

// Task 503d-14. The header fences this declaration, so the definition follows.
// A vectored exception handler is how Windows hands a fault over; on Linux the
// same faults arrive through the signal handler 3c installs instead.
//
// Task 758. Built on Win32 only. It used to be compiled empty on Linux so the two
// hosts kept one source list.
// Task 759. It calls into the engine, so it stays in the engine until the
// Win32 entry is reduced to a platform function that takes a callback.
#if defined(_WIN32)

namespace repiu::engine
{

LONG WINAPI GuestStackVectoredExceptionHandler(EXCEPTION_POINTERS* exception_info)
{
    return DispatchGuestException(exception_info);
}

} // namespace repiu::engine

#endif  // defined(_WIN32)
