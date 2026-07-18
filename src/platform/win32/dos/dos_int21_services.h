#pragma once

// DOS INT 21h / INT 2Fh service handlers extracted from execution_trampoline.cpp
// (Phase 1 increment 7). File-system HLE lives in repiu/hle/dos_file_system.h;
// shared substrate (RecordHandledDosInterrupt etc.) in execution_internal.h.

#include "thread_context.h"

#include <string>

namespace repiu::platform::win32
{

void RecordDosChangeDirectory(ThreadContext* context,
                              const std::string& guest_path,
                              const repiu::hle::DosResolvedPath& resolved);

void RecordDosPathTrace(ThreadContext* context,
                        const char* service,
                        const std::string& guest_path,
                        const std::string& virtual_path,
                        const std::string& host_path,
                        bool success,
                        std::uint16_t dos_error,
                        std::uint8_t drive,
                        std::uint8_t access_mode);

std::string BuildCurrentDosVirtualPath(
    const repiu::hle::DosVirtualFileSystemState& state);

std::string BuildCurrentDosHostPath(
    const repiu::hle::DosVirtualFileSystemState& state);

bool HandleDosChangeDirectory(CONTEXT* win32_context, ThreadContext* context);

bool HandleDosGetCurrentDirectory(CONTEXT* win32_context,
                                  ThreadContext* context);

void HandleDosGetCurrentDrive(CONTEXT* win32_context, ThreadContext* context);

void RecordDosOpen(ThreadContext* context,
                   const std::string& guest_path,
                   const repiu::hle::DosResolvedPath& resolved,
                   std::uint16_t handle,
                   std::uint8_t access_mode);

bool HandleDosOpenFile(CONTEXT* win32_context, ThreadContext* context);

bool HandleDosFileAttributes(CONTEXT* win32_context,
                             ThreadContext* context);

void CaptureDosTermination(CONTEXT* win32_context,
                           ThreadContext* context);

void RecordDosRead(const CONTEXT* win32_context,
                   ThreadContext* context,
                   std::uint16_t handle,
                   std::uint32_t requested_bytes,
                   std::uint32_t actual_bytes,
                   std::uint32_t buffer,
                   bool success,
                   std::uint16_t error,
                   const std::vector<std::uint8_t>* bytes);

bool HandleDosReadFile(CONTEXT* win32_context, ThreadContext* context);

void RecordDosSeek(ThreadContext* context,
                   std::uint16_t handle,
                   std::uint8_t origin,
                   std::int32_t offset,
                   std::uint32_t position_before,
                   std::uint32_t position,
                   bool success,
                   std::uint16_t error);

bool HandleDosSeekFile(CONTEXT* win32_context, ThreadContext* context);

void RecordDosClose(ThreadContext* context,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error);

bool HandleDosCloseFile(CONTEXT* win32_context, ThreadContext* context);

void RecordDosIoctl(ThreadContext* context,
                    std::uint8_t subfunction,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error,
                    std::uint16_t device_info);

bool HandleDosIoctl(CONTEXT* win32_context, ThreadContext* context);

void RecordDosResize(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint16_t paragraphs,
                     bool success,
                     std::uint16_t error,
                     std::uint32_t requested_end = 0,
                     std::uint32_t allocator_end = 0);

bool HandleDosResizeMemoryBlock(CONTEXT* win32_context,
                                ThreadContext* context);

void HandleDosGetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context);

void HandleDosSetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context);

bool HandleDosInterrupt21(CONTEXT* win32_context, ThreadContext* context);

bool HandleDosInterrupt2F(CONTEXT* win32_context, ThreadContext* context);

} // namespace repiu::platform::win32
