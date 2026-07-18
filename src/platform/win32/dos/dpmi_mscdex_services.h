#pragma once

// MSCDEX (INT 2Fh AH=15h) request/ioctl helpers, DPMI INT 31h, and mouse INT 33h
// handlers extracted from execution_trampoline.cpp (Phase 1 increment 8).

#include "thread_context.h"

#include <cstddef>
#include <cstdint>

namespace repiu::platform::win32
{

std::uint8_t* ResolveMscdexBuffer(ThreadContext* context,
                                  std::uint16_t segment,
                                  std::uint16_t offset,
                                  std::uint32_t bytes,
                                  std::uint32_t* resolve_kind = nullptr);

std::uint32_t ReadPacketU32(const std::uint8_t* packet,
                            std::size_t offset);

void WritePacketU16(std::uint8_t* packet, std::size_t offset,
                    std::uint16_t value);

void WritePacketU32(std::uint8_t* packet, std::size_t offset,
                    std::uint32_t value);

void WritePacketMsf3(std::uint8_t* packet, std::size_t offset,
                     std::uint32_t msf);

std::uint32_t MscdexMsfToLba(std::uint32_t msf);

std::uint32_t MscdexLbaToMsf(std::uint32_t lba);

bool HandleMscdexIoctl(ThreadContext* context, std::uint8_t* request);

bool HandleDpmiInterrupt31(CONTEXT* win32_context, ThreadContext* context);

bool HandleMouseInterrupt33(CONTEXT* win32_context, ThreadContext* context);

bool HandleMscdexRequest(ThreadContext* context,
                         std::uint16_t segment,
                         std::uint16_t offset);

} // namespace repiu::platform::win32
