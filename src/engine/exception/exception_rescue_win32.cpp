#include "repiu/engine/exception_rescue_win32.h"

// Task 503d-14. The header fences this declaration, so the definition follows.
// On Linux the file is empty on purpose: a vectored exception handler is how
// Windows hands a fault over, and the same faults arrive through the signal
// handler 3c installs instead. Keeping the file rather than excluding it from
// the build means the two hosts still compile the same source list.
#if defined(_WIN32)

namespace repiu::engine
{

LONG WINAPI GuestStackVectoredExceptionHandler(EXCEPTION_POINTERS* exception_info)
{
    return DispatchGuestException(exception_info);
}

} // namespace repiu::engine

#endif  // defined(_WIN32)
