#include "repiu/platform/linux_x64_aot_dispatch.h"

#if !defined(_WIN32) && defined(__x86_64__)

#include <cstdint>

extern "C"
{
// The three the thunk reads through RIP-relative loads. Defined here rather
// than in the assembly so their types are C++'s and a mismatch is a compile
// error rather than a wrong load at run time.
void* repiu_linux_x64_dispatch_frame = nullptr;
void* repiu_linux_x64_dispatch_context = nullptr;
void* repiu_linux_x64_dispatch_resolver = nullptr;

void RepiuLinuxX64ReturnThunk();
}

namespace repiu::platform
{

void InstallLinuxX64Dispatch(LinuxX64AotDispatchFrame* const frame,
                             void* const context,
                             const LinuxX64DispatchResolver resolver)
{
    repiu_linux_x64_dispatch_frame = frame;
    repiu_linux_x64_dispatch_context = context;
    repiu_linux_x64_dispatch_resolver =
        reinterpret_cast<void*>(resolver);
}

void ClearLinuxX64Dispatch()
{
    repiu_linux_x64_dispatch_frame = nullptr;
    repiu_linux_x64_dispatch_context = nullptr;
    repiu_linux_x64_dispatch_resolver = nullptr;
}

std::uintptr_t LinuxX64ReturnThunkAddress()
{
    return reinterpret_cast<std::uintptr_t>(&RepiuLinuxX64ReturnThunk);
}

}  // namespace repiu::platform

#endif  // !_WIN32 && __x86_64__
