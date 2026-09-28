#include "repiu/platform/guest_cpu_context.h"

#if !defined(_WIN32)

#include <csignal>
#include <ucontext.h>

// Task 757. The part of the guest context conversion both Linux architectures
// share. The register and x87 conversions live in x86/ and x64/, because the
// guest context remains 32-bit on every host while the machine context it is
// read from does not.

namespace repiu::platform
{

GuestFaultInfo ReadGuestFaultInfo(const void* signal_info,
                                  const void* host_context)
{
    GuestFaultInfo info;
    if (signal_info == nullptr)
    {
        return info;
    }
    const auto* siginfo = static_cast<const siginfo_t*>(signal_info);
    info.fault_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(siginfo->si_addr));
    if (host_context != nullptr)
    {
        const auto* context = static_cast<const ucontext_t*>(host_context);
        // Bit 1 of the page-fault error code is the write flag, which is where
        // Linux keeps the direction Windows reports as ExceptionInformation[0].
        constexpr std::uint32_t kPageFaultWrite = 0x2U;
        // Bit 4 is set when the access was an instruction fetch, which is what
        // Windows reports as access kind 8.
        constexpr std::uint32_t kPageFaultInstructionFetch = 0x10U;
        const auto error = static_cast<std::uint32_t>(
            context->uc_mcontext.gregs[REG_ERR]);
        info.write_access = (error & kPageFaultWrite) != 0U;
        info.execute_access = (error & kPageFaultInstructionFetch) != 0U;
    }
    info.valid = true;
    return info;
}

}  // namespace repiu::platform

#endif
