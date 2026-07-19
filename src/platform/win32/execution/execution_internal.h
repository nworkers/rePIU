#pragma once

// Declarations for functions that are defined in execution_trampoline.cpp but
// must be visible across the win32 execution translation units extracted from
// it (Phase 1 decomposition). These carry external linkage; their definitions
// were moved out of the anonymous namespace accordingly. As dedicated module
// headers are formalized, declarations here migrate to their owning module.

#include "thread_context.h"

#include <cstddef>
#include <cstdint>

namespace repiu::platform::win32
{

bool IsAotCacheAddress(const ThreadContext* context, std::uint32_t address);

bool WriteGuestBytes(ThreadContext* context,
                     void* destination,
                     const void* source,
                     std::size_t byte_count);

bool NoteSuccessfulAotGuestWrite(ThreadContext* context,
                                 std::uint32_t destination,
                                 std::uint32_t byte_count);

void RecoverFromHleExit(CONTEXT* win32_context, ThreadContext* thread_context);

void RecordHandledDosInterrupt(ThreadContext* context, std::uint8_t vector, std::uint16_t ax);

void RecordLowMemoryAccess(CONTEXT* win32_context, ThreadContext* context,
                           std::uint8_t opcode, std::uint32_t destination, std::uint32_t value);

std::uint16_t ReadGuestSegmentSelector(const ThreadContext& context,
                                       std::uint8_t segment_register, const CONTEXT* win32_context);

bool ResolveSegmentLinearRange(ThreadContext* context, std::uint16_t selector,
                               std::uint32_t offset, std::uint32_t byte_count,
                               bool writable, std::uint32_t* linear_address);

bool IsGuestInstructionPointer(const ThreadContext* context, std::uint32_t eip);
void InjectPendingInterrupts(CONTEXT* win32_context, ThreadContext* context);

void RecordExecutionProbe(CONTEXT* win32_context, ThreadContext* context);
void RecordExecutionTrace(CONTEXT* win32_context, ThreadContext* context);

} // namespace repiu::platform::win32
