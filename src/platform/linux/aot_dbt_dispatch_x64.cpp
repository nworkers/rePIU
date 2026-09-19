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
// Task 720. The Glide gate thunk's resolver, installed beside the dispatch.
void* repiu_linux_x64_glide_gate_resolver = nullptr;
volatile std::uint64_t repiu_linux_x64_guest_entry_rsp = 0U;
volatile std::uint64_t repiu_linux_x64_cache_call_rsp = 0U;
volatile std::uint64_t repiu_linux_x64_return_thunk_rsp = 0U;
volatile std::uint32_t repiu_linux_x64_guest_esp_trace_site = 0U;
volatile std::uint32_t repiu_linux_x64_guest_esp_trace_value = 0U;

void RepiuLinuxX64ReturnThunk();
void RepiuLinuxX64LegacyResumeThunk();
void RepiuLinuxX64GlideGateThunk();
}

namespace repiu::platform
{

void InstallLinuxX64Dispatch(LinuxX64AotDispatchFrame* const frame,
                             void* const context,
                             const LinuxX64DispatchResolver resolver)
{
    if (frame != nullptr)
    {
        // Native observers run between return-dispatch calls, so seed the
        // frame's context once instead of requiring each observer to recover
        // it from a separate platform global.
        frame->context = reinterpret_cast<std::uintptr_t>(context);
    }
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
    repiu_linux_x64_glide_gate_resolver = nullptr;
}

void InstallLinuxX64GlideGateResolver(
    const LinuxX64GlideGateResolver resolver)
{
    repiu_linux_x64_glide_gate_resolver = reinterpret_cast<void*>(resolver);
}

std::uintptr_t LinuxX64GlideGateThunkAddress()
{
    return reinterpret_cast<std::uintptr_t>(&RepiuLinuxX64GlideGateThunk);
}

std::uintptr_t LinuxX64DispatchFramePointerAddress()
{
    return reinterpret_cast<std::uintptr_t>(&repiu_linux_x64_dispatch_frame);
}

std::uintptr_t LinuxX64ReturnThunkAddress()
{
    return reinterpret_cast<std::uintptr_t>(&RepiuLinuxX64ReturnThunk);
}

std::uintptr_t LinuxX64LegacyResumeThunkAddress()
{
    return reinterpret_cast<std::uintptr_t>(&RepiuLinuxX64LegacyResumeThunk);
}

std::uintptr_t LinuxX64GuestEspTraceSiteAddress()
{
    return reinterpret_cast<std::uintptr_t>(
        &repiu_linux_x64_guest_esp_trace_site);
}

std::uintptr_t LinuxX64GuestEspTraceValueAddress()
{
    return reinterpret_cast<std::uintptr_t>(
        &repiu_linux_x64_guest_esp_trace_value);
}

}  // namespace repiu::platform

#endif  // !_WIN32 && __x86_64__
