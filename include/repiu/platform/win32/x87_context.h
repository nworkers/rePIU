#ifndef REPIU_PLATFORM_WIN32_X87_CONTEXT_H_
#define REPIU_PLATFORM_WIN32_X87_CONTEXT_H_

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::platform::win32
{

#if defined(_WIN32)
bool PushX87Float(CONTEXT* context, float value);
#endif

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_X87_CONTEXT_H_
