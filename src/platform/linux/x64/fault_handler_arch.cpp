#include "../fault_handler_arch.h"

// Task 757. The x86-64 half of the Linux fault handler: the host registers the
// report reads, the Task 673 resumed-boundary record, and the Task 717 data
// watchpoint. CMake builds this file only when pointers are eight bytes.
#if !defined(__x86_64__)
#error "src/platform/linux/x64/ is the x86-64 Linux platform layer"
#endif

#include "../fault_report_writer.h"
#include "repiu/platform/guest_cpu_context.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <ucontext.h>
#include <unistd.h>

extern "C" volatile std::uint64_t repiu_linux_x64_guest_entry_rsp;
extern "C" volatile std::uint64_t repiu_linux_x64_cache_call_rsp;
extern "C" volatile std::uint64_t repiu_linux_x64_return_thunk_rsp;
extern "C" volatile std::uint32_t repiu_linux_x64_guest_esp_trace_site;
extern "C" volatile std::uint32_t repiu_linux_x64_guest_esp_trace_value;

namespace repiu::platform
{
namespace linux_fault
{
namespace
{

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

bool LinuxX64SignalBoundaryTraceEnabled()
{
    static const bool enabled = [] {
        const char* const value = std::getenv(
            "REPIU_LINUX_X64_SIGNAL_BOUNDARY_TRACE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}


// Task 717. A hardware write watchpoint on one guest address, reported and
// resumed. The kernel delivers it as SIGTRAP with si_code TRAP_PERF, after the
// write, with RIP on the next instruction -- so the report names the writing
// host instruction by the bytes just before RIP, and the guest registers as
// they stand after it.
constexpr int kTrapPerf = 6;  // TRAP_PERF, absent from older headers
constexpr std::uint32_t kDataWatchPrintLimit = 16U;
volatile std::uint32_t g_data_watch_hits = 0U;

bool IsDataWatchTrap(const int signal_number, const siginfo_t& info)
{
    return signal_number == SIGTRAP && info.si_code == kTrapPerf;
}

void ReportDataWatchHit(void* host_context, const GuestCpuContext& registers)
{
    const std::uint32_t hit = ++g_data_watch_hits;
    if (hit > kDataWatchPrintLimit)
    {
        return;
    }
    char line[512];
    std::size_t length = 0;
    const char prefix[] = "[repiu-data-watch] hit=";
    for (std::size_t index = 0; index + 1U < sizeof(prefix); ++index)
    {
        line[length++] = prefix[index];
    }
    WriteHex(line, &length, hit);
    const std::uintptr_t rip = ReadHostInstructionPointer(host_context);
    WriteNamedHex64(line, &length, " rip=", rip);
    // Sixteen bytes before RIP, read without faulting.
    std::uint8_t before[16] = {};
    std::size_t before_count = 0U;
    if (rip >= sizeof(before))
    {
        struct iovec local = {before, sizeof(before)};
        struct iovec remote = {reinterpret_cast<void*>(rip - sizeof(before)),
                               sizeof(before)};
        const long copied = syscall(SYS_process_vm_readv,
                                    static_cast<long>(getpid()), &local, 1U,
                                    &remote, 1U, 0U);
        before_count = copied > 0L ? static_cast<std::size_t>(copied) : 0U;
    }
    const char bytes_text[] = " before=";
    for (std::size_t index = 0; index + 1U < sizeof(bytes_text); ++index)
    {
        line[length++] = bytes_text[index];
    }
    for (std::size_t index = 0; index < before_count; ++index)
    {
        WriteByteHex(line, &length, before[index]);
    }
    std::uint64_t host_r10 = 0U;
    std::uint64_t host_r14 = 0U;
    std::uint64_t host_r15 = 0U;
    HostDispatchRegisters(host_context, &host_r10, &host_r14, &host_r15);
    WriteNamedHex64(line, &length, " r15=", host_r15);
    WriteNamedHex(line, &length, " eax=", registers.Eax);
    WriteNamedHex(line, &length, " ebx=", registers.Ebx);
    WriteNamedHex(line, &length, " ecx=", registers.Ecx);
    WriteNamedHex(line, &length, " edx=", registers.Edx);
    WriteNamedHex(line, &length, " esi=", registers.Esi);
    WriteNamedHex(line, &length, " edi=", registers.Edi);
    WriteNamedHex(line, &length, " ebp=", registers.Ebp);
    line[length++] = static_cast<char>(10);  // newline
    const ssize_t written = write(2, line, length);
    (void)written;
    if (host_r15 != 0U)
    {
        WriteFaultGuestStackDump(static_cast<std::uint32_t>(host_r15));
    }
}

}  // namespace

std::uintptr_t HostStackPointer(const void* host_context)
{
    const auto* context = static_cast<const ucontext_t*>(host_context);
    return static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_RSP]);
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
    const auto* context = static_cast<const ucontext_t*>(host_context);
    *r10 = static_cast<std::uint64_t>(context->uc_mcontext.gregs[REG_R10]);
    *r14 = static_cast<std::uint64_t>(context->uc_mcontext.gregs[REG_R14]);
    *r15 = static_cast<std::uint64_t>(context->uc_mcontext.gregs[REG_R15]);
}

bool HandleArchDiagnosticTrap(const int signal_number,
                              const siginfo_t& info,
                              void* host_context,
                              const GuestCpuContext& registers)
{
    if (!IsDataWatchTrap(signal_number, info))
    {
        return false;
    }
    ReportDataWatchHit(host_context, registers);
    return true;
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
    g_last_resumed_rip = ReadHostInstructionPointer(host_context);
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
        g_first_low_resumed_rip = ReadHostInstructionPointer(host_context);
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

void AppendArchFaultFields(char* line, std::size_t* length)
{
    WriteNamedHex64(line, length, " entry_rsp=",
                    repiu_linux_x64_guest_entry_rsp);
    WriteNamedHex64(line, length, " cache_rsp=",
                    repiu_linux_x64_cache_call_rsp);
    WriteNamedHex64(line, length, " thunk_rsp=",
                    repiu_linux_x64_return_thunk_rsp);
    WriteNamedHex64(line, length, " trace_site=",
                    repiu_linux_x64_guest_esp_trace_site);
    WriteNamedHex64(line, length, " trace_esp=",
                    repiu_linux_x64_guest_esp_trace_value);
    WriteNamedHex64(line, length, " last_signal=",
                    g_last_resumed_signal);
    WriteNamedHex64(line, length, " last_kind=",
                    g_last_resumed_fault_kind);
    WriteNamedHex64(line, length, " last_rip=", g_last_resumed_rip);
    WriteNamedHex64(line, length, " last_rsp=", g_last_resumed_rsp);
    WriteNamedHex64(line, length, " last_r10=", g_last_resumed_r10);
    WriteNamedHex64(line, length, " last_r14=", g_last_resumed_r14);
    WriteNamedHex64(line, length, " last_r15=", g_last_resumed_r15);
    WriteNamedHex64(line, length, " last_exit_site=",
                    repiu_last_veh_exit_site);
    WriteNamedHex64(line, length, " last_exit_eip=",
                    repiu_last_veh_exit_eip);
    WriteNamedHex64(line, length, " last_eip=", g_last_resumed_guest_eip);
    WriteNamedHex64(line, length, " last_esp=", g_last_resumed_guest_esp);
    WriteNamedHex64(line, length, " first_low_signal=",
                    g_first_low_resumed_signal);
    WriteNamedHex64(line, length, " first_low_kind=",
                    g_first_low_resumed_fault_kind);
    WriteNamedHex64(line, length, " first_low_rip=", g_first_low_resumed_rip);
    WriteNamedHex64(line, length, " first_low_rsp=", g_first_low_resumed_rsp);
    WriteNamedHex64(line, length, " first_low_r10=", g_first_low_resumed_r10);
    WriteNamedHex64(line, length, " first_low_r14=", g_first_low_resumed_r14);
    WriteNamedHex64(line, length, " first_low_r15=", g_first_low_resumed_r15);
    WriteNamedHex64(line, length, " first_low_eip=",
                    g_first_low_resumed_guest_eip);
    WriteNamedHex64(line, length, " first_low_esp=",
                    g_first_low_resumed_guest_esp);
}

}  // namespace linux_fault

// Task 717. REPIU_LINUX_X64_DATA_WATCH=<address>: a four-byte hardware write
// watchpoint on the calling thread, reported by the handler above. Diagnostic
// only; nothing is armed when the variable is unset.
bool ArmLinuxDataWatchFromEnvironment()
{
    const char* const text = std::getenv("REPIU_LINUX_X64_DATA_WATCH");
    if (text == nullptr || *text == 0)
    {
        return false;
    }
    const unsigned long address = std::strtoul(text, nullptr, 0);
    if (address == 0UL)
    {
        return false;
    }
    struct perf_event_attr attr = {};
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.size = sizeof(attr);
    attr.bp_type = HW_BREAKPOINT_W;
    attr.bp_addr = address;
    attr.bp_len = HW_BREAKPOINT_LEN_4;
    attr.sample_period = 1U;
    attr.exclude_kernel = 1U;
    attr.exclude_hv = 1U;
    attr.sigtrap = 1U;
    attr.remove_on_exec = 1U;
    const long fd = syscall(SYS_perf_event_open, &attr, 0, -1, -1,
                            PERF_FLAG_FD_CLOEXEC);
    char line[128];
    const int length = std::snprintf(
        line, sizeof(line), "[repiu-data-watch] armed address=0x%08lX fd=%ld\n",
        address, fd);
    if (length > 0)
    {
        const ssize_t written = write(2, line, static_cast<std::size_t>(length));
        (void)written;
    }
    return fd >= 0;
}

}  // namespace repiu::platform
