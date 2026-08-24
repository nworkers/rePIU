#include "repiu/platform/worker_signal.h"

#if defined(_WIN32)

#include <windows.h>

namespace repiu::platform
{

void* CreateWorkerSignal()
{
    // Auto-reset, initially unsignalled -- the same two arguments the engine
    // has always passed, so the worker handshake behaves identically.
    return CreateEventA(nullptr, FALSE, FALSE, nullptr);
}

void DestroyWorkerSignal(void* signal)
{
    if (signal != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(signal));
    }
}

bool SignalWorker(void* signal)
{
    return signal != nullptr && SetEvent(static_cast<HANDLE>(signal)) != 0;
}

bool WaitForWorkerSignal(void* signal)
{
    return signal != nullptr &&
        WaitForSingleObject(static_cast<HANDLE>(signal), INFINITE) ==
            WAIT_OBJECT_0;
}

void ResetWorkerSignal(void* signal)
{
    if (signal != nullptr)
    {
        ResetEvent(static_cast<HANDLE>(signal));
    }
}

}  // namespace repiu::platform

#endif
