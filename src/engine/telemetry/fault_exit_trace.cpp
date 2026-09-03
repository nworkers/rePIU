#include "fault_exit_trace.h"

#include "repiu/engine/veh_exit_site.h"
#include "repiu/platform/fault_handler.h"
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/host_error_stream.h"
#include "repiu/runtime/env_toggle.h"
#include "thread_context.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace repiu::engine
{

namespace
{

// A whole-trace limit rather than one per name. The run this exists for prints
// once and dies; a run that declines steadily would otherwise fill the log with
// the same name.
constexpr std::uint32_t kFaultExitTracePrintLimit = 16U;

std::atomic<std::uint32_t> g_fault_exit_trace_count{0};

}  // namespace

bool ResolveFaultExitTraceEnabled(const char* setting)
{
    return repiu::runtime::ResolveOptInToggle(setting);
}

bool FaultExitTraceEnabled()
{
    static const bool enabled =
        ResolveFaultExitTraceEnabled(std::getenv("REPIU_FAULT_EXIT_TRACE"));
    return enabled;
}

void RecordFaultExit(const ThreadContext* context,
                     const repiu::platform::FaultEvent& fault)
{
    if (!FaultExitTraceEnabled() || context == nullptr)
    {
        return;
    }
    const std::uint32_t occurrence =
        g_fault_exit_trace_count.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (occurrence > kFaultExitTracePrintLimit)
    {
        return;
    }
    // snprintf into a local buffer and one unbuffered write. This runs on the
    // fault path, and on the run it exists for the process is about to die, so
    // a line that is still sitting in a stdio buffer is a line that is lost.
    char line[192] = {};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[repiu-exit] site=%s eip=0x%08X code=0x%08X "
        "guest_stack=%u call_state=%u n=%u\n",
        VehExitSiteName(context->last_veh_exit_site),
        static_cast<unsigned>(fault.instruction_address),
        static_cast<unsigned>(fault.host_code),
        context->use_guest_stack ? 1U : 0U,
        context->active_call_state != nullptr ? 1U : 0U,
        occurrence);
    if (length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            line, static_cast<std::size_t>(length));
    }

    // Task 584: the register file, as a second line.
    //
    // Two lines rather than one because a single line wraps in a terminal, and
    // a wrapped `esi=` is hard to attribute to the fault above it.
    //
    // `ds`, `es` and `ss` are deliberately absent. Linux's x86-64 `mcontext_t`
    // packs only CS, GS and FS into `REG_CSGSFS` and saves none of the other
    // three, so `LoadGuestCpuContext` sets them to zero on that host. Printing
    // a synthesized zero next to a segment-shaped bug is how a reader concludes
    // "DS is zero, that explains it" from a value that was never measured.
    const repiu::platform::GuestCpuContext* registers = fault.registers;
    if (registers == nullptr)
    {
        return;
    }
    char access_text[16] = "none";
    if (fault.access.valid)
    {
        std::snprintf(access_text, sizeof(access_text), "0x%08X",
                      static_cast<unsigned>(fault.access.fault_address));
    }
    char register_line[288] = {};
    const int register_length = std::snprintf(
        register_line,
        sizeof(register_line),
        "[repiu-regs] access=%s eax=0x%08X ecx=0x%08X edx=0x%08X ebx=0x%08X "
        "esp=0x%08X ebp=0x%08X esi=0x%08X edi=0x%08X eflags=0x%08X "
        "cs=0x%04X fs=0x%04X gs=0x%04X\n",
        access_text,
        static_cast<unsigned>(registers->Eax),
        static_cast<unsigned>(registers->Ecx),
        static_cast<unsigned>(registers->Edx),
        static_cast<unsigned>(registers->Ebx),
        static_cast<unsigned>(registers->Esp),
        static_cast<unsigned>(registers->Ebp),
        static_cast<unsigned>(registers->Esi),
        static_cast<unsigned>(registers->Edi),
        static_cast<unsigned>(registers->EFlags),
        static_cast<unsigned>(registers->SegCs) & 0xFFFFU,
        static_cast<unsigned>(registers->SegFs) & 0xFFFFU,
        static_cast<unsigned>(registers->SegGs) & 0xFFFFU);
    if (register_length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            register_line, static_cast<std::size_t>(register_length));
    }
}

}  // namespace repiu::engine
