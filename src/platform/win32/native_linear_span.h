#pragma once

#include "repiu/runtime/execution_backend.h"

#include <cstdint>
#include <string_view>

struct _CONTEXT;
using CONTEXT = _CONTEXT;

namespace repiu::platform::win32
{

struct ThreadContext;

bool ResolveNativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend,
    std::string_view setting);
bool ResolveNativeLinearSpanCacheEnabled(std::string_view setting);
bool ResolveNativeLinearSpanWritesEnabled(std::string_view setting);
bool ResolveNativeLinearSpanJumpsEnabled(std::string_view setting);
bool NativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend);
bool TryEnterNativeLinearSpan(CONTEXT* win32_context,
                              ThreadContext* context);
void LeaveNativeLinearSpan(CONTEXT* win32_context,
                           ThreadContext* context,
                           bool reached_boundary,
                           bool write_fault_cancel,
                           std::uint32_t exception_code);

namespace detail
{

struct NativeFastPathState;
struct NativeLinearSpan;

bool LookupNativeLinearSpanScanCache(
    NativeFastPathState* state,
    std::uint32_t entry,
    std::uint32_t guest_page,
    std::uint32_t generation,
    NativeLinearSpan* span);
void StoreNativeLinearSpanScanCache(
    NativeFastPathState* state,
    std::uint32_t entry,
    std::uint32_t guest_page,
    std::uint32_t generation,
    const NativeLinearSpan& span);

}  // namespace detail

}  // namespace repiu::platform::win32
