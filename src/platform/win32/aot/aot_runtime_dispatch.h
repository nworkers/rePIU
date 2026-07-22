#pragma once

// AOT (ahead-of-time translation) runtime dispatch extracted from
// execution_trampoline.cpp (Phase 1 increment 10): translation worker, guest
// code-write watch/fault handling, inline-cache patching, page retirement,
// transfer-target resolution, and conditional/indirect/return/reentry dispatch.

#include "aot_boundary_reason.h"
#include "thread_context.h"

#include <cstdint>

namespace repiu::platform::win32
{

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

// Task 263(b): accumulate the AOT residency proxy for a real cache entry --
// straight-line guest instruction count from `guest_entry_eip` to the first
// control transfer (cap 64), honoring readability.
void AccumulateAotResidency(ThreadContext* context,
                            std::uint32_t guest_entry_eip);

void BumpAotReentryCount(ThreadContext* context);

void BumpAotPageRetireAttemptCount(ThreadContext* context);

void BumpAotPageRetireSuccessCount(ThreadContext* context);

void BumpAotRetiredEntryTrapCount(ThreadContext* context);

void BumpAotQuarantineCount(ThreadContext* context);

// Evidence packet for a pathological zero return address / zero EIP
// (design 246): guest stack around ESP, live code bytes around
// code_center, tracked call frames, and the recent return trace.
void DumpZeroReturnEvidence(const CONTEXT* win32_context,
                            ThreadContext* context,
                            const char* reason,
                            std::uint32_t code_center);

DWORD WINAPI AotTranslationWorkerProc(void* parameter);

bool RequestAotDynamicTranslation(ThreadContext* context,
                                  std::uint32_t target,
                                  std::uint32_t* cache_entry,
                                  std::uint32_t* added_bytes);

void ReleaseUnneededWin32AotGuestPageWatches(ThreadContext* context,
                                             std::uint32_t address,
                                             std::uint32_t size);

bool HandleAotGuestCodeWriteCompletion(EXCEPTION_POINTERS* exception_info,
                                       CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleAotGuestCodeWriteFault(EXCEPTION_POINTERS* exception_info,
                                  CONTEXT* win32_context,
                                  ThreadContext* context);

bool RequestAotInlineCachePatch(ThreadContext* context,
                                std::uint32_t cache_miss_address,
                                std::uint32_t guest_target,
                                std::uint32_t cache_target);

bool RequestAotGuestPageRetirement(ThreadContext* context,
                                   std::uint32_t guest_page,
                                   bool quarantine);

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
                              bool force_generation = false);

bool EvaluateAotCondition(std::uint8_t condition, std::uint32_t eflags);

bool HandleAotConditionalTransfer(EXCEPTION_POINTERS* exception_info,
                                  CONTEXT* win32_context,
                                  ThreadContext* context);

bool HandleAotIndirectTransfer(EXCEPTION_POINTERS* exception_info,
                               CONTEXT* win32_context,
                               ThreadContext* context);

bool HandleAotReturnTransfer(EXCEPTION_POINTERS* exception_info,
                             CONTEXT* win32_context,
                             ThreadContext* context);

bool HandleAotReentry(EXCEPTION_POINTERS* exception_info,
                      CONTEXT* win32_context,
                      ThreadContext* context);

} // namespace repiu::platform::win32
