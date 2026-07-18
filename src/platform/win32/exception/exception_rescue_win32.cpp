#include "exception_rescue_win32.h"

namespace repiu::platform::win32
{

LONG WINAPI GuestStackVectoredExceptionHandler(EXCEPTION_POINTERS* exception_info)
{
    return DispatchGuestException(exception_info);
}

} // namespace repiu::platform::win32
