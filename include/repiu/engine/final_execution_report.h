#ifndef REPIU_ENGINE_FINAL_EXECUTION_REPORT_H_
#define REPIU_ENGINE_FINAL_EXECUTION_REPORT_H_

// Task 709. The last chance to report a run that cannot return.
//
// The shutdown block has two arms. One recovers the guest thread, cleans up and
// returns `MinimalExecutionAttempt` to the loader, which prints the summary --
// the DOS path and file-I/O traces, the Glide ordinal counts, everything a run
// is read by afterwards. The other refuses recovery and ends in `_Exit`,
// because Tasks 507 and 508 established that removing the fault handler under a
// still-running guest thread lets the kernel's default disposition dump core.
//
// `_Exit` never returns the attempt, so on that arm the loader is never given
// anything to print. Linux x64 takes it on every run, which is why the same
// 30-second `pumpit2a` writes 945 lines on Win32 and 133 on Linux, and why a
// question that needs the DOS file traces cannot be asked there at all.
//
// This is the seam that closes that. The loader registers how to report; the
// shutdown block calls it before leaving, on whichever arm it takes. What the
// refused arm may safely put into the attempt first is a separate question, and
// the answer is in the trampoline beside the call: scalars yes, the strings the
// guest thread owns no.
//
// A function pointer and a `void*`, not `std::function`: the refused arm runs
// beside a live guest thread and is no place to introduce an allocation, and
// threading a parameter through would mean lengthening three existing
// positional parameter lists that are already long.

namespace repiu::engine
{

struct MinimalExecutionAttempt;

using FinalExecutionReportCallback =
    void (*)(const MinimalExecutionAttempt& attempt, void* user);

// Registers how to report. A null callback clears the registration. The
// registration is process-wide because its one caller is a shutdown path that
// cannot be handed a parameter.
void SetFinalExecutionReport(FinalExecutionReportCallback callback, void* user);

// Reports once. Returns whether this call was the one that reported.
//
// Doing nothing when nothing is registered is the contract, not a fallback: a
// probe, a tool, or a host that has no summary to print calls this on the same
// paths and must not be made to care.
//
// Reporting at most once is what lets both shutdown arms call it. The arm that
// returns normally would otherwise print a second copy of what the refused arm
// had already printed.
bool EmitFinalExecutionReport(const MinimalExecutionAttempt& attempt);

// Whether a report has already gone out.
bool FinalExecutionReportEmitted();

// Forgets both the registration and the fact that a report went out. For tests
// and probes, which run several cases in one process.
void ResetFinalExecutionReport();

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_FINAL_EXECUTION_REPORT_H_
