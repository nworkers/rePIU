#include "repiu/platform/fault_handler.h"

#if !defined(_WIN32)

#include <csignal>
#include <cstring>
#include <pthread.h>
#include <ucontext.h>

namespace repiu::platform
{
namespace
{

FaultCallback g_callback = nullptr;
void* g_user_data = nullptr;
bool g_installed = false;

// The signals that carry the events this engine cares about. SIGBUS joins
// SIGSEGV because an access that cannot be performed is the same event to a
// caller, whichever of the two the kernel picks.
constexpr int kHandledSignals[] = {SIGSEGV, SIGBUS, SIGTRAP, SIGILL, SIGFPE};
constexpr std::size_t kHandledSignalCount =
    sizeof(kHandledSignals) / sizeof(kHandledSignals[0]);

struct sigaction g_previous[kHandledSignalCount];
stack_t g_previous_stack;
bool g_replaced_stack = false;

// The handler must be able to run when the guest stack is damaged or being
// switched, so it gets its own. SIGSTKSZ is not a constant expression on newer
// glibc, hence a fixed size chosen to exceed it comfortably.
constexpr std::size_t kAlternateStackBytes = 256U * 1024U;
alignas(16) char g_alternate_stack[kAlternateStackBytes];

FaultKind ClassifySignal(const int signal_number, const siginfo_t& info)
{
    switch (signal_number)
    {
        case SIGSEGV:
        case SIGBUS:
            return FaultKind::kAccessViolation;
        case SIGTRAP:
            // TRAP_TRACE is the trap flag firing after one instruction.
            // Everything else on SIGTRAP is a breakpoint: an int3 reports
            // TRAP_BRKPT on some kernels and SI_KERNEL on others, and telling
            // those two apart would be a distinction without a difference.
            return info.si_code == TRAP_TRACE ? FaultKind::kSingleStep
                                              : FaultKind::kBreakpoint;
        case SIGILL:
            // A privileged instruction from user space is ILL_PRVOPC; anything
            // else illegal is an illegal instruction.
            return info.si_code == ILL_PRVOPC
                ? FaultKind::kPrivilegedInstruction
                : FaultKind::kIllegalInstruction;
        case SIGFPE:
            return info.si_code == FPE_INTDIV
                ? FaultKind::kIntegerDivideByZero
                : FaultKind::kOther;
        default:
            return FaultKind::kOther;
    }
}

// The one place the two hosts genuinely disagree about registers.
//
// `int3` is a trap, so the processor saves the address of the following
// instruction, and that is what Linux reports. Windows rewinds it, so a handler
// there sees Eip on the `int3` byte itself -- which is the convention the engine
// is written against: it reads the byte at Eip to decide what boundary it hit.
//
// Presenting the same convention on both hosts is the whole purpose of this
// layer, so Linux rewinds too. The rule is then identical everywhere: Eip names
// the `int3`, and a handler that wants to continue past it advances Eip itself.
//
// Only when the preceding byte really is `0xCC`. A SIGTRAP from anywhere else --
// `raise`, a debugger -- must not have its Eip moved. Reading that byte needs no
// guard: it is the instruction that just executed, so it is mapped.
void RewindPastBreakpoint(GuestCpuContext* registers)
{
    if (registers->Eip == 0U)
    {
        return;
    }
    const auto candidate =
        static_cast<std::uintptr_t>(registers->Eip) - 1U;
    if (*reinterpret_cast<const std::uint8_t*>(candidate) == 0xCCU)
    {
        registers->Eip = static_cast<std::uint32_t>(candidate);
    }
}

void SignalHandler(int signal_number, siginfo_t* info, void* host_context)
{
    if (g_callback == nullptr || info == nullptr || host_context == nullptr)
    {
        return;
    }

    GuestCpuContext registers;
    if (!LoadGuestCpuContext(host_context, &registers))
    {
        // The machine context was not the shape this build expects. Resuming
        // from registers that were never read would be worse than dying here.
        return;
    }

    FaultEvent event;
    event.kind = ClassifySignal(signal_number, *info);
    event.host_code = static_cast<std::uint32_t>(signal_number);
    if (event.kind == FaultKind::kAccessViolation)
    {
        event.access = ReadGuestFaultInfo(info, host_context);
    }
    else if (event.kind == FaultKind::kBreakpoint)
    {
        RewindPastBreakpoint(&registers);
    }
    // Linux has nothing that corresponds; Eip is the faulting instruction for
    // every fault this handler takes, and for a breakpoint that is after the
    // rewind above.
    event.instruction_address = registers.Eip;
    event.registers = &registers;

    if (g_callback(&event, g_user_data) != FaultDisposition::kResume)
    {
        // Restoring the default and returning lets the fault happen again with
        // nothing to catch it, which is how an unhandled fault should end:
        // returning resumes at the faulting instruction, because this path
        // deliberately does not write the registers back.
        //
        // The unblock is not what the comment here used to claim. It said the
        // signal "is masked for the duration of this handler" -- it is not,
        // because `InstallFaultHandler` sets SA_NODEFER, which is exactly the
        // flag that turns that masking off. Under this handler's own flags the
        // call below is a no-op.
        //
        // It is kept, and this is why: it costs one syscall on a path that is
        // about to end the process, and it guards the worst failure available
        // here. If the signal ever is blocked -- an embedder that masked it, or
        // a later change to those flags -- the default action cannot be taken,
        // and "die with a core dump" silently becomes "hang forever".
        // Guaranteeing the mask rather than assuming it is cheap insurance
        // against the one outcome nobody can debug.
        //
        // `pthread_sigmask` rather than `sigprocmask`: this process is
        // multithreaded, and POSIX leaves `sigprocmask` unspecified there. Both
        // are async-signal-safe.
        struct sigaction fallback = {};
        fallback.sa_handler = SIG_DFL;
        sigemptyset(&fallback.sa_mask);
        sigaction(signal_number, &fallback, nullptr);
        sigset_t unblock;
        sigemptyset(&unblock);
        sigaddset(&unblock, signal_number);
        pthread_sigmask(SIG_UNBLOCK, &unblock, nullptr);
        return;
    }

    // Writing the registers back is what makes the return a resume: the kernel
    // restores from this context, so an edited Eip or EFlags takes effect.
    StoreGuestCpuContext(registers, host_context);
}

}  // namespace

bool InstallFaultHandler(FaultCallback callback, void* user_data)
{
    if (callback == nullptr || g_installed)
    {
        return false;
    }

    stack_t alternate = {};
    alternate.ss_sp = g_alternate_stack;
    alternate.ss_size = sizeof(g_alternate_stack);
    alternate.ss_flags = 0;
    g_replaced_stack = sigaltstack(&alternate, &g_previous_stack) == 0;
    if (!g_replaced_stack)
    {
        return false;
    }

    g_callback = callback;
    g_user_data = user_data;

    struct sigaction action = {};
    action.sa_sigaction = &SignalHandler;
    // SA_ONSTACK puts the handler on the alternate stack. SA_NODEFER lets a
    // fault raised while handling one be delivered rather than turning the
    // process into a silent hang -- the engine plants breakpoints and single
    // steps from inside this handler, so nesting is normal here.
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&action.sa_mask);

    for (std::size_t index = 0; index < kHandledSignalCount; ++index)
    {
        if (sigaction(kHandledSignals[index], &action, &g_previous[index]) != 0)
        {
            // Undo the ones already replaced, so a partial install does not
            // leave some signals pointing at a handler with no callback.
            for (std::size_t undo = 0; undo < index; ++undo)
            {
                sigaction(kHandledSignals[undo], &g_previous[undo], nullptr);
            }
            sigaltstack(&g_previous_stack, nullptr);
            g_replaced_stack = false;
            g_callback = nullptr;
            g_user_data = nullptr;
            return false;
        }
    }

    g_installed = true;
    return true;
}

bool RemoveFaultHandler()
{
    if (!g_installed)
    {
        return false;
    }
    bool restored = true;
    for (std::size_t index = 0; index < kHandledSignalCount; ++index)
    {
        restored =
            sigaction(kHandledSignals[index], &g_previous[index], nullptr) == 0 &&
            restored;
    }
    if (g_replaced_stack)
    {
        restored = sigaltstack(&g_previous_stack, nullptr) == 0 && restored;
        g_replaced_stack = false;
    }
    g_installed = false;
    g_callback = nullptr;
    g_user_data = nullptr;
    return restored;
}

}  // namespace repiu::platform

#endif
