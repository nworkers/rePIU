#pragma once

#include "repiu/platform/win32/execution_trampoline.h"
#include "repiu/runtime/aot_code_cache.h"

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repiu::platform::win32
{

struct ThreadContext;

void ConfigureAotDbtCallStepProbe(
    ThreadContext* context, const char* sequence_list);

bool MaybeArmAotDbtCallStepProbe(
    ThreadContext* context,
    Win32AotTransferOrigin origin,
    const runtime::AotDbtIndirectDispatchSite& site,
    std::uint32_t call_sequence,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    std::uint32_t guest_return,
    std::uint32_t entry_esp,
    std::uint32_t* saved_eflags);

bool HandleAotDbtCallStepProbe(
    EXCEPTION_POINTERS* exception_info,
    CONTEXT* win32_context,
    ThreadContext* context);

bool AotDbtCallStepReturnWatchActive(const ThreadContext* context);

}  // namespace repiu::platform::win32
