#include "aot_dbt_glide_gate_dispatch.h"

#include "aot_runtime_dispatch.h"
#include "aot_residency_sample.h"
#include "repiu/engine/aot_code_cache.h"
#include "../boundary/linexe_glide_boundary.h"
#include "../execution/execution_internal.h"
#include "../execution/thread_context.h"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/thunk_calling_convention.h"
#include "repiu/platform/virtual_memory.h"

namespace repiu::engine
{
namespace
{

constexpr std::size_t kSavedEspIndex = 3U;
constexpr std::size_t kSavedEflagsIndex = 8U;
constexpr std::size_t kGateContinuationIndex = 9U;
constexpr std::size_t kGuestReturnIndex = 10U;

std::atomic<std::uint32_t> g_patched_gate_count{0};
std::atomic<std::uint32_t> g_verified_gate_count{0};
std::atomic<std::uint32_t> g_resolved_target_count{0};
std::atomic<std::uint32_t> g_relinked_cache_target_count{0};
// Task 519: the same total, split by how each patch was found.
//
// `content` slots are collected by reading the cache and matching the boundary
// address, so a slot that was patched last time cannot be collected again --
// finding one means the write did not stick. `fixup` slots come from the static
// fixup list and are rewritten on every activation regardless, so they say
// nothing about persistence. Only the first number can answer it, and the
// combined counter cannot.
std::atomic<std::uint32_t> g_relink_content_patch_count{0};
std::atomic<std::uint32_t> g_relink_fixup_patch_count{0};
std::atomic<std::uint32_t> g_entry_count{0};
std::atomic<std::uint32_t> g_success_count{0};
std::atomic<std::uint32_t> g_target_miss_count{0};
std::atomic<std::uint32_t> g_terminal_failure_count{0};

extern "C" void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtGlideGateFrame(
    ThreadContext* context, std::uint32_t* frame)
{
    if (context == nullptr || frame == nullptr)
    {
        g_terminal_failure_count.fetch_add(1U, std::memory_order_relaxed);
        return;
    }
    context->aot_dbt_glide_dispatch_entry_count.fetch_add(
        1U, std::memory_order_relaxed);
    g_entry_count.fetch_add(1U, std::memory_order_relaxed);

    const std::uint32_t continuation = frame[kGateContinuationIndex];
    if (continuation < 5U)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        g_terminal_failure_count.fetch_add(1U, std::memory_order_relaxed);
        return;
    }
    const std::uint32_t gate_address = continuation - 5U;
    const std::uint32_t original_esp = frame[kSavedEspIndex] + 8U;

    repiu::platform::GuestCpuContext guest_context{};
    guest_context.ContextFlags =
        repiu::platform::kGuestCpuContextIntegerControlSegments;
    guest_context.Edi = frame[0];
    guest_context.Esi = frame[1];
    guest_context.Ebp = frame[2];
    guest_context.Esp = original_esp;
    guest_context.Ebx = frame[4];
    guest_context.Edx = frame[5];
    guest_context.Ecx = frame[6];
    guest_context.Eax = frame[7];
    guest_context.EFlags = frame[kSavedEflagsIndex];
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
        static_cast<std::uint32_t>(guest_context.Esp) !=
            original_esp + expected_adjust)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        g_terminal_failure_count.fetch_add(1U, std::memory_order_relaxed);
        return;
    }

    frame[0] = guest_context.Edi;
    frame[1] = guest_context.Esi;
    frame[2] = guest_context.Ebp;
    frame[4] = guest_context.Ebx;
    frame[5] = guest_context.Edx;
    frame[6] = guest_context.Ecx;
    frame[7] = guest_context.Eax;

    std::uint32_t cache_target = 0U;
    if (ResolveAotTransferTarget(
            context, static_cast<std::uint32_t>(guest_context.Eip),
            &cache_target))
    {
        frame[kSavedEflagsIndex] = guest_context.EFlags & ~0x00000100U;
        frame[kGuestReturnIndex] = cache_target;
        context->aot_reentry_pending = false;
        context->enable_single_step_trace = false;
        AccumulateAotResidency(
            context, static_cast<std::uint32_t>(guest_context.Eip));
        BumpAotReentryCount(context);
        g_success_count.fetch_add(1U, std::memory_order_relaxed);
        return;
    }

    // The Glide side effect is already committed. Resume the handler-produced
    // guest return under the established one-step bridge; never re-run gate.
    frame[kSavedEflagsIndex] = guest_context.EFlags | 0x00000100U;
    frame[kGuestReturnIndex] = static_cast<std::uint32_t>(guest_context.Eip);
    context->aot_reentry_pending = true;
    context->enable_single_step_trace = true;
    g_target_miss_count.fetch_add(1U, std::memory_order_relaxed);
}

#if defined(_MSC_VER) && defined(_M_IX86)
extern "C" __declspec(naked) void AotDbtGlideGateDispatchThunk()
{
    __asm
    {
        pushfd
        pushad
        cld

        mov esi, esp
        mov ecx, dword ptr [g_repiu_active_thread_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_dbt_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_dbt_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_dbt_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call ResolveAotDbtGlideGateFrame
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_dbt_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_dbt_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        int 3
    }
}
#endif

#if !defined(_MSC_VER) && defined(__i386__)
// Task 503d-12: the same thunk on Linux, one instantiation of the shared
// bridge macro in src/platform/linux/aot_dbt_dispatch_thunks.S. GCC has no
// naked functions on x86, so only the declaration is here.
extern "C" void AotDbtGlideGateDispatchThunk();
#endif

}  // namespace

bool ResolveGlideGateDirectDispatchEnabled(const char* setting)
{
    if (setting == nullptr)
    {
        return true;
    }
    const std::string_view value(setting);
    return value == "1" || value == "on" || value == "true";
}

bool ResolveGlideGateDirectTarget(
    const ThreadContext* context,
    std::uint32_t target,
    std::uint32_t* direct_target)
{
    // Task 426: the backend check here was redundant. The trampoline sets
    // `aot_dbt_glide_direct_dispatch` only when the backend uses dynamic
    // translation, so the flag already subsumes it.
    if (context == nullptr || direct_target == nullptr ||
        !context->aot_dbt_glide_direct_dispatch ||
        !context->linexe_environment_active ||
        target < context->linexe_arena_layout.gate_code_base)
    {
        return false;
    }
    const std::uint32_t offset =
        target - context->linexe_arena_layout.gate_code_base;
    if (repiu::hle::DecodeGlideGate(context->glide_gate_plan, offset) == nullptr)
    {
        return false;
    }
    *direct_target = target;
    g_resolved_target_count.fetch_add(1U, std::memory_order_relaxed);
    return true;
}

bool ActivateGlideGateDirectTarget(
    ThreadContext* context,
    std::uint32_t cache_boundary_address,
    std::uint32_t gate_address)
{
    std::uint32_t direct_target = 0U;
    if (!ResolveGlideGateDirectTarget(
            context, gate_address, &direct_target) ||
        context->aot_placement == nullptr)
    {
        return false;
    }
    auto* placement = context->aot_placement;
    std::vector<std::uint32_t> patches;
    for (const runtime::AotIndirectInlineCacheSite& site :
         placement->indirect_inline_cache_sites)
    {
        const auto inspect = [&](std::uint32_t offset) {
            std::uint32_t value = 0U;
            std::memcpy(&value, reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(placement->base_address + offset)),
                sizeof(value));
            if (value == cache_boundary_address)
            {
                patches.push_back(offset);
            }
        };
        if (site.entries.empty())
        {
            inspect(site.target_immediate_offset);
        }
        else
        {
            for (const runtime::AotInlineCacheEntry& entry : site.entries)
            {
                inspect(entry.target_immediate_offset);
            }
        }
    }
    std::vector<std::uint32_t> direct_patches;
    for (const runtime::AotCodeCacheFixup& fixup : placement->fixups)
    {
        if (fixup.guest_target == gate_address &&
            (fixup.kind == runtime::AotFixupKind::kDirectCall ||
             fixup.kind == runtime::AotFixupKind::kDirectJump ||
             fixup.kind == runtime::AotFixupKind::kBlockFallthrough))
        {
            direct_patches.push_back(fixup.cache_patch_offset);
        }
    }
    if (!patches.empty() || !direct_patches.empty())
    {
        auto* cache = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(placement->base_address));
        repiu::platform::MemoryProtection previous =
            repiu::platform::MemoryProtection::kOther;
        if (!repiu::platform::ProtectMemory(
                cache, placement->capacity,
                repiu::platform::MemoryProtection::kExecuteReadWrite,
                &previous))
        {
            return false;
        }
        for (std::uint32_t offset : patches)
        {
            std::memcpy(reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(placement->base_address + offset)),
                &direct_target, sizeof(direct_target));
        }
        for (std::uint32_t offset : direct_patches)
        {
            const std::int64_t relative =
                static_cast<std::int64_t>(direct_target) -
                static_cast<std::int64_t>(
                    placement->base_address + offset + 4U);
            if (relative < std::numeric_limits<std::int32_t>::min() ||
                relative > std::numeric_limits<std::int32_t>::max())
            {
                continue;
            }
            const std::int32_t displacement =
                static_cast<std::int32_t>(relative);
            std::memcpy(reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(
                    placement->base_address + offset)),
                &displacement, sizeof(displacement));
        }
        const bool restored = repiu::platform::ProtectMemory(
            cache, placement->capacity, previous, nullptr);
        repiu::platform::FlushInstructionCacheRange(cache, placement->size);
        if (!restored)
        {
            return false;
        }
        g_relinked_cache_target_count.fetch_add(
            static_cast<std::uint32_t>(
                patches.size() + direct_patches.size()),
            std::memory_order_relaxed);
        g_relink_content_patch_count.fetch_add(
            static_cast<std::uint32_t>(patches.size()),
            std::memory_order_relaxed);
        g_relink_fixup_patch_count.fetch_add(
            static_cast<std::uint32_t>(direct_patches.size()),
            std::memory_order_relaxed);
    }
    return true;
}

bool PatchGlideGatePlanForDirectDispatch(
    std::uint32_t gate_code_base,
    repiu::hle::GlideGatePlan* plan)
{
    const std::uintptr_t thunk_value = reinterpret_cast<std::uintptr_t>(
        GetGlideGateDirectDispatchThunkAddress());
    if (plan == nullptr || !plan->valid || plan->gate_stride != 8U ||
        thunk_value == 0U ||
        thunk_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    std::vector<std::uint8_t> patched = plan->image;
    for (const repiu::hle::GlideExportGate& gate : plan->exports)
    {
        const std::uint32_t cleanup = gate.argument_byte_count;
        const std::size_t offset = static_cast<std::size_t>(
            gate.gate_offset - plan->first_gate_offset);
        if (gate.gate_offset < plan->first_gate_offset ||
            offset + 8U > patched.size() ||
            cleanup > std::numeric_limits<std::uint16_t>::max() ||
            patched[offset] != 0x0FU || patched[offset + 1U] != 0x0BU ||
            patched[offset + 2U] !=
                static_cast<std::uint8_t>(gate.ordinal) ||
            patched[offset + 3U] !=
                static_cast<std::uint8_t>(gate.ordinal >> 8U) ||
            patched[offset + 4U] != 0xC3U)
        {
            return false;
        }
        const std::uint64_t call_address =
            static_cast<std::uint64_t>(gate_code_base) + gate.gate_offset;
        const std::int64_t displacement =
            static_cast<std::int64_t>(thunk_value) -
            static_cast<std::int64_t>(call_address + 5U);
        if (displacement < std::numeric_limits<std::int32_t>::min() ||
            displacement > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        patched[offset] = 0xE8U;
        const std::int32_t rel32 = static_cast<std::int32_t>(displacement);
        std::memcpy(patched.data() + offset + 1U, &rel32, sizeof(rel32));
        patched[offset + 5U] = 0xC2U;
        patched[offset + 6U] = static_cast<std::uint8_t>(cleanup & 0xFFU);
        patched[offset + 7U] = static_cast<std::uint8_t>(cleanup >> 8U);
    }
    plan->image = std::move(patched);
    g_patched_gate_count.store(
        static_cast<std::uint32_t>(plan->exports.size()),
        std::memory_order_relaxed);
    plan->message = "asset-derived Glide gates use direct Win32 dispatch";
    return true;
}

bool VerifyGlideGateDirectDispatchImage(
    std::uint32_t gate_code_base,
    const repiu::hle::GlideGatePlan& plan)
{
    if (!plan.valid || plan.gate_stride != 8U)
    {
        return false;
    }
    std::uint32_t verified = 0U;
    for (const repiu::hle::GlideExportGate& gate : plan.exports)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(gate_code_base + gate.gate_offset));
        const std::uint32_t cleanup = gate.argument_byte_count;
        if (bytes[0] != 0xE8U || bytes[5] != 0xC2U ||
            bytes[6] != static_cast<std::uint8_t>(cleanup) ||
            bytes[7] != static_cast<std::uint8_t>(cleanup >> 8U))
        {
            return false;
        }
        ++verified;
    }
    g_verified_gate_count.store(verified, std::memory_order_relaxed);
    return true;
}

GlideGateDirectDispatchStats
ReadGlideGateDirectDispatchStats()
{
    return {
        g_patched_gate_count.load(std::memory_order_relaxed),
        g_verified_gate_count.load(std::memory_order_relaxed),
        g_resolved_target_count.load(std::memory_order_relaxed),
        g_relinked_cache_target_count.load(std::memory_order_relaxed),
        g_entry_count.load(std::memory_order_relaxed),
        g_success_count.load(std::memory_order_relaxed),
        g_target_miss_count.load(std::memory_order_relaxed),
        g_terminal_failure_count.load(std::memory_order_relaxed),
        g_relink_content_patch_count.load(std::memory_order_relaxed),
        g_relink_fixup_patch_count.load(std::memory_order_relaxed),
    };
}

void* GetGlideGateDirectDispatchThunkAddress()
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    return reinterpret_cast<void*>(&AotDbtGlideGateDispatchThunk);
#else
    return nullptr;
#endif
}

}  // namespace repiu::engine
