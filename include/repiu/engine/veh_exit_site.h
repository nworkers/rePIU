#pragma once

// Task 410: names for the points at which the vectored exception handler hands
// an exception back to the guest. Tasks 407-409 settled *what* the exception
// before an arena entry is; they could not say *who consumed it*, and reading
// the code produced a contradiction rather than an answer (see
// docs/design/20260803-410-veh-exit-site-attribution.md section 3). A name per
// exit turns that question into a measurement.
//
// Values are stable identifiers written into ThreadContext and printed by the
// host report, so entries are appended rather than reordered. `kUnknown` is
// zero on purpose: an exit that was never tagged reports itself.
//
// Public rather than internal to the execution module because the host report
// prints these names beside the port I/O census, the same arrangement as
// out_of_arena_step_census.h.

#include <cstdint>

namespace repiu::engine
{

enum class VehExitSite : std::uint8_t
{
    kUnknown = 0,
    // Pre-chain consumers, in the order the handler reaches them.
    kZeroEip,
    kCallStepProbe,
    kNativeRegionReturn,
    kNativeRegionSensitive,
    kAotWriteCompletion,
    kAotWriteFault,
    kAotTimerSafePoint,
    // HandleAotReentry, split by which of its outcomes resumed the guest. The
    // boundary and fallthrough cases return false and therefore only mark the
    // state they left; a later site overwrites the tag if it consumes the
    // exception, which is what makes "who finished it" unambiguous.
    kAotReentryCacheAddress,
    kAotReentryResolved,
    kAotReentryRetiredResolved,
    kAotReentryQuarantined,
    kAotReentryBoundary,
    kAotReentryLegacyFallback,
    kAotIndirectTransfer,
    kAotConditionalTransfer,
    kAotReturnTransfer,
    // Task 376's discard: a single step whose EIP is outside the arena.
    kOutOfArenaStepDiscard,
    kDebugPrintDiscard,
    // Boundary gates.
    kGlideGateBoundary,
    kTimerChainBoundary,
    kLinexeFarTransferBoundary,
    // HandleSingleStepTrace, by outcome.
    kSingleStepTraceHleResumed,
    kSingleStepTraceHleStepped,
    kSingleStepTraceTimerInjected,
    kSingleStepTraceNativeEntry,
    kSingleStepTraceStepped,
    // The sequential HLE chain below the single-step path.
    kHleChainPrivileged,
    kHleChainPortIo,
    kHleChainTracedDos,
    kHleChainSegment,
    kHleChainDosHle,
    kHleChainAccessViolation,
    kHleChainStringOp,
    // Terminal and pass-through paths.
    kFatalBreakpoint,
    kReentryBreakpointPassThrough,
    kConsoleOutputExit,
    kUnreadableDecodeWindow,
    kUnhandledRecover,
    kTerminalFailureSearch,
    kContinueSearch,
};

inline constexpr std::uint32_t kVehExitSiteCount =
    static_cast<std::uint32_t>(VehExitSite::kContinueSearch) + 1U;

// Short, stable names for the host report. Kept adjacent to the enumeration so
// a new value without a name is a compile-time-visible omission rather than a
// silent blank column.
inline const char* VehExitSiteName(std::uint32_t site)
{
    switch (static_cast<VehExitSite>(site))
    {
        case VehExitSite::kUnknown: return "unknown";
        case VehExitSite::kZeroEip: return "zero-eip";
        case VehExitSite::kCallStepProbe: return "call-step-probe";
        case VehExitSite::kNativeRegionReturn: return "native-region-return";
        case VehExitSite::kNativeRegionSensitive: return "native-region-sensitive";
        case VehExitSite::kAotWriteCompletion: return "aot-write-completion";
        case VehExitSite::kAotWriteFault: return "aot-write-fault";
        case VehExitSite::kAotTimerSafePoint: return "aot-timer-safe-point";
        case VehExitSite::kAotReentryCacheAddress: return "aot-reentry-cache-address";
        case VehExitSite::kAotReentryResolved: return "aot-reentry-resolved";
        case VehExitSite::kAotReentryRetiredResolved: return "aot-reentry-retired-resolved";
        case VehExitSite::kAotReentryQuarantined: return "aot-reentry-quarantined";
        case VehExitSite::kAotReentryBoundary: return "aot-reentry-boundary";
        case VehExitSite::kAotReentryLegacyFallback: return "aot-reentry-legacy-fallback";
        case VehExitSite::kAotIndirectTransfer: return "aot-indirect-transfer";
        case VehExitSite::kAotConditionalTransfer: return "aot-conditional-transfer";
        case VehExitSite::kAotReturnTransfer: return "aot-return-transfer";
        case VehExitSite::kOutOfArenaStepDiscard: return "out-of-arena-step-discard";
        case VehExitSite::kDebugPrintDiscard: return "debug-print-discard";
        case VehExitSite::kGlideGateBoundary: return "glide-gate-boundary";
        case VehExitSite::kTimerChainBoundary: return "timer-chain-boundary";
        case VehExitSite::kLinexeFarTransferBoundary: return "linexe-far-transfer-boundary";
        case VehExitSite::kSingleStepTraceHleResumed: return "step-trace-hle-resumed";
        case VehExitSite::kSingleStepTraceHleStepped: return "step-trace-hle-stepped";
        case VehExitSite::kSingleStepTraceTimerInjected: return "step-trace-timer-injected";
        case VehExitSite::kSingleStepTraceNativeEntry: return "step-trace-native-entry";
        case VehExitSite::kSingleStepTraceStepped: return "step-trace-stepped";
        case VehExitSite::kHleChainPrivileged: return "hle-chain-privileged";
        case VehExitSite::kHleChainPortIo: return "hle-chain-port-io";
        case VehExitSite::kHleChainTracedDos: return "hle-chain-traced-dos";
        case VehExitSite::kHleChainSegment: return "hle-chain-segment";
        case VehExitSite::kHleChainDosHle: return "hle-chain-dos-hle";
        case VehExitSite::kHleChainAccessViolation: return "hle-chain-access-violation";
        case VehExitSite::kHleChainStringOp: return "hle-chain-string-op";
        case VehExitSite::kFatalBreakpoint: return "fatal-breakpoint";
        case VehExitSite::kReentryBreakpointPassThrough: return "reentry-bp-pass-through";
        case VehExitSite::kConsoleOutputExit: return "console-output-exit";
        case VehExitSite::kUnreadableDecodeWindow: return "unreadable-decode-window";
        case VehExitSite::kUnhandledRecover: return "unhandled-recover";
        case VehExitSite::kTerminalFailureSearch: return "terminal-failure-search";
        case VehExitSite::kContinueSearch: return "continue-search";
    }
    return "invalid";
}

}  // namespace repiu::engine
