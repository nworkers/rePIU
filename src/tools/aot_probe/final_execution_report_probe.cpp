#include "final_execution_report_probe.h"

#include "repiu/engine/execution_trampoline.h"
#include "repiu/engine/final_execution_report.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

using repiu::engine::EmitFinalExecutionReport;
using repiu::engine::FinalExecutionReportEmitted;
using repiu::engine::MinimalExecutionAttempt;
using repiu::engine::ResetFinalExecutionReport;
using repiu::engine::SetFinalExecutionReport;

struct Observation
{
    std::uint32_t calls = 0;
    bool saw_entry_address = false;
};

void Record(const MinimalExecutionAttempt& attempt, void* const user)
{
    auto* const observation = static_cast<Observation*>(user);
    if (observation == nullptr)
    {
        return;
    }
    ++observation->calls;
    // The attempt arrives by reference and is read, not copied: this is what
    // the loader's summary does with it, and a seam that handed over something
    // emptied on the way would pass a weaker check than that.
    observation->saw_entry_address = attempt.entry_address == 0xDEADBEEFU;
}

// Emitting with nothing registered is the contract for every caller that has no
// summary to print -- a probe, a tool, a host under test. It must be safe and
// must not consume the one report.
bool ProbeUnregistered()
{
    ResetFinalExecutionReport();
    MinimalExecutionAttempt attempt;
    if (EmitFinalExecutionReport(attempt) || FinalExecutionReportEmitted())
    {
        return false;
    }
    // And having refused, the seam is still armable: a `true` return above
    // would have meant the report was spent before anything could register.
    Observation observation;
    SetFinalExecutionReport(&Record, &observation);
    const bool emitted = EmitFinalExecutionReport(attempt);
    ResetFinalExecutionReport();
    return emitted && observation.calls == 1U;
}

// Reporting at most once is what lets both shutdown arms call it. The arm that
// returns normally would otherwise print a second copy of what the arm that
// exited had already printed.
bool ProbeEmitsOnce()
{
    ResetFinalExecutionReport();
    Observation observation;
    SetFinalExecutionReport(&Record, &observation);

    MinimalExecutionAttempt attempt;
    attempt.entry_address = 0xDEADBEEFU;
    const bool first = EmitFinalExecutionReport(attempt);
    const bool first_flag = FinalExecutionReportEmitted();
    const bool second = EmitFinalExecutionReport(attempt);
    const bool third = EmitFinalExecutionReport(attempt);

    const bool ok = first && first_flag && !second && !third &&
        observation.calls == 1U && observation.saw_entry_address;
    ResetFinalExecutionReport();
    return ok;
}

// Clearing the registration stops reporting without stopping the caller. A
// host that tore its logger down before the shutdown block ran would otherwise
// report through a dangling pointer.
bool ProbeClearedRegistration()
{
    ResetFinalExecutionReport();
    Observation observation;
    SetFinalExecutionReport(&Record, &observation);
    SetFinalExecutionReport(nullptr, nullptr);

    MinimalExecutionAttempt attempt;
    const bool emitted = EmitFinalExecutionReport(attempt);
    const bool ok = !emitted && observation.calls == 0U &&
        !FinalExecutionReportEmitted();
    ResetFinalExecutionReport();
    return ok;
}

// The reset itself, which every case above leans on and which exists for
// callers that run several shutdowns in one process.
bool ProbeReset()
{
    ResetFinalExecutionReport();
    Observation observation;
    SetFinalExecutionReport(&Record, &observation);
    MinimalExecutionAttempt attempt;
    if (!EmitFinalExecutionReport(attempt) || !FinalExecutionReportEmitted())
    {
        ResetFinalExecutionReport();
        return false;
    }
    ResetFinalExecutionReport();
    if (FinalExecutionReportEmitted())
    {
        return false;
    }
    // The reset clears the registration too, so a second round has to arm
    // again rather than inheriting the first round's callback.
    Observation second;
    if (EmitFinalExecutionReport(attempt))
    {
        return false;
    }
    SetFinalExecutionReport(&Record, &second);
    const bool ok = EmitFinalExecutionReport(attempt) && second.calls == 1U &&
        observation.calls == 1U;
    ResetFinalExecutionReport();
    return ok;
}

}  // namespace

bool RunFinalExecutionReportProbe()
{
    const bool unregistered_ok = ProbeUnregistered();
    const bool once_ok = ProbeEmitsOnce();
    const bool cleared_ok = ProbeClearedRegistration();
    const bool reset_ok = ProbeReset();
    const bool all =
        unregistered_ok && once_ok && cleared_ok && reset_ok;
    std::cout << "final_execution_report_unregistered="
              << (unregistered_ok ? "true" : "false")
              << "\nfinal_execution_report_emits_once="
              << (once_ok ? "true" : "false")
              << "\nfinal_execution_report_cleared="
              << (cleared_ok ? "true" : "false")
              << "\nfinal_execution_report_reset="
              << (reset_ok ? "true" : "false")
              << "\nfinal_execution_report_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
