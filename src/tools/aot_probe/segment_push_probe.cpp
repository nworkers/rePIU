#include "segment_push_probe.h"

#include "repiu/runtime/selector_table.h"
#include "repiu/runtime/guest_stack_access.h"

#include <iostream>
#include <cstring>
#include <memory>

#if defined(_WIN32) || defined(__linux__)
#include "../../engine/cpu_emul/mode16_stack_push.h"
#include "../../engine/execution/thread_context.h"
#include "repiu/platform/virtual_memory.h"
#endif

namespace repiu::tools
{
namespace
{

bool ProbeMode16PushWrites()
{
#if defined(_WIN32) || defined(__linux__)
    constexpr std::uint32_t kBase = 0x31000000U;
    constexpr std::size_t kBytes = 0x10000U;
    auto reservation = platform::ReserveMemory(
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(kBase)), kBytes,
        true, platform::MemoryProtection::kReadWrite);
    if (!reservation.valid || reservation.base == nullptr)
    {
        return false;
    }
    if (reinterpret_cast<std::uintptr_t>(reservation.base) != kBase)
    {
        platform::ReleaseMemory(reservation.base, kBytes);
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(reservation.base);
    auto context = std::make_unique<engine::ThreadContext>();
    context->runtime_base = kBase;
    context->runtime_size = kBytes;
    context->guest_ss = 0xB4U;
    runtime::InitializeSelectorTable(&context->selector_table);
    runtime::GuestDescriptor code{0x2CU, kBase, 0xFFU, 0x9AU, true};
    code.executable = true;
    code.code_default_operand_size = runtime::GuestCodeDefaultOperandSize::k16;
    runtime::RegisterDescriptor(&context->selector_table, code);
    runtime::GuestDescriptor stack{0xB4U, kBase + 0x1000U, 0xFFFU, 0x92U, true};
    runtime::RegisterDescriptor(&context->selector_table, stack);
    struct Case
    {
        std::uint8_t opcode;
        bool dword;
        std::uint32_t expected;
    };
    const Case cases[] = {
        {0x50U, false, 0x5678U}, {0x57U, true, 0x87654321U},
        {0x54U, false, 0x0100U}, {0x54U, true, 0xA5A50100U},
        {0x0EU, false, 0x2CU}, {0x0EU, true, 0x2CU},
    };
    bool ok = true;
    for (const Case& test : cases)
    {
        std::memset(bytes, 0xCC, 16U);
        bytes[0] = test.dword ? 0x66U : test.opcode;
        if (test.dword)
        {
            bytes[1] = test.opcode;
        }
        std::memset(bytes + 0x10F8U, 0xA5, 12U);
        platform::GuestCpuContext registers{};
        registers.Eip = kBase;
        registers.Esp = 0xA5A50100U;
        registers.Eax = 0x12345678U;
        registers.Edi = 0x87654321U;
        registers.EFlags = 0x246U;
        const auto result = engine::HandleMode16StackPush(&registers, context.get());
        const std::uint32_t width = test.dword ? 4U : 2U;
        std::uint32_t observed = 0;
        std::memcpy(&observed, bytes + 0x1100U - width, width);
        ok = result.has_value() && *result &&
            registers.Eip == kBase + (test.dword ? 2U : 1U) &&
            registers.Esp == 0xA5A50100U - width &&
            registers.EFlags == 0x246U && registers.Eax == 0x12345678U &&
            observed == test.expected && bytes[0x1100U - width - 1U] == 0xA5U &&
            bytes[0x1100U] == 0xA5U && ok;
    }
    // Rejection must neither modify guest state nor overwrite the stack.
    stack.flags = 0x90U;
    runtime::RegisterDescriptor(&context->selector_table, stack);
    bytes[0] = 0x50U;
    std::memset(bytes + 0x10FCU, 0xA5, 4U);
    platform::GuestCpuContext rejected{};
    rejected.Eip = kBase;
    rejected.Esp = 0xA5A50100U;
    const auto result = engine::HandleMode16StackPush(&rejected, context.get());
    ok = result.has_value() && !*result && rejected.Eip == kBase &&
        rejected.Esp == 0xA5A50100U && bytes[0x10FEU] == 0xA5U && ok;
    platform::ReleaseMemory(reservation.base, kBytes);
    std::cout << "mode16_push_writes=" << (ok ? "true" : "false") << "\n";
    return ok;
#else
    return true;
#endif
}

}  // namespace

bool RunSegmentPushProbe()
{
    repiu::runtime::SelectorTable table;
    const bool initialized = repiu::runtime::InitializeSelectorTable(&table);
    const bool descriptors = initialized &&
        repiu::runtime::RegisterDescriptor(
            &table, {0x0024U, 0x01010000U, 0x000EBBDFU, 0U, true}) &&
        repiu::runtime::RegisterDescriptor(
            &table, {0x0080U, 0x09000000U, 0x0000914FU, 0U, true});

    std::uint16_t selector = 0;
    const bool unique = descriptors &&
        repiu::runtime::FindSelectorForLinearAddress(
            table, 0x010F0117U, &selector) && selector == 0x0024U;
    const bool absent = descriptors &&
        !repiu::runtime::FindSelectorForLinearAddress(
            table, 0x08000000U, &selector);
    const bool overlap_registered = descriptors &&
        repiu::runtime::RegisterDescriptor(
            &table, {0x0090U, 0x010F0000U, 0x000001FFU, 0U, true});
    const bool overlap_rejected = overlap_registered &&
        !repiu::runtime::FindSelectorForLinearAddress(
            table, 0x010F0117U, &selector);
    runtime::GuestDescriptor stack{0xB4U, 0x0158A83CU, 0xFFFFU, 0x92U, true};
    runtime::GuestStackPushAccess access;
    const bool word_stack = runtime::ResolveGuestStackPushAccess(
        stack, 0xA5A52000U, 2U, &access) &&
        access.next_esp == 0xA5A51FFEU && access.linear_address == 0x0158C83AU;
    const bool dword_on_word_stack = runtime::ResolveGuestStackPushAccess(
        stack, 0xA5A52000U, 4U, &access) &&
        access.next_esp == 0xA5A51FFCU && access.linear_address == 0x0158C838U;
    const bool wrap = runtime::ResolveGuestStackPushAccess(
        stack, 0xA5A50000U, 2U, &access) &&
        access.next_esp == 0xA5A5FFFEU && access.linear_address == 0x0159A83AU;
    const bool straddle_rejected = !runtime::ResolveGuestStackPushAccess(
        stack, 0xA5A50001U, 2U, &access);
    stack.flags = 0x4092U;
    stack.limit = 0xFFFFFFFFU;
    const bool big_stack_word = runtime::ResolveGuestStackPushAccess(
        stack, 0x12000U, 2U, &access) && access.next_esp == 0x11FFEU &&
        access.linear_address == 0x0159C83AU;
    const bool big_stack_dword = runtime::ResolveGuestStackPushAccess(
        stack, 0x12000U, 4U, &access) && access.next_esp == 0x11FFCU &&
        access.linear_address == 0x0159C838U;
    const bool linear_overflow = !runtime::ResolveGuestStackPushAccess(
        stack, 0U, 4U, &access);
    stack.flags = 0x96U;
    const bool expand_down_rejected = !runtime::ResolveGuestStackPushAccess(
        stack, 0x2000U, 2U, &access);
    stack.flags = 0x90U;
    const bool read_only_rejected = !runtime::ResolveGuestStackPushAccess(
        stack, 0x2000U, 2U, &access);
    stack.flags = 0x9AU;
    const bool code_rejected = !runtime::ResolveGuestStackPushAccess(
        stack, 0x2000U, 2U, &access);
    stack.flags = 0x92U;
    stack.present = false;
    const bool absent_stack = !runtime::ResolveGuestStackPushAccess(
        stack, 0x2000U, 2U, &access);
    stack.present = true;
    const bool invalid_width = !runtime::ResolveGuestStackPushAccess(
        stack, 0x2000U, 8U, &access);
    const bool stack_ok = word_stack && dword_on_word_stack && wrap &&
        straddle_rejected && big_stack_word && big_stack_dword &&
        linear_overflow && expand_down_rejected && read_only_rejected &&
        code_rejected && absent_stack && invalid_width;
    std::cout << "segment_push_stack_geometry=" << (stack_ok ? "true" : "false")
              << ",word=" << word_stack << ",dword=" << dword_on_word_stack
              << ",wrap=" << wrap << ",big_word=" << big_stack_word
              << ",big_dword=" << big_stack_dword << "\n";
    const bool writes_ok = ProbeMode16PushWrites();
    const bool all = unique && absent && overlap_rejected && stack_ok && writes_ok;

    std::cout << "segment_push_code_selector=true,unique="
              << (unique ? "true" : "false")
              << ",absent=" << (absent ? "true" : "false")
              << ",overlap_rejected="
              << (overlap_rejected ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
