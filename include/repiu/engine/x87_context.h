#ifndef REPIU_ENGINE_X87_CONTEXT_H_
#define REPIU_ENGINE_X87_CONTEXT_H_

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::engine
{

#if defined(_WIN32)
bool PushX87Float(CONTEXT* context, float value);
#endif

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_X87_CONTEXT_H_
