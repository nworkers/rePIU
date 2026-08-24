#pragma once

// MSCDEX (INT 2Fh AH=15h) request/ioctl helpers, DPMI INT 31h, and mouse INT 33h
// handlers extracted from execution_trampoline.cpp (Phase 1 increment 8).

#include "thread_context.h"

#include <cstddef>
#include <cstdint>
#include "repiu/platform/guest_cpu_context.h"

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

// Absolute disc address: MSF counts from the 2-second lead-in, so LBA 0 is
// 00:02:00. Use this for anything the spec calls an absolute address.
std::uint32_t MscdexLbaToMsf(std::uint32_t lba);

// Elapsed-time address: a running time within a track starts at 00:00:00 and
// must not carry the lead-in offset.
std::uint32_t MscdexFramesToMsf(std::uint32_t frames);

bool HandleMscdexIoctl(ThreadContext* context, std::uint8_t* request);

bool HandleMscdexIoctlOutput(ThreadContext* context, std::uint8_t* request);

bool HandleDpmiInterrupt31(repiu::platform::GuestCpuContext* win32_context, ThreadContext* context);

bool HandleMouseInterrupt33(repiu::platform::GuestCpuContext* win32_context, ThreadContext* context);

bool HandleMscdexRequest(ThreadContext* context,
                         std::uint16_t segment,
                         std::uint16_t offset);

} // namespace repiu::platform::win32
