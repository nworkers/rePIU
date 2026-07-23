#include "native_linear_span_probe.h"

#include "verified_region_analyzer.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::tools
{

bool RunNativeLinearSpanProbe()
{
#if !defined(_WIN32)
    return true;
#else
    constexpr std::uint32_t kPageSize = 4096;
    auto* memory = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,
        kPageSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE));
    if (memory == nullptr)
    {
        std::cout << "linear_span_all=false\n";
        return false;
    }
    std::memset(memory, 0x90, kPageSize);

    const std::uint8_t control_bytes[] = {
        0x8B, 0xC1,              // mov eax, ecx
        0x83, 0xC0, 0x01,        // add eax, 1
        0x75, 0x00};             // jne next
    const std::uint8_t sensitive_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0x64, 0xA1, 0, 0, 0, 0  // mov eax, fs:[0]
    };
    const std::uint8_t write_bytes[] = {
        0x90,                    // nop
        0x40,                    // inc eax
        0x89, 0x01};             // mov [ecx], eax
    const std::uint8_t short_bytes[] = {
        0x90,                    // nop
        0x75, 0x00};             // jne next
    std::memcpy(memory, control_bytes, sizeof(control_bytes));
    std::memcpy(memory + 16, sensitive_bytes, sizeof(sensitive_bytes));
    std::memcpy(memory + 32, write_bytes, sizeof(write_bytes));
    std::memcpy(memory + 48, short_bytes, sizeof(short_bytes));

    DWORD old_protection = 0;
    const bool protected_rx =
        VirtualProtect(memory, kPageSize, PAGE_EXECUTE_READ,
                       &old_protection) != FALSE;
    const std::uint32_t base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(memory));
    platform::win32::detail::NativeLinearSpan control;
    platform::win32::detail::NativeLinearSpan sensitive;
    platform::win32::detail::NativeLinearSpan write;
    platform::win32::detail::NativeLinearSpan short_span;
    const bool control_ok = protected_rx &&
        platform::win32::detail::ScanNativeLinearSpanWithZydis(
            base, base, kPageSize, &control) &&
        control.boundary_address == base + 5U &&
        control.instruction_count == 2U &&
        !control.boundary_sensitive &&
        !control.boundary_memory_write;
    const bool sensitive_ok =
        platform::win32::detail::ScanNativeLinearSpanWithZydis(
            base + 16U, base, kPageSize, &sensitive) &&
        sensitive.boundary_address == base + 18U &&
        sensitive.instruction_count == 2U &&
        sensitive.boundary_sensitive;
    const bool write_ok =
        platform::win32::detail::ScanNativeLinearSpanWithZydis(
            base + 32U, base, kPageSize, &write) &&
        write.boundary_address == base + 34U &&
        write.instruction_count == 2U &&
        write.boundary_memory_write;
    const bool short_rejected =
        !platform::win32::detail::ScanNativeLinearSpanWithZydis(
            base + 48U, base, kPageSize, &short_span);
    VirtualFree(memory, 0, MEM_RELEASE);

    const bool all =
        control_ok && sensitive_ok && write_ok && short_rejected;
    std::cout << "linear_span_control_boundary="
              << (control_ok ? "true" : "false")
              << "\nlinear_span_sensitive_boundary="
              << (sensitive_ok ? "true" : "false")
              << "\nlinear_span_write_boundary="
              << (write_ok ? "true" : "false")
              << "\nlinear_span_short_rejected="
              << (short_rejected ? "true" : "false")
              << "\nlinear_span_all=" << (all ? "true" : "false")
              << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
