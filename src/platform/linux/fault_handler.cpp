#include "repiu/platform/fault_handler.h"

#if !defined(_WIN32)

#include "repiu/engine/guest_write_trace.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <sys/uio.h>
#endif
#include <ucontext.h>
#include <unistd.h>

#if defined(__x86_64__)
extern "C" volatile std::uint64_t repiu_linux_x64_guest_entry_rsp;
extern "C" volatile std::uint64_t repiu_linux_x64_cache_call_rsp;
extern "C" volatile std::uint64_t repiu_linux_x64_return_thunk_rsp;
#endif

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

#if defined(__x86_64__)
// Task 673. These values describe the last signal whose callback actually
// resumed execution. The current unhandled fault is intentionally not written
// here, so the report can distinguish the last successful boundary from the
// faulting instruction itself.
volatile std::uint32_t g_last_resumed_signal = 0U;
volatile std::uint32_t g_last_resumed_fault_kind = 0U;
volatile std::uint64_t g_last_resumed_rip = 0U;
volatile std::uint64_t g_last_resumed_rsp = 0U;
volatile std::uint64_t g_last_resumed_r10 = 0U;
volatile std::uint64_t g_last_resumed_r14 = 0U;
volatile std::uint64_t g_last_resumed_r15 = 0U;
volatile std::uint32_t g_last_resumed_guest_eip = 0U;
volatile std::uint32_t g_last_resumed_guest_esp = 0U;
volatile std::uint32_t g_first_low_resumed_signal = 0U;
volatile std::uint32_t g_first_low_resumed_fault_kind = 0U;
volatile std::uint64_t g_first_low_resumed_rip = 0U;
volatile std::uint64_t g_first_low_resumed_rsp = 0U;
volatile std::uint64_t g_first_low_resumed_r10 = 0U;
volatile std::uint64_t g_first_low_resumed_r14 = 0U;
volatile std::uint64_t g_first_low_resumed_r15 = 0U;
volatile std::uint32_t g_first_low_resumed_guest_eip = 0U;
volatile std::uint32_t g_first_low_resumed_guest_esp = 0U;
#endif

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
//
// Task 549. The byte is found through the host's own instruction pointer, not
// through `registers->Eip`.
//
// They are the same number on i386 and only there. `GuestCpuContext` is a
// 32-bit contract on every host, so on x86-64 `Eip` holds the low half of RIP,
// and the `int3` this rewinds past sits at a host address well above 4 GiB.
// Subtracting one from the truncated half and dereferencing it reads an address
// that was never mapped -- a fault raised inside the fault handler, which is
// the one place it cannot be reported.
std::uintptr_t HostInstructionPointer(const void* host_context)
{
    const auto* context = static_cast<const ucontext_t*>(host_context);
#if defined(__i386__)
    return static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_EIP]);
#elif defined(__x86_64__)
    return static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_RIP]);
#else
    (void)context;
    return 0U;
#endif
}

std::uintptr_t HostStackPointer(const void* host_context)
{
    const auto* context = static_cast<const ucontext_t*>(host_context);
#if defined(__i386__)
    return static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_ESP]);
#elif defined(__x86_64__)
    return static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_RSP]);
#else
    (void)context;
    return 0U;
#endif
}

void HostDispatchRegisters(const void* host_context,
                           std::uint64_t* r10,
                           std::uint64_t* r14,
                           std::uint64_t* r15)
{
    if (r10 == nullptr || r14 == nullptr || r15 == nullptr)
    {
        return;
    }
    *r10 = 0U;
    *r14 = 0U;
    *r15 = 0U;
    const auto* context = static_cast<const ucontext_t*>(host_context);
#if defined(__x86_64__)
    *r10 = static_cast<std::uint64_t>(context->uc_mcontext.gregs[REG_R10]);
    *r14 = static_cast<std::uint64_t>(context->uc_mcontext.gregs[REG_R14]);
    *r15 = static_cast<std::uint64_t>(context->uc_mcontext.gregs[REG_R15]);
#else
    (void)context;
#endif
}

#if defined(__x86_64__)
bool LinuxX64SignalBoundaryTraceEnabled()
{
    static const bool enabled = [] {
        const char* const value = std::getenv(
            "REPIU_LINUX_X64_SIGNAL_BOUNDARY_TRACE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

void RecordLastResumedSignal(const int signal_number,
                             const FaultKind fault_kind,
                             const void* host_context,
                             const GuestCpuContext& registers)
{
    if (!LinuxX64SignalBoundaryTraceEnabled())
    {
        return;
    }
    std::uint64_t r10 = 0U;
    std::uint64_t r14 = 0U;
    std::uint64_t r15 = 0U;
    HostDispatchRegisters(host_context, &r10, &r14, &r15);
    // Publish the validity marker last. The handler is normally single-threaded
    // for this process, but this order also keeps a concurrent crash report
    // from mistaking a partially written snapshot for a complete one.
    g_last_resumed_signal = 0U;
    g_last_resumed_fault_kind = static_cast<std::uint32_t>(fault_kind);
    g_last_resumed_rip = HostInstructionPointer(host_context);
    g_last_resumed_rsp = HostStackPointer(host_context);
    g_last_resumed_r10 = r10;
    g_last_resumed_r14 = r14;
    g_last_resumed_r15 = r15;
    g_last_resumed_guest_eip = registers.Eip;
    g_last_resumed_guest_esp = registers.Esp;
    g_last_resumed_signal = static_cast<std::uint32_t>(signal_number);
    const std::uint64_t rsp = HostStackPointer(host_context);
    if (rsp <= UINT64_C(0xFFFFFFFF) && g_first_low_resumed_signal == 0U)
    {
        g_first_low_resumed_fault_kind =
            static_cast<std::uint32_t>(fault_kind);
        g_first_low_resumed_rip = HostInstructionPointer(host_context);
        g_first_low_resumed_rsp = rsp;
        g_first_low_resumed_r10 = r10;
        g_first_low_resumed_r14 = r14;
        g_first_low_resumed_r15 = r15;
        g_first_low_resumed_guest_eip = registers.Eip;
        g_first_low_resumed_guest_esp = registers.Esp;
        // Publish the first-low marker last for the same reason as the last
        // resumed marker above.
        g_first_low_resumed_signal =
            static_cast<std::uint32_t>(signal_number);
    }
}
#endif

void RewindPastBreakpoint(GuestCpuContext* registers, const void* host_context)
{
    const std::uintptr_t host_instruction = HostInstructionPointer(
        host_context);
    if (host_instruction == 0U)
    {
        return;
    }
    const std::uintptr_t candidate = host_instruction - 1U;
    if (*reinterpret_cast<const std::uint8_t*>(candidate) == 0xCCU)
    {
        // The low half is what the caller reads, and on x86-64 the upper half
        // it belongs to is restored by `StoreGuestCpuContext`, which preserves
        // what it does not own.
        registers->Eip = static_cast<std::uint32_t>(candidate);
    }
}

// Task 578. The unhandled-fault line, written with `write` alone.
//
// Nothing here may allocate, lock, or call into a logging library: this runs on
// a signal handler that is about to let the process die, and a handler that
// hangs replaces a diagnosable crash with an undiagnosable one.
void WriteHex(char* out, std::size_t* length, std::uint64_t value)
{
    out[(*length)++] = '0';
    out[(*length)++] = 'x';
    bool leading = true;
    for (int shift = 60; shift >= 0; shift -= 4)
    {
        const auto digit = static_cast<unsigned>((value >> shift) & 0xFU);
        if (leading && digit == 0U && shift != 0)
        {
            continue;
        }
        leading = false;
        out[(*length)++] = static_cast<char>(
            digit < 10U ? '0' + digit : 'a' + (digit - 10U));
    }
}

void WriteNamedHex(char* out, std::size_t* length, const char* name,
                   std::uint32_t value)
{
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        out[(*length)++] = *cursor;
    }
    WriteHex(out, length, value);
}

void WriteNamedHex64(char* out, std::size_t* length, const char* name,
                     std::uint64_t value)
{
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        out[(*length)++] = *cursor;
    }
    WriteHex(out, length, value);
}

// Task 597. Read the instruction bytes without dereferencing an arbitrary
// address from a signal handler. A data fault has already fetched the
// instruction at RIP, but an instruction-fetch fault may point at an unmapped
// page, so the latter is reported as unreadable without attempting a read.
constexpr std::size_t kFaultInstructionByteCount = 16U;

struct FaultInstructionBytes
{
    std::uint8_t bytes[kFaultInstructionByteCount] = {};
    std::size_t count = 0U;
};

FaultInstructionBytes ReadFaultInstructionBytes(
    const std::uintptr_t host_instruction_address,
    const bool execute_access)
{
    FaultInstructionBytes result;
    if (host_instruction_address == 0U || execute_access)
    {
        return result;
    }
    // `process_vm_readv` reports EFAULT through its return value instead of
    // delivering another SIGSEGV to this handler. It is used only on the
    // unhandled path, after the normal callback has declined the fault.
#if defined(__linux__) && defined(SYS_process_vm_readv)
    struct iovec local = {};
    local.iov_base = result.bytes;
    local.iov_len = sizeof(result.bytes);
    struct iovec remote = {};
    remote.iov_base = reinterpret_cast<void*>(host_instruction_address);
    remote.iov_len = sizeof(result.bytes);
    const long copied = syscall(
        SYS_process_vm_readv, static_cast<long>(getpid()), &local, 1U,
        &remote, 1U, 0U);
    if (copied > 0L)
    {
        result.count = static_cast<std::size_t>(copied) > sizeof(result.bytes)
            ? sizeof(result.bytes)
            : static_cast<std::size_t>(copied);
    }
#endif
    return result;
}

// Task 597. On Linux, GuestCpuContext::Esp is the guest stack pointer carried
// by the x64 AOT frame. Keep the two dwords immediately before it in the
// report: after `pop es` and `pop ebx`, the second one is the value that fed
// the original `pop ebx`.
constexpr std::size_t kFaultGuestStackWordCount = 4U;

struct FaultGuestStackWords
{
    std::uint32_t base = 0U;
    std::uint32_t words[kFaultGuestStackWordCount] = {};
    std::size_t count = 0U;
};

FaultGuestStackWords ReadFaultGuestStackWords(
    const std::uint32_t guest_esp)
{
    FaultGuestStackWords result;
    if (guest_esp < 8U)
    {
        return result;
    }
    result.base = guest_esp - 8U;
#if defined(__linux__) && defined(SYS_process_vm_readv)
    struct iovec local = {};
    local.iov_base = result.words;
    local.iov_len = sizeof(result.words);
    struct iovec remote = {};
    remote.iov_base = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(result.base));
    remote.iov_len = sizeof(result.words);
    const long copied = syscall(
        SYS_process_vm_readv, static_cast<long>(getpid()), &local, 1U,
        &remote, 1U, 0U);
    if (copied > 0L)
    {
        result.count = static_cast<std::size_t>(copied) /
            sizeof(result.words[0]);
        if (result.count > kFaultGuestStackWordCount)
        {
            result.count = kFaultGuestStackWordCount;
        }
    }
#endif
    return result;
}

void WriteByteHex(char* out, std::size_t* length, const std::uint8_t value)
{
    const char digits[] = "0123456789abcdef";
    out[(*length)++] = digits[(value >> 4U) & 0x0FU];
    out[(*length)++] = digits[value & 0x0FU];
}

void WriteFaultInstructionBytes(char* out, std::size_t* length,
                                const std::uintptr_t host_instruction_address,
                                const bool execute_access)
{
    const char prefix[] = " bytes=";
    for (std::size_t index = 0; index + 1U < sizeof(prefix); ++index)
    {
        out[(*length)++] = prefix[index];
    }
    const FaultInstructionBytes bytes = ReadFaultInstructionBytes(
        host_instruction_address, execute_access);
    if (bytes.count == 0U)
    {
        const char unavailable[] = "<unreadable>";
        for (std::size_t index = 0; index + 1U < sizeof(unavailable); ++index)
        {
            out[(*length)++] = unavailable[index];
        }
        return;
    }
    for (std::size_t index = 0; index < bytes.count; ++index)
    {
        if (index != 0U)
        {
            out[(*length)++] = ' ';
        }
        WriteByteHex(out, length, bytes.bytes[index]);
    }
}

void WriteFaultGuestStack(char* out, std::size_t* length,
                          const std::uint32_t guest_esp)
{
    const FaultGuestStackWords stack = ReadFaultGuestStackWords(guest_esp);
    if (stack.count == 0U)
    {
        const char unavailable[] = " guest_stack=<unreadable>";
        for (std::size_t index = 0; index + 1U < sizeof(unavailable); ++index)
        {
            out[(*length)++] = unavailable[index];
        }
        return;
    }
    static constexpr const char* kNames[kFaultGuestStackWordCount] = {
        " guest_stack_m8=", " guest_stack_m4=", " guest_stack_0=",
        " guest_stack_p4="};
    for (std::size_t index = 0; index < stack.count; ++index)
    {
        WriteNamedHex(out, length, kNames[index], stack.words[index]);
    }
}

void ReportUnhandledFault(const int signal_number,
                          const std::uintptr_t host_instruction_address,
                          const std::uintptr_t host_stack_pointer,
                          const std::uint64_t host_r10,
                          const std::uint64_t host_r14,
                          const std::uint64_t host_r15,
                          const std::uint32_t instruction_address,
                          const std::uint32_t access_address,
                          const bool execute_access,
                          const GuestCpuContext& registers)
{
    char line[1024];
    std::size_t length = 0;
    const char prefix[] = "[repiu-fault] unhandled signal=";
    for (std::size_t index = 0; index + 1U < sizeof(prefix); ++index)
    {
        line[length++] = prefix[index];
    }
    WriteHex(line, &length, static_cast<std::uint64_t>(signal_number));
    const char rip_text[] = " rip=";
    for (std::size_t index = 0; index + 1U < sizeof(rip_text); ++index)
    {
        line[length++] = rip_text[index];
    }
    WriteHex(line, &length, host_instruction_address);
    const char rsp_text[] = " rsp=";
    for (std::size_t index = 0; index + 1U < sizeof(rsp_text); ++index)
    {
        line[length++] = rsp_text[index];
    }
    WriteHex(line, &length, host_stack_pointer);
#if defined(__x86_64__)
    WriteNamedHex64(line, &length, " entry_rsp=",
                    repiu_linux_x64_guest_entry_rsp);
    WriteNamedHex64(line, &length, " cache_rsp=",
                    repiu_linux_x64_cache_call_rsp);
    WriteNamedHex64(line, &length, " thunk_rsp=",
                    repiu_linux_x64_return_thunk_rsp);
    WriteNamedHex64(line, &length, " last_signal=",
                    g_last_resumed_signal);
    WriteNamedHex64(line, &length, " last_kind=",
                    g_last_resumed_fault_kind);
    WriteNamedHex64(line, &length, " last_rip=", g_last_resumed_rip);
    WriteNamedHex64(line, &length, " last_rsp=", g_last_resumed_rsp);
    WriteNamedHex64(line, &length, " last_r10=", g_last_resumed_r10);
    WriteNamedHex64(line, &length, " last_r14=", g_last_resumed_r14);
    WriteNamedHex64(line, &length, " last_r15=", g_last_resumed_r15);
    WriteNamedHex64(line, &length, " last_eip=", g_last_resumed_guest_eip);
    WriteNamedHex64(line, &length, " last_esp=", g_last_resumed_guest_esp);
    WriteNamedHex64(line, &length, " first_low_signal=",
                    g_first_low_resumed_signal);
    WriteNamedHex64(line, &length, " first_low_kind=",
                    g_first_low_resumed_fault_kind);
    WriteNamedHex64(line, &length, " first_low_rip=", g_first_low_resumed_rip);
    WriteNamedHex64(line, &length, " first_low_rsp=", g_first_low_resumed_rsp);
    WriteNamedHex64(line, &length, " first_low_r10=", g_first_low_resumed_r10);
    WriteNamedHex64(line, &length, " first_low_r14=", g_first_low_resumed_r14);
    WriteNamedHex64(line, &length, " first_low_r15=", g_first_low_resumed_r15);
    WriteNamedHex64(line, &length, " first_low_eip=",
                    g_first_low_resumed_guest_eip);
    WriteNamedHex64(line, &length, " first_low_esp=",
                    g_first_low_resumed_guest_esp);
#endif
    WriteNamedHex64(line, &length, " r10=", host_r10);
    WriteNamedHex64(line, &length, " r14=", host_r14);
    WriteNamedHex64(line, &length, " r15=", host_r15);
    const char eip_text[] = " eip=";
    for (std::size_t index = 0; index + 1U < sizeof(eip_text); ++index)
    {
        line[length++] = eip_text[index];
    }
    WriteHex(line, &length, instruction_address);
    const char access_text[] = " access=";
    for (std::size_t index = 0; index + 1U < sizeof(access_text); ++index)
    {
        line[length++] = access_text[index];
    }
    WriteHex(line, &length, access_address);
    WriteFaultInstructionBytes(line, &length, host_instruction_address,
                               execute_access);
    WriteFaultGuestStack(line, &length, registers.Esp);
    WriteNamedHex(line, &length, " eax=", registers.Eax);
    WriteNamedHex(line, &length, " ebx=", registers.Ebx);
    WriteNamedHex(line, &length, " ecx=", registers.Ecx);
    WriteNamedHex(line, &length, " edx=", registers.Edx);
    WriteNamedHex(line, &length, " esi=", registers.Esi);
    WriteNamedHex(line, &length, " edi=", registers.Edi);
    WriteNamedHex(line, &length, " ebp=", registers.Ebp);
    WriteNamedHex(line, &length, " esp=", registers.Esp);
    WriteNamedHex(line, &length, " eflags=", registers.EFlags);
    line[length++] = static_cast<char>(10);  // newline
    const ssize_t written = write(2, line, length);
    (void)written;
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
        RewindPastBreakpoint(&registers, host_context);
    }
    // Linux has nothing that corresponds; Eip is the faulting instruction for
    // every fault this handler takes, and for a breakpoint that is after the
    // rewind above.
    event.instruction_address = registers.Eip;
    event.registers = &registers;

    if (g_callback(&event, g_user_data) != FaultDisposition::kResume)
    {
        // Tasks 578 and 597. Say where and which host instruction faulted,
        // before dying.
        //
        // Windows has an unhandled-exception filter that reports this; Linux
        // had nothing, so an unhandled guest fault ended as a bare exit 139
        // with the address in a core dump nobody could read without a debugger.
        // Bringing up x64 execution is exactly the situation that needs it.
        //
        // Async-signal-safe: direct system-call read plus `write`, no
        // formatting library, and all values rendered by hand into a stack
        // buffer.
        std::uint64_t host_r10 = 0U;
        std::uint64_t host_r14 = 0U;
        std::uint64_t host_r15 = 0U;
        HostDispatchRegisters(host_context, &host_r10, &host_r14, &host_r15);
        ReportUnhandledFault(signal_number,
                             HostInstructionPointer(host_context),
                             HostStackPointer(host_context),
                             host_r10,
                             host_r14,
                             host_r15,
                             registers.Eip,
                             event.access.fault_address,
                             event.access.execute_access,
                             registers);
        repiu::engine::DumpGuestWriteTraceTail(2);
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

    // Task 673. Capture the pre-resume native boundary only after the callback
    // accepts the event. The current fault is therefore excluded from the
    // snapshot printed above.
#if defined(__x86_64__)
    RecordLastResumedSignal(signal_number, event.kind, host_context, registers);
#endif

    // Writing the registers back is what makes the return a resume: the kernel
    // restores from this context, so an edited Eip or EFlags takes effect.
    StoreGuestCpuContext(registers, host_context);
    if (event.host_resume_address != 0U)
    {
        StoreHostInstructionPointer(event.host_resume_address, host_context);
    }
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
