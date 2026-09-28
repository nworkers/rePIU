#include "repiu/platform/host_telemetry.h"

#include "repiu/platform/win32/win32_thread_api.h"
#include "repiu/runtime/execution_timeout.h"

// Task 759. The Win32 half of the live telemetry's host reads: the section
// another process maps, the loader's module range, the thread times, and the
// registers of a suspended thread.
#if !defined(_M_IX86)
#error "the Win32 host is built as x86 only"
#endif

// Task 503d-16. psapi is read by the module information below. The include
// follows what uses it.
#include <psapi.h>

namespace repiu::platform
{

// 3d-14: the section another process maps, and the thread a watchdog waits
// on. Neither is needed to run the guest, and neither has a counterpart
// worth inventing -- Linux cannot suspend a thread and read its registers
// from inside the same process at all.
HostSharedMapping OpenHostSharedMappingFromEnvironment(
    const char* variable_name)
{
    HostSharedMapping result;
    char mapping_name[256] = {};
    if (GetEnvironmentVariableA(variable_name,
                                mapping_name,
                                sizeof(mapping_name)) == 0)
    {
        return result;
    }
    result.mapping = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        mapping_name);
    if (result.mapping == nullptr)
    {
        return result;
    }
    result.view = MapViewOfFile(
        static_cast<HANDLE>(result.mapping), FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (result.view == nullptr)
    {
        CloseHandle(static_cast<HANDLE>(result.mapping));
        result.mapping = nullptr;
    }
    return result;
}

void CloseHostSharedMapping(HostSharedMapping* mapping)
{
    if (mapping == nullptr)
    {
        return;
    }
    if (mapping->view != nullptr)
    {
        UnmapViewOfFile(mapping->view);
        mapping->view = nullptr;
    }
    if (mapping->mapping != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mapping->mapping));
        mapping->mapping = nullptr;
    }
}

// Task 503d-17. The host spells "no limit" with a neutral constant, and on
// Windows it also reaches a Win32 wait. They are the same number, and this is
// what says so where a change to either would be noticed.
static_assert(repiu::runtime::kWaitForeverMilliseconds == INFINITE);

void ReadHostImageRange(std::uint32_t* base, std::uint32_t* size)
{
    std::uint32_t& loader_module_base = *base;
    std::uint32_t& loader_module_size = *size;
    {
        const HMODULE loader_module = GetModuleHandleW(nullptr);
        MODULEINFO module_info = {};
        if (loader_module != nullptr &&
            GetModuleInformation(GetCurrentProcess(), loader_module,
                                 &module_info, sizeof(module_info)))
        {
            loader_module_base = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll));
            loader_module_size =
                static_cast<std::uint32_t>(module_info.SizeOfImage);
        }
    }
}

bool ReadHostThreadTimes(const HostThread& thread,
                         std::uint64_t* kernel_100ns,
                         std::uint64_t* user_100ns)
{
    // Task 503d-18: the loop's own question goes through the layer, but the
    // diagnostics below sample a thread with Win32 calls that take a HANDLE.
    // `HostThread::handle` is documented as being one on this host.
    // Task 503d-21: what stays fenced is `GetThreadTimes` alone. The register
    // sampling that used to be here with it moved onto the platform layer's
    // interrupt and runs on both hosts.
    auto* thread_handle = static_cast<HANDLE>(thread.handle);
    //
    // Task 503d-21: fenced on its own now. Linux reports the same split
    // in /proc/<pid>/task/<tid>/stat, but as jiffies against a
    // configurable tick rather than as 100ns units, so a counterpart
    // would be a different measurement under this one's name.
    FILETIME creation_time = {};
    FILETIME exit_time = {};
    FILETIME kernel_time = {};
    FILETIME user_time = {};
    if (GetThreadTimes(thread_handle, &creation_time, &exit_time,
                       &kernel_time, &user_time))
    {
        const auto to_100ns = [](const FILETIME& value) {
            return (static_cast<std::uint64_t>(value.dwHighDateTime)
                    << 32) |
                static_cast<std::uint64_t>(value.dwLowDateTime);
        };
        *kernel_100ns = to_100ns(kernel_time);
        *user_100ns = to_100ns(user_time);
        return true;
    }
    return false;
}

bool ReadSuspendedHostThreadContext(void* thread_handle,
                                    GuestCpuContext* context)
{
    auto* thread = static_cast<HANDLE>(thread_handle);
    if (thread == nullptr || context == nullptr)
    {
        return false;
    }

    const repiu::platform::win32::Win32ThreadApi& api =
        repiu::platform::win32::GetWin32ThreadApi();
    if (api.suspend_thread == nullptr ||
        api.get_thread_context == nullptr ||
        api.resume_thread == nullptr)
    {
        return false;
    }

    if (api.suspend_thread(thread) == static_cast<DWORD>(-1))
    {
        return false;
    }

    context->ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
    const bool read = api.get_thread_context(thread, context) != 0;
    api.resume_thread(thread);
    return read;
}

}  // namespace repiu::platform
