#ifndef REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_
#define REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_

#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/guest_context.h"

#include <cstdint>
#include <string>

namespace repiu::platform::win32
{

struct Win32MinimalExecutionAttempt
{
    bool valid = false;
    bool supported = false;
    bool attempted = false;
    bool returned = false;
    bool exception_caught = false;
    bool timed_out = false;
    bool guest_stack_switch_supported = false;
    bool guest_stack_switch_attempted = false;
    std::uint32_t entry_address = 0;
    std::uint32_t guest_stack_initial_esp = 0;
    std::uint32_t guest_stack_return_esp = 0;
    std::uint32_t seh_exception_code = 0;
    std::uint32_t seh_exception_address = 0;
    std::uint32_t exception_eax = 0;
    std::uint32_t exception_ebx = 0;
    std::uint32_t exception_ecx = 0;
    std::uint32_t exception_edx = 0;
    std::uint32_t exception_esi = 0;
    std::uint32_t exception_edi = 0;
    std::uint32_t handled_hle_trap_count = 0;
    std::uint32_t last_hle_trap_address = 0;
    std::uint32_t last_hle_trap_opcode = 0;
    std::uint32_t handled_dos_interrupt_count = 0;
    std::uint32_t last_dos_interrupt_vector = 0;
    std::uint32_t last_dos_interrupt_ah = 0;
    std::uint32_t handled_segment_load_count = 0;
    std::uint32_t last_segment_load_address = 0;
    std::uint32_t last_segment_load_opcode = 0;
    std::uint32_t last_segment_load_register = 0;
    std::uint32_t last_segment_load_selector = 0;
    std::uint32_t thread_exit_code = 0;
    std::string hle_console_output;
    std::string message;
};

bool AttemptWin32MinimalExecution(
    const Win32RelocatedImagePlacement& placement,
    std::uint32_t entry_address,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackTrapExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

bool AttemptWin32GuestStackHleExecution(
    const Win32RelocatedImagePlacement& placement,
    const runtime::GuestStackSwitchPlan& stack_plan,
    std::uint32_t timeout_milliseconds,
    Win32MinimalExecutionAttempt* attempt);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_EXECUTION_TRAMPOLINE_H_
