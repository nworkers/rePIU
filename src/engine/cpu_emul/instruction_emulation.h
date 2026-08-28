#pragma once

// x86 instruction emulation extracted from execution_trampoline.cpp (Phase 1
// increment 9): register/flag/decode helpers, segment-register instruction
// handlers, traced-memory instruction handlers, REP string operations, and
// the traced DOS/DPMI/mouse interrupt wrappers.

#include "thread_context.h"

#include <cstddef>
#include <cstdint>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{

std::uint16_t ReadRegister16(const repiu::platform::GuestCpuContext& win32_context,
                             std::uint8_t register_id);

void WriteRegister16(repiu::platform::GuestCpuContext* win32_context,
                     std::uint8_t register_id,
                     std::uint16_t value);

std::uint8_t ReadRegister8(const repiu::platform::GuestCpuContext& win32_context,
                           std::uint8_t register_index);

void WriteRegister8(repiu::platform::GuestCpuContext* win32_context,
                    std::uint8_t register_index,
                    std::uint8_t value);

// CMP flag semantics at an 8-, 16-, or 32-bit operand width. Any other width
// leaves the flags untouched.
void SetCompareFlags(repiu::platform::GuestCpuContext* win32_context,
                     std::uint32_t lhs,
                     std::uint32_t rhs,
                     std::uint32_t width_bytes);

void SetCompareFlags8(repiu::platform::GuestCpuContext* win32_context,
                      std::uint8_t lhs,
                      std::uint8_t rhs);

void RecordGuestSegmentLoad(repiu::platform::GuestCpuContext* win32_context,
                            ThreadContext* context,
                            std::uint8_t segment_register,
                            std::uint16_t selector,
                            std::uint32_t source);

void RecordGuestSegmentStore(repiu::platform::GuestCpuContext* win32_context,
                             ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t destination);

void RecordGuestSegmentMemoryLoad(repiu::platform::GuestCpuContext* win32_context,
                                  ThreadContext* context,
                                  std::uint8_t opcode,
                                  std::uint8_t segment_register,
                                  std::uint16_t selector,
                                  std::uint32_t offset,
                                  std::uint32_t byte_width,
                                  std::uint32_t value);

void RecordGuestMemoryStore(repiu::platform::GuestCpuContext* win32_context,
                            ThreadContext* context,
                            std::uint32_t opcode,
                            std::uint32_t destination,
                            std::uint32_t value,
                            std::uint32_t byte_width,
                            const char* source_kind,
                            bool applied);

std::uint32_t ReadGeneralRegister32(const repiu::platform::GuestCpuContext* win32_context,
                                    std::uint8_t register_index);

void WriteGeneralRegister32(repiu::platform::GuestCpuContext* win32_context,
                            std::uint8_t register_index,
                            std::uint32_t value);

bool DecodeModRmMemoryAddress(
    const repiu::platform::GuestCpuContext* win32_context,
    const std::uint8_t* instruction,
    std::uint32_t* destination,
    std::uint32_t* instruction_size);

bool HandleSegmentLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                  ThreadContext* context);
bool HandleSegmentPopInstruction(repiu::platform::GuestCpuContext* win32_context,
                                 ThreadContext* context);

bool HandleSegmentStoreInstruction(repiu::platform::GuestCpuContext* win32_context,
                                   ThreadContext* context);

bool HandleSegmentOverrideByteLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                              ThreadContext* context);

bool HandleSegmentMemoryLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                        ThreadContext* context);

bool HandleSegmentMemoryCompareInstruction(repiu::platform::GuestCpuContext* win32_context,
                                           ThreadContext* context);

bool HandleTracedMemoryStoreInstruction(repiu::platform::GuestCpuContext* win32_context,
                                        ThreadContext* context);

bool HandleTracedMemoryTestInstruction(repiu::platform::GuestCpuContext* win32_context,
                                       ThreadContext* context);

bool HandleTracedFpuMemoryInstruction(repiu::platform::GuestCpuContext* win32_context,
                                      ThreadContext* context);

bool HandleTracedMemoryLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                       ThreadContext* context);

bool HandleSegmentLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                  ThreadContext* context);

bool HandleSegmentPopInstruction(repiu::platform::GuestCpuContext* win32_context,
                                 ThreadContext* context);

bool HandleRepStosdInstruction(repiu::platform::GuestCpuContext* win32_context,
                               ThreadContext* context);

bool HandleLodsbInstruction(repiu::platform::GuestCpuContext* win32_context,
                            ThreadContext* context);

bool HandleSegmentStoreInstruction(repiu::platform::GuestCpuContext* win32_context,
                                   ThreadContext* context);

bool ReadSegmentOverrideByte(ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t offset,
                             std::uint8_t* value);

bool HandleSegmentOverrideByteLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                              ThreadContext* context);

void RecordDosEnvironmentAccess(ThreadContext* context, std::uint32_t offset);

bool ReadSegmentDword(ThreadContext* context,
                      std::uint8_t segment_register,
                      std::uint16_t selector,
                      std::uint32_t offset,
                      std::uint32_t* value);

bool ReadSegmentByte(ThreadContext* context,
                     std::uint8_t segment_register,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint8_t* value);

bool ReadSegmentWord(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint16_t* value);

bool HandleSegmentOverrideMemoryLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                                ThreadContext* context);

bool HandleFsSegmentWordLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                        ThreadContext* context);

bool HandleSegmentMemoryLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                        ThreadContext* context);

bool HandleSegmentMemoryCompareInstruction(repiu::platform::GuestCpuContext* win32_context,
                                           ThreadContext* context);

bool HandleTracedMemoryStoreInstruction(repiu::platform::GuestCpuContext* win32_context,
                                        ThreadContext* context);

void AttachAllocatorReadProvenance(ThreadContext* context,
                                   std::uint32_t eip_offset,
                                   std::uint32_t source,
                                   std::uint32_t value);

bool HandleTracedMemoryLoadInstruction(repiu::platform::GuestCpuContext* win32_context,
                                       ThreadContext* context);

bool HasEvenParity(std::uint8_t value);

void UpdateAdd32Flags(repiu::platform::GuestCpuContext* win32_context,
                      std::uint32_t left,
                      std::uint32_t right,
                      std::uint32_t result);

std::uint8_t ReadGeneralRegister8(const repiu::platform::GuestCpuContext* win32_context,
                                  std::uint8_t register_index);

void UpdateLogical32Flags(repiu::platform::GuestCpuContext* win32_context, std::uint32_t result);

void UpdateSubtract8Flags(repiu::platform::GuestCpuContext* win32_context,
                          std::uint8_t left,
                          std::uint8_t right,
                          std::uint8_t result);

bool HandleTracedMemoryAddInstruction(repiu::platform::GuestCpuContext* win32_context,
                                      ThreadContext* context);

bool HandleTracedMemoryOrInstruction(repiu::platform::GuestCpuContext* win32_context,
                                     ThreadContext* context);

bool HandleTracedMemoryCompareByteInstruction(repiu::platform::GuestCpuContext* win32_context,
                                              ThreadContext* context);

bool HandleTracedMemoryTestInstruction(repiu::platform::GuestCpuContext* win32_context,
                                       ThreadContext* context);

bool HandleTracedFpuMemoryInstruction(repiu::platform::GuestCpuContext* win32_context,
                                      ThreadContext* context);

bool HandleTracedDosInterrupt21(repiu::platform::GuestCpuContext* win32_context,
                                ThreadContext* context);

bool HandleTracedDosInterrupt2F(repiu::platform::GuestCpuContext* win32_context,
                                ThreadContext* context);

bool HandleTracedDpmiInterrupt31(repiu::platform::GuestCpuContext* win32_context,
                                 ThreadContext* context);

bool HandleTracedBiosInterrupt16(repiu::platform::GuestCpuContext* win32_context,
                                 ThreadContext* context);

// Names an unrecognised software interrupt in hle_message so a backend running
// without the DOS HLE fallback still reports which vector stopped it.
void RecordUnsupportedTracedSoftwareInterrupt(repiu::platform::GuestCpuContext* win32_context,
                                              ThreadContext* context);

bool HandleTracedMouseInterrupt33(repiu::platform::GuestCpuContext* win32_context,
                                  ThreadContext* context);

bool HandleRepCmpsbInstruction(repiu::platform::GuestCpuContext* win32_context,
                               ThreadContext* context);

bool CopyHostMemoryWithoutVehRecursion(ThreadContext* context,
                                       std::uint32_t destination,
                                       const void* source,
                                       std::uint32_t byte_count,
                                       std::uint32_t* failure_stage,
                                       std::uint32_t* windows_error);

bool HandleRepMovsInstruction(repiu::platform::GuestCpuContext* win32_context,
                              ThreadContext* context);

} // namespace repiu::engine
