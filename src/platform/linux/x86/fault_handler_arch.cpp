#include "../fault_handler_arch.h"

// Task 757. The i386 half of the Linux fault handler. i386 has no dispatch
// registers, resumed-boundary record or data watchpoint, so most of this is the
// explicit statement that there is nothing to do. CMake builds this file only
// when pointers are four bytes.
#if !defined(__i386__)
#error "src/platform/linux/x86/ is the i386 Linux platform layer"
#endif

#include <ucontext.h>

namespace repiu::platform
{
namespace linux_fault
{

std::uintptr_t HostStackPointer(const void* host_context)
{
    const auto* context = static_cast<const ucontext_t*>(host_context);
    return static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_ESP]);
}

void HostDispatchRegisters(const void* host_context,
                           std::uint64_t* r10,
                           std::uint64_t* r14,
                           std::uint64_t* r15)
{
    (void)host_context;
    if (r10 == nullptr || r14 == nullptr || r15 == nullptr)
    {
        return;
    }
    *r10 = 0U;
    *r14 = 0U;
    *r15 = 0U;
}

bool HandleArchDiagnosticTrap(const int signal_number,
                              const siginfo_t& info,
                              void* host_context,
                              const GuestCpuContext& registers)
{
    (void)signal_number;
    (void)info;
    (void)host_context;
    (void)registers;
    return false;
}

void RecordLastResumedSignal(const int signal_number,
                             const FaultKind fault_kind,
                             const void* host_context,
                             const GuestCpuContext& registers)
{
    (void)signal_number;
    (void)fault_kind;
    (void)host_context;
    (void)registers;
}

void AppendArchFaultFields(char* line, std::size_t* length)
{
    (void)line;
    (void)length;
}

}  // namespace linux_fault

// Task 717's watchpoint reports through the x64 handler only.
bool ArmLinuxDataWatchFromEnvironment()
{
    return false;
}

}  // namespace repiu::platform
