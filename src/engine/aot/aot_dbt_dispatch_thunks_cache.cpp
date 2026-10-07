#include "aot_dbt_direct_edge_dispatch.h"
#include "aot_dbt_glide_gate_dispatch.h"
#include "aot_dbt_glide_gate_dispatch_state.h"
#include "aot_dbt_hle_dispatch.h"
#include "aot_dbt_return_dispatch.h"

#include "aot_runtime_dispatch.h"
#include "../boundary/linexe_glide_boundary.h"
#include "../execution/execution_internal.h"
#include "../execution/thread_context.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/linux/x64/linux_x64_aot_dispatch.h"

// Task 759. The dispatch thunks of the cache execution model (Linux x64).
// Emitted long-mode code does not call the direct model's four thunks -- its
// returns and misses go through the platform's long-mode return thunk -- so
// those four have no address here. The Glide gate has its own long-mode thunk
// (Task 720), and its resolver is below.
#if defined(_WIN32) || !defined(__x86_64__)
#error "the cache execution model needs a Linux x86-64 host"
#endif

namespace repiu::engine
{
namespace
{

std::atomic<std::uint32_t>& g_entry_count =
    glide_gate_dispatch_state::entry_count;
std::atomic<std::uint32_t>& g_success_count =
    glide_gate_dispatch_state::success_count;
std::atomic<std::uint32_t>& g_terminal_failure_count =
    glide_gate_dispatch_state::terminal_failure_count;

// Task 720. The x64 counterpart of ResolveAotDbtGlideGateFrame, reached from
// RepiuLinuxX64GlideGateThunk with the guest state in the dispatch frame
// rather than in a PUSHAD image on the guest stack.
//
// The checks are the i386 resolver's: the export must decode, the handler must
// move EIP off the gate, and ESP must move by exactly the return address plus
// the stdcall arguments. What differs is the continuation. The i386 thunk
// resolves the cache target itself; this one only reports the guest return
// address, and the thunk hands it to the return thunk, whose lookup, dynamic
// translation and legacy resume every emitted `ret` already relies on.
std::uint32_t ResolveLinuxX64GlideGateFrame(
    void* resolver_context,
    repiu::platform::LinuxX64AotDispatchFrame* frame)
{
    auto* const context = static_cast<ThreadContext*>(resolver_context);
    if (context == nullptr || frame == nullptr)
    {
        g_terminal_failure_count.fetch_add(1U, std::memory_order_relaxed);
        return 0U;
    }
    context->aot_dbt_glide_dispatch_entry_count.fetch_add(
        1U, std::memory_order_relaxed);
    g_entry_count.fetch_add(1U, std::memory_order_relaxed);

    const std::uint32_t gate_address = frame->guest.eip;
    const std::uint32_t original_esp = frame->guest.esp;
    repiu::platform::GuestCpuContext guest_context{};
    guest_context.ContextFlags =
        repiu::platform::kGuestCpuContextIntegerControlSegments;
    guest_context.Edi = frame->guest.edi;
    guest_context.Esi = frame->guest.esi;
    guest_context.Ebp = frame->guest.ebp;
    guest_context.Esp = original_esp;
    guest_context.Ebx = frame->guest.ebx;
    guest_context.Edx = frame->guest.edx;
    guest_context.Ecx = frame->guest.ecx;
    guest_context.Eax = frame->guest.eax;
    guest_context.EFlags = frame->guest.eflags;
    guest_context.Eip = gate_address;
    guest_context.SegEs = context->guest_es;
    guest_context.SegSs = context->guest_ss;
    guest_context.SegDs = context->guest_ds;
    guest_context.SegFs = context->guest_fs;
    guest_context.SegGs = context->guest_gs;

    const repiu::hle::GlideExportGate* gate =
        repiu::hle::DecodeGlideGate(
            context->glide_gate_plan,
            gate_address - context->linexe_arena_layout.gate_code_base);
    const std::uint32_t expected_adjust =
        gate != nullptr ? 4U + gate->argument_byte_count : 0U;
    if (gate == nullptr || expected_adjust >
            std::numeric_limits<std::uint16_t>::max() ||
        !HandleGlideGateBoundary(&guest_context, context) ||
        static_cast<std::uint32_t>(guest_context.Eip) == gate_address ||
        // Task 750: a tick injected while the swap gate waits pops the return
        // address and leaves an interrupt frame (EFLAGS, CS, EIP) returning
        // to the call, instead of the gate's own return.
        static_cast<std::uint32_t>(guest_context.Esp) !=
            (context->glide_gate_interrupt_injected
                 ? original_esp + 4U - 12U
                 : original_esp + expected_adjust))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        g_terminal_failure_count.fetch_add(1U, std::memory_order_relaxed);
        return 0U;
    }

    frame->guest.edi = guest_context.Edi;
    frame->guest.esi = guest_context.Esi;
    frame->guest.ebp = guest_context.Ebp;
    frame->guest.ebx = guest_context.Ebx;
    frame->guest.edx = guest_context.Edx;
    frame->guest.ecx = guest_context.Ecx;
    frame->guest.eax = guest_context.Eax;
    frame->guest.esp = static_cast<std::uint32_t>(guest_context.Esp);
    frame->guest.eflags = guest_context.EFlags & ~0x00000100U;
    frame->guest.eip = static_cast<std::uint32_t>(guest_context.Eip);
    frame->guest_source = static_cast<std::uint32_t>(guest_context.Eip);
    g_success_count.fetch_add(1U, std::memory_order_relaxed);
    return 1U;
}

}  // namespace

void* GetAotDbtDirectEdgeDispatchThunkAddress()
{
    return nullptr;
}

void* GetAotDbtHleDispatchThunkAddress()
{
    return nullptr;
}

void* GetAotDbtReturnMissThunkAddress()
{
    return nullptr;
}

void* GetGlideGateDirectDispatchThunkAddress()
{
    // Task 720.
    return reinterpret_cast<void*>(
        repiu::platform::LinuxX64GlideGateThunkAddress());
}

void InstallGlideGateDirectDispatchResolver()
{
    repiu::platform::InstallLinuxX64GlideGateResolver(
        &ResolveLinuxX64GlideGateFrame);
}

}  // namespace repiu::engine
