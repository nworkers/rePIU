#pragma once

// AOT (ahead-of-time translation) runtime dispatch extracted from
// execution_trampoline.cpp (Phase 1 increment 10): translation worker, guest
// code-write watch/fault handling, inline-cache patching, page retirement,
// transfer-target resolution, and conditional/indirect/return/reentry dispatch.

#include "aot_boundary_reason.h"
#include "thread_context.h"

#include <cstdint>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/fault_handler.h"

// Task 503d-2. The only thing left in this header that needs the Win32 headers
// is the translation worker's entry point below: CreateThread dictates its
// return type and calling convention, so they are the operating system's to
// name. Everything else moved to the platform-neutral types.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Task 503d-2. EXCEPTION_POINTERS is forward declared by its underlying tag so
// this header needs no <windows.h>: a pointer to an incomplete type is all a
// declaration requires, and on Windows it resolves to the very same type.
struct _EXCEPTION_POINTERS;

namespace repiu::platform::win32
{

struct Win32AotSegmentTable;

void BumpAotBoundaryCount(ThreadContext* context);

// Increment the per-reason boundary counter matching `reason` (Task 262),
// mirrored to shared telemetry the same way as BumpAotBoundaryCount.
void BumpAotBoundaryReason(ThreadContext* context, AotBoundaryReason reason);

// Task 263(a): record a sample of an `other` boundary (lead-opcode histogram,
// last EIP + bytes, mirrored top opcode). `bytes`/`length` are the readable
// boundary guest bytes already probed at the call site.
void RecordAotOtherBoundarySample(ThreadContext* context,
                                  std::uint32_t guest_eip,
                                  const std::uint8_t* bytes,
                                  std::size_t length);

// AccumulateAotResidency moved to telemetry/aot_residency_sample.h (Task 478).

void BumpAotReentryCount(ThreadContext* context);

void BumpAotPageRetireAttemptCount(ThreadContext* context);

void BumpAotPageRetireSuccessCount(ThreadContext* context);

void BumpAotRetiredEntryTrapCount(ThreadContext* context);

void BumpAotQuarantineCount(ThreadContext* context);

// Evidence packet for a pathological zero return address / zero EIP
// (design 246): guest stack around ESP, live code bytes around
// code_center, tracked call frames, and the recent return trace.
void DumpZeroReturnEvidence(const repiu::platform::GuestCpuContext* win32_context,
                            ThreadContext* context,
                            const char* reason,
                            std::uint32_t code_center);

// Task 503d-6. Was a Win32 thread procedure, guarded so it could name DWORD
// and WINAPI; now an ordinary function, and the Win32 shim at the creation site
// casts its result back into a thread exit code. The guard went with the
// signature, exactly as 503d-2 said it would.
int AotTranslationWorkerProc(void* parameter);

// Task 264 Phase 3a: build the per-segment resolution table (shadow addresses,
// current selectors, descriptor bases) from the live guest context.
void BuildWin32AotSegmentTable(ThreadContext* context,
                               Win32AotSegmentTable* table);

// Re-apply the guard selectors and folded bases to every segment-override site,
// using the current segment state. Called after the guest reloads a segment
// register so static-image sites (baked before configuration) become active.
void ReResolveAotSegmentOverrides(ThreadContext* context);

bool RequestAotDynamicTranslation(ThreadContext* context,
                                  std::uint32_t target,
                                  std::uint32_t* cache_entry,
                                  std::uint32_t* added_bytes);

void ReleaseUnneededWin32AotGuestPageWatches(ThreadContext* context,
                                             std::uint32_t address,
                                             std::uint32_t size);

bool HandleAotGuestCodeWriteCompletion(
    const repiu::platform::FaultEvent& fault, ThreadContext* context);

bool HandleAotGuestCodeWriteFault(const repiu::platform::FaultEvent& fault,
                                  ThreadContext* context);

// Task 445: opt-in. When on, the inline-cache patch runs on the guest thread
// instead of costing a worker event round trip -- measured at 34.1% of the
// guest thread's position samples on pumpit2, 385 patches per frame.
bool AotInlineCachePatchOnGuestThreadEnabled();

bool RequestAotInlineCachePatch(ThreadContext* context,
                                std::uint32_t cache_miss_address,
                                std::uint32_t guest_target,
                                std::uint32_t cache_target);

bool RequestAotGuestPageRetirement(ThreadContext* context,
                                   std::uint32_t guest_page,
                                   bool quarantine);

// Task 415's generation-failure policy counters live in
// aot/aot_generation_failure_policy.h, which carries no ThreadContext
// dependency so the host can read them.

std::uint32_t AotGuestAddressForExecutionAddress(
    const ThreadContext* context,
    std::uint32_t execution_address);

bool IsAotInlineCacheMiss(const ThreadContext* context,
                          std::uint32_t cache_address);

bool IsAotHleBoundaryAddress(const ThreadContext* context,
                             std::uint32_t guest_address);

bool ResolveAotTransferTarget(ThreadContext* context,
                              std::uint32_t target,
                              std::uint32_t* cache_target,
                              bool force_generation = false,
                              AotRetiredTrapResolution* retired_resolution =
                                  nullptr);

bool EvaluateAotCondition(std::uint8_t condition, std::uint32_t eflags);

bool HandleAotConditionalTransfer(const repiu::platform::FaultEvent& fault,
                                  ThreadContext* context);

bool HandleAotIndirectTransfer(const repiu::platform::FaultEvent& fault,
                               ThreadContext* context,
                               AotDbtDispatchFallbackReason* fallback_reason =
                                   nullptr,
                               Win32AotTransferOrigin origin =
                                   Win32AotTransferOrigin::kVeh);

bool HandleAotReturnTransfer(const repiu::platform::FaultEvent& fault,
                             ThreadContext* context,
                             AotDbtDispatchFallbackReason* fallback_reason =
                                 nullptr,
                             Win32AotTransferOrigin origin =
                                 Win32AotTransferOrigin::kVeh,
                             std::uint32_t return_patch_site_index =
                                 0xFFFFFFFFU);

bool HandleAotReentry(const repiu::platform::FaultEvent& fault,
                      ThreadContext* context);

} // namespace repiu::platform::win32
