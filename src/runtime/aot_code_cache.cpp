#include "repiu/runtime/aot_code_cache.h"

#include "repiu/runtime/aot_long_mode_compatibility.h"

#include <Zydis.h>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace repiu::runtime
{
namespace
{

constexpr std::uint32_t kInlineCacheEntryCount = 4U;

bool IsBackwardEdge(const AotInstructionRecord& instruction)
{
    if (instruction.kind == AotInstructionKind::kDirectJump)
    {
        return instruction.direct_target <= instruction.guest_address;
    }
    if (instruction.kind == AotInstructionKind::kConditionalBranch)
    {
        return instruction.direct_target <= instruction.guest_address ||
               instruction.fallthrough_target <= instruction.guest_address;
    }
    return false;
}

void EmitTimerSafePoint(const AotInstructionRecord& instruction,
                        AotCodeCacheImage* image)
{
    const std::uint32_t cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    // pushfd; cmp dword ptr [abs32],0; jne trap; popfd; jmp continue;
    // trap: popfd; int3; continue:
    image->bytes.push_back(0x9CU);
    image->bytes.insert(image->bytes.end(), {0x83U, 0x3DU});
    const std::uint32_t request_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.insert(image->bytes.end(), 4U, 0U);
    image->bytes.insert(image->bytes.end(),
                        {0x00U, 0x75U, 0x03U, 0x9DU, 0xEBU, 0x02U, 0x9DU});
    const std::uint32_t breakpoint_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    image->timer_safe_point_sites.push_back({
        instruction.guest_address, cache_offset, request_address_offset,
        breakpoint_offset});
}

// Task 553. The `kCopy` path for a long-mode host.
//
// This is the point where Task 550's judgement and Task 552's rewrite stop
// standing on their own: until this function existed the emitter copied the
// guest's bytes without asking, which is the identity on i386 and is not on
// x86-64. Returns false when the bytes may not be emitted at all, and the
// caller writes the boundary the emitter already uses for everything else.
bool EmitLongModeCopy(const AotInstructionRecord& instruction,
                      AotCodeCacheImage* image)
{
    if (instruction.bytes.empty())
    {
        return false;
    }
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        instruction.bytes.data(), instruction.bytes.size());
    if (verdict.compatibility == LongModeByteCompatibility::kIdenticalBytes)
    {
        image->bytes.insert(image->bytes.end(), instruction.bytes.begin(),
                            instruction.bytes.end());
        ++image->long_mode_copied_count;
        return true;
    }
    if (verdict.lowering == LongModeLowering::kNone)
    {
        return false;
    }
    std::uint8_t lowered[kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    if (!LowerLongModeBytes(instruction.bytes.data(), instruction.bytes.size(),
                            lowered, &lowered_count) ||
        lowered_count == 0U)
    {
        // A named lowering that the rewriter declines is still a refusal. It
        // happens for real encodings -- `kAbsoluteToSib` needs the disp32 to be
        // the instruction's tail, which `C7 05 disp32 imm32` is not -- so this
        // is an expected outcome rather than an internal error.
        return false;
    }
    image->bytes.insert(image->bytes.end(), lowered, lowered + lowered_count);
    ++image->long_mode_lowered_count;
    return true;
}

bool ReadConditionOpcode(std::uint16_t mnemonic, std::uint8_t* opcode)
{
    if (opcode == nullptr)
    {
        return false;
    }
    switch (static_cast<ZydisMnemonic>(mnemonic))
    {
        case ZYDIS_MNEMONIC_JO: *opcode = 0x80U; return true;
        case ZYDIS_MNEMONIC_JNO: *opcode = 0x81U; return true;
        case ZYDIS_MNEMONIC_JB: *opcode = 0x82U; return true;
        case ZYDIS_MNEMONIC_JNB: *opcode = 0x83U; return true;
        case ZYDIS_MNEMONIC_JZ: *opcode = 0x84U; return true;
        case ZYDIS_MNEMONIC_JNZ: *opcode = 0x85U; return true;
        case ZYDIS_MNEMONIC_JBE: *opcode = 0x86U; return true;
        case ZYDIS_MNEMONIC_JNBE: *opcode = 0x87U; return true;
        case ZYDIS_MNEMONIC_JS: *opcode = 0x88U; return true;
        case ZYDIS_MNEMONIC_JNS: *opcode = 0x89U; return true;
        case ZYDIS_MNEMONIC_JP: *opcode = 0x8AU; return true;
        case ZYDIS_MNEMONIC_JNP: *opcode = 0x8BU; return true;
        case ZYDIS_MNEMONIC_JL: *opcode = 0x8CU; return true;
        case ZYDIS_MNEMONIC_JNL: *opcode = 0x8DU; return true;
        case ZYDIS_MNEMONIC_JLE: *opcode = 0x8EU; return true;
        case ZYDIS_MNEMONIC_JNLE: *opcode = 0x8FU; return true;
        default: return false;
    }
}

void AppendRel32(std::vector<std::uint8_t>* bytes, std::uint8_t opcode)
{
    bytes->push_back(opcode);
    bytes->insert(bytes->end(), 4U, 0U);
}

void AppendImmediate32(std::vector<std::uint8_t>* bytes, std::uint32_t value)
{
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U)
    {
        bytes->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

bool PatchRel32(std::vector<std::uint8_t>* bytes,
                std::uint32_t patch_offset,
                std::uint32_t target_offset)
{
    if (bytes == nullptr || patch_offset + 4U > bytes->size())
    {
        return false;
    }
    const std::int64_t displacement =
        static_cast<std::int64_t>(target_offset) - (patch_offset + 4U);
    if (displacement < std::numeric_limits<std::int32_t>::min() ||
        displacement > std::numeric_limits<std::int32_t>::max())
    {
        return false;
    }
    const std::int32_t value = static_cast<std::int32_t>(displacement);
    std::memcpy(bytes->data() + patch_offset, &value, sizeof(value));
    return true;
}

bool EmitIndirectInlineCacheSlot(const AotInstructionRecord& instruction,
                                 std::uint32_t entry_count,
                                 bool enable_call_dispatch,
                                 bool enable_jump_dispatch,
                                 AotCodeCacheImage* image)
{
    if (image == nullptr || instruction.bytes.size() < 2U ||
        instruction.bytes[0] != 0xFFU || entry_count == 0U ||
        entry_count > kInlineCacheEntryCount)
    {
        return false;
    }
    const std::uint8_t original_modrm = instruction.bytes[1];
    const std::uint8_t operation = (original_modrm >> 3U) & 0x07U;
    if (operation != 2U && operation != 4U)
    {
        return false;
    }
    const std::uint8_t mod = original_modrm >> 6U;
    const std::uint8_t rm = original_modrm & 0x07U;
    std::size_t cursor = 2U;
    bool has_sib = mod != 3U && rm == 4U;
    std::uint8_t sib = 0;
    if (has_sib)
    {
        if (cursor >= instruction.bytes.size())
        {
            return false;
        }
        sib = instruction.bytes[cursor++];
    }
    std::size_t displacement_size = 0U;
    if (mod == 1U)
    {
        displacement_size = 1U;
    }
    else if (mod == 2U ||
             (mod == 0U && (rm == 5U ||
              (has_sib && (sib & 0x07U) == 5U))))
    {
        displacement_size = 4U;
    }
    if (cursor + displacement_size != instruction.bytes.size())
    {
        return false;
    }

    std::uint8_t compare_modrm =
        static_cast<std::uint8_t>((original_modrm & 0xC7U) | 0x38U);
    const bool esp_based = mod != 3U && has_sib &&
                           (sib & 0x07U) == 4U;
    std::vector<std::uint8_t> compare;
    compare.push_back(0x81U);
    if (esp_based && mod == 0U)
    {
        compare_modrm = static_cast<std::uint8_t>(
            (compare_modrm & 0x3FU) | 0x40U);
    }
    else if (esp_based && mod == 1U)
    {
        const std::int32_t adjusted =
            static_cast<std::int8_t>(instruction.bytes[cursor]) + 4;
        if (adjusted < std::numeric_limits<std::int8_t>::min() ||
            adjusted > std::numeric_limits<std::int8_t>::max())
        {
            compare_modrm = static_cast<std::uint8_t>(
                (compare_modrm & 0x3FU) | 0x80U);
            displacement_size = 4U;
        }
    }
    compare.push_back(compare_modrm);
    if (has_sib)
    {
        compare.push_back(sib);
    }
    if (esp_based && mod == 0U)
    {
        compare.push_back(4U);
    }
    else if (esp_based && mod == 1U && displacement_size == 4U)
    {
        const std::int32_t adjusted =
            static_cast<std::int8_t>(instruction.bytes[cursor]) + 4;
        const auto* value = reinterpret_cast<const std::uint8_t*>(&adjusted);
        compare.insert(compare.end(), value, value + sizeof(adjusted));
    }
    else if (esp_based && mod == 1U)
    {
        compare.push_back(static_cast<std::uint8_t>(
            static_cast<std::int8_t>(instruction.bytes[cursor]) + 4));
    }
    else if (esp_based && mod == 2U)
    {
        std::int32_t adjusted = 0;
        std::memcpy(&adjusted, instruction.bytes.data() + cursor,
                    sizeof(adjusted));
        adjusted += 4;
        const auto* value = reinterpret_cast<const std::uint8_t*>(&adjusted);
        compare.insert(compare.end(), value, value + sizeof(adjusted));
    }
    else
    {
        compare.insert(compare.end(), instruction.bytes.begin() + cursor,
                       instruction.bytes.end());
    }

    AotIndirectInlineCacheSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.is_call = operation == 2U;
    // Task 283: gate the host-dispatch tail by instruction kind so a live A/B run
    // can bisect the Task 282 crash. When both flags are set (the default and the
    // probe's path) this is identical to the original single-flag behavior.
    const bool enable_dbt_indirect_miss_dispatch =
        site.is_call ? enable_call_dispatch : enable_jump_dispatch;
    image->bytes.push_back(0x9CU);  // pushfd
    for (std::uint32_t index = 0; index < entry_count; ++index)
    {
        AotInlineCacheEntry entry;
        entry.compare_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        image->bytes.insert(image->bytes.end(), compare.begin(), compare.end());
        entry.target_immediate_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        AppendImmediate32(&image->bytes, 0U);
        entry.guard_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        AppendRel32(&image->bytes, 0xE9U);  // initially always miss
        image->bytes.push_back(0x90U);      // JNE uses the same six bytes
        image->bytes.push_back(0x9DU);      // popfd
        if (site.is_call)
        {
            image->bytes.push_back(0x68U);
            AppendImmediate32(
                &image->bytes,
                instruction.guest_address + instruction.length);
        }
        AppendRel32(&image->bytes, 0xE9U);
        entry.jump_displacement_offset =
            static_cast<std::uint32_t>(image->bytes.size() - 4U);
        site.entries.push_back(entry);
    }
    site.target_immediate_offset = site.entries[0].target_immediate_offset;
    site.guard_offset = site.entries[0].guard_offset;
    site.jump_displacement_offset =
        site.entries[0].jump_displacement_offset;
    site.miss_cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x9DU);  // popfd
    if (!enable_dbt_indirect_miss_dispatch)
    {
        image->bytes.push_back(0xCCU);  // dispatcher miss
    }
    else
    {
        // Task 282 host-dispatch tail. The three pushed slots sit exactly where
        // the shared resolver expects them: a call's return address lands on the
        // slot the handler itself rewrites at `Esp - 4`, the miss address
        // becomes the resolved cache target, and the guest source doubles as the
        // continuation the thunk returns through.
        AotDbtIndirectDispatchSite dispatch_site;
        dispatch_site.guest_source = instruction.guest_address;
        dispatch_site.miss_cache_offset = site.miss_cache_offset;
        dispatch_site.is_call = site.is_call;
        image->bytes.push_back(0x68U);
        AppendImmediate32(
            &image->bytes,
            site.is_call ? instruction.guest_address + instruction.length : 0U);
        image->bytes.push_back(0x68U);
        dispatch_site.miss_address_immediate_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        AppendImmediate32(&image->bytes, 0U);
        image->bytes.push_back(0x68U);
        AppendImmediate32(&image->bytes, instruction.guest_address);
        AppendRel32(&image->bytes, 0xE9U);
        dispatch_site.thunk_displacement_offset =
            static_cast<std::uint32_t>(image->bytes.size() - 4U);
        dispatch_site.fallback_cache_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        image->bytes.insert(image->bytes.end(), {0x8DU, 0x64U, 0x24U, 0x08U});
        image->bytes.push_back(0xCCU);
        dispatch_site.success_cache_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        if (site.is_call)
        {
            image->bytes.push_back(0xC3U);
        }
        else
        {
            image->bytes.push_back(0xC2U);
            image->bytes.push_back(0x04U);
            image->bytes.push_back(0x00U);
        }
        image->dbt_indirect_dispatch_sites.push_back(dispatch_site);
    }
    for (const AotInlineCacheEntry& entry : site.entries)
    {
        if (!PatchRel32(&image->bytes, entry.guard_offset + 1U,
                        AotInlineCacheGuardTargetOffset(site)))
        {
            return false;
        }
    }
    image->indirect_inline_cache_sites.push_back(site);
    return true;
}

bool EmitJumpTableSlot(const AotInstructionRecord& instruction,
                       AotCodeCacheImage* image)
{
    const std::size_t entry_count = instruction.table_targets.size();
    if (image == nullptr || entry_count == 0U || entry_count > 61U ||
        instruction.table_index_register > 7U ||
        instruction.table_index_register == 4U)
    {
        return false;
    }
    AotJumpTableSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.guest_targets = instruction.table_targets;
    image->bytes.push_back(0xFFU);  // jmp dword ptr [index*4 + table]
    image->bytes.push_back(0x24U);
    image->bytes.push_back(static_cast<std::uint8_t>(
        0x80U | (instruction.table_index_register << 3U) | 0x05U));
    site.displacement_patch_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    site.fallback_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);  // unresolved entries dispatch here
    site.table_cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    for (std::size_t index = 0; index < entry_count; ++index)
    {
        AppendImmediate32(&image->bytes, 0U);
    }
    image->jump_table_sites.push_back(std::move(site));
    return true;
}

bool EmitReturnInlineCacheSlot(const AotInstructionRecord& instruction,
                               bool enable_dbt_return_miss_dispatch,
                               bool enable_direct_return_table,
                               AotCodeCacheImage* image)
{
    if (image == nullptr || instruction.bytes.empty() ||
        (instruction.bytes[0] != 0xC3U &&
         (instruction.bytes[0] != 0xC2U || instruction.bytes.size() != 3U)))
    {
        return false;
    }
    std::uint32_t pop_bytes = 4U;
    if (instruction.bytes[0] == 0xC2U)
    {
        pop_bytes += static_cast<std::uint32_t>(instruction.bytes[1]) |
                     (static_cast<std::uint32_t>(instruction.bytes[2]) << 8U);
    }
    if (enable_dbt_return_miss_dispatch && pop_bytes > 0xFFFFU)
    {
        return false;
    }
    AotIndirectInlineCacheSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.is_return = true;
    image->bytes.push_back(0x9CU);  // pushfd
    for (std::uint32_t index = 0; index < kInlineCacheEntryCount;
         ++index)
    {
        AotInlineCacheEntry entry;
        entry.compare_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        image->bytes.insert(image->bytes.end(),
                            {0x81U, 0x7CU, 0x24U, 0x04U});
        entry.target_immediate_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        AppendImmediate32(&image->bytes, 0U);
        entry.guard_offset = static_cast<std::uint32_t>(image->bytes.size());
        AppendRel32(&image->bytes, 0xE9U);  // initially always miss
        image->bytes.push_back(0x90U);      // JNE uses the same six bytes
        image->bytes.push_back(0x9DU);      // popfd
        if (pop_bytes <= 0x7FU)
        {
            image->bytes.insert(image->bytes.end(),
                                {0x8DU, 0x64U, 0x24U,
                                 static_cast<std::uint8_t>(pop_bytes)});
        }
        else
        {
            image->bytes.insert(image->bytes.end(),
                                {0x8DU, 0xA4U, 0x24U});
            AppendImmediate32(&image->bytes, pop_bytes);
        }
        AppendRel32(&image->bytes, 0xE9U);
        entry.jump_displacement_offset =
            static_cast<std::uint32_t>(image->bytes.size() - 4U);
        site.entries.push_back(entry);
    }
    site.target_immediate_offset = site.entries[0].target_immediate_offset;
    site.guard_offset = site.entries[0].guard_offset;
    site.jump_displacement_offset =
        site.entries[0].jump_displacement_offset;
    // Task 499: the probe precedes the miss tail so a guard reaches it first
    // and a probe miss falls straight through. `miss_cache_offset` therefore
    // keeps pointing at the popfd below, which every existing consumer keys on.
    if (enable_direct_return_table && enable_dbt_return_miss_dispatch)
    {
        AotDirectReturnProbeSite probe_site;
        if (EmitAotDirectReturnProbe(&image->bytes, instruction.guest_address,
                                     pop_bytes, &probe_site))
        {
            site.miss_probe_cache_offset = probe_site.cache_offset;
            image->direct_return_probe_sites.push_back(probe_site);
        }
    }
    site.miss_cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x9DU);
    if (!enable_dbt_return_miss_dispatch)
    {
        image->bytes.push_back(0xCCU);
    }
    else
    {
        AotDbtReturnDispatchSite dispatch_site;
        dispatch_site.guest_source = instruction.guest_address;
        dispatch_site.miss_cache_offset = site.miss_cache_offset;
        image->bytes.push_back(0x68U);
        dispatch_site.miss_address_immediate_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        AppendImmediate32(&image->bytes, 0U);
        image->bytes.push_back(0x68U);
        AppendImmediate32(&image->bytes, instruction.guest_address);
        AppendRel32(&image->bytes, 0xE9U);
        dispatch_site.thunk_displacement_offset =
            static_cast<std::uint32_t>(image->bytes.size() - 4U);
        dispatch_site.fallback_cache_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        image->bytes.insert(image->bytes.end(), {0x8DU, 0x64U, 0x24U, 0x04U});
        image->bytes.push_back(0xCCU);
        dispatch_site.success_cache_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        image->bytes.push_back(0xC2U);
        image->bytes.push_back(static_cast<std::uint8_t>(pop_bytes));
        image->bytes.push_back(static_cast<std::uint8_t>(pop_bytes >> 8U));
        image->dbt_return_dispatch_sites.push_back(dispatch_site);
    }
    for (const AotInlineCacheEntry& entry : site.entries)
    {
        if (!PatchRel32(&image->bytes, entry.guard_offset + 1U,
                        AotInlineCacheGuardTargetOffset(site)))
        {
            return false;
        }
    }
    image->indirect_inline_cache_sites.push_back(site);
    return true;
}

bool EmitHleDispatchSlot(const AotInstructionRecord& instruction,
                         AotCodeCacheImage* image);

// Task 264 Phase 3a. Translate a segment-override memory access natively:
// pushfd; cmp word [shadow selector], S; je do_access; (fallback) popfd; int3;
// do_access: popfd; <access with the segment prefix removed>; jmp fallthrough.
// The guard falls back to the companion HLE slot (or INT3 when disabled) on
// a selector mismatch, so the Win32-baked base/selector is self-corrected.
// First slice: only forms that already carry a 32-bit displacement (the segment base is folded
// into it at placement, no ModRM re-encode); every other form returns false so
// the caller emits a boundary (current behavior). Returns true if emitted.
bool EmitSegmentOverrideSlot(const AotInstructionRecord& instruction,
                             AotCodeCacheImage* image,
                             bool enable_hybrid_dispatch)
{
    if (image == nullptr || instruction.bytes.empty())
    {
        return false;
    }
    std::uint8_t segment_prefix = 0;
    switch (instruction.segment_override_register)
    {
        case 0U: segment_prefix = 0x26U; break; // ES
        case 2U: segment_prefix = 0x36U; break; // SS
        case 3U: segment_prefix = 0x3EU; break; // DS
        case 4U: segment_prefix = 0x64U; break; // FS
        case 5U: segment_prefix = 0x65U; break; // GS
        default: return false;
    }
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                                       ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }
    ZydisDecodedInstruction insn{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, instruction.bytes.data(), instruction.bytes.size(),
            &insn, operands)) ||
        insn.length != instruction.bytes.size())
    {
        return false;
    }
    // A memory ModRM (mod != 3). `disp.size` is in bits (0, 8, or 32); the disp8
    // and no-displacement forms are widened to disp32 so the segment base can be
    // folded into the displacement. rm==101 (disp32, no base) and SIB base==101
    // report disp.size 32 and are kept as-is.
    if ((insn.attributes & ZYDIS_ATTRIB_HAS_MODRM) == 0U ||
        insn.raw.modrm.mod == 3U)
    {
        return false;
    }
    const std::uint32_t prefix_count = insn.raw.prefix_count;
    const std::uint32_t modrm_offset = insn.raw.modrm.offset;
    const std::uint32_t disp_bits = insn.raw.disp.size;
    if (modrm_offset < prefix_count || modrm_offset >= insn.length ||
        (disp_bits != 0U && disp_bits != 8U && disp_bits != 32U))
    {
        return false;
    }
    std::size_t segment_index = prefix_count;
    for (std::size_t i = 0;
         i < prefix_count && i < instruction.bytes.size(); ++i)
    {
        if (instruction.bytes[i] == segment_prefix)
        {
            segment_index = i;
            break;
        }
    }
    if (segment_index >= prefix_count)
    {
        return false; // the segment-override prefix was not located
    }
    const bool sib_present = insn.raw.modrm.rm == 4U;
    const std::uint32_t modrm_sib_end =
        modrm_offset + 1U + (sib_present ? 1U : 0U);
    const std::uint32_t disp_bytes = disp_bits / 8U;
    const std::uint32_t immediate_offset =
        disp_bits != 0U ? insn.raw.disp.offset + disp_bytes : modrm_sib_end;
    if (immediate_offset > insn.length)
    {
        return false;
    }
    std::vector<std::uint8_t> access;
    access.reserve(instruction.bytes.size() + 4U);
    for (std::size_t i = 0; i < prefix_count; ++i)
    {
        if (i != segment_index)
        {
            access.push_back(instruction.bytes[i]); // prefixes minus override
        }
    }
    for (std::uint32_t i = prefix_count; i < modrm_offset; ++i)
    {
        access.push_back(instruction.bytes[i]);      // opcode bytes
    }
    const std::uint8_t original_modrm = instruction.bytes[modrm_offset];
    // Keep the ModRM when it already carries a disp32; otherwise force mod=10 so
    // a disp32 field exists (reg/rm/SIB are preserved).
    access.push_back(disp_bits == 32U
                         ? original_modrm
                         : static_cast<std::uint8_t>(
                               (original_modrm & 0x3FU) | 0x80U));
    if (sib_present)
    {
        access.push_back(instruction.bytes[modrm_offset + 1U]);
    }
    std::int32_t displacement_value = 0;
    if (disp_bits == 32U)
    {
        std::memcpy(&displacement_value,
                    instruction.bytes.data() + insn.raw.disp.offset,
                    sizeof(displacement_value));
    }
    else if (disp_bits == 8U)
    {
        displacement_value = static_cast<std::int32_t>(
            static_cast<std::int8_t>(
                instruction.bytes[insn.raw.disp.offset]));
    }
    const std::uint32_t displacement_offset_in_access =
        static_cast<std::uint32_t>(access.size());
    const auto* displacement_bytes =
        reinterpret_cast<const std::uint8_t*>(&displacement_value);
    access.insert(access.end(), displacement_bytes, displacement_bytes + 4U);
    for (std::size_t i = immediate_offset; i < instruction.bytes.size(); ++i)
    {
        access.push_back(instruction.bytes[i]);      // immediate bytes
    }

    AotSegmentOverrideSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_override_register;
    site.original_displacement = displacement_value;
    image->bytes.push_back(0x9CU);           // pushfd
    image->bytes.push_back(0x66U);           // cmp word [abs32], imm16
    image->bytes.push_back(0x81U);
    image->bytes.push_back(0x3DU);
    site.guard_address_offset = static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);    // shadow-selector address (patched)
    site.guard_selector_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x00U);           // selector S (patched)
    image->bytes.push_back(0x00U);
    image->bytes.push_back(0x74U);           // je do_access
    image->bytes.push_back(enable_hybrid_dispatch ? 0x06U : 0x02U);
    image->bytes.push_back(0x9DU);           // fallback: popfd
    std::uint32_t dispatch_jump_patch = 0U;
    if (enable_hybrid_dispatch)
    {
        image->bytes.push_back(0xE9U);       // jmp companion HLE slot
        dispatch_jump_patch = static_cast<std::uint32_t>(image->bytes.size());
        AppendImmediate32(&image->bytes, 0U);
    }
    else
    {
        image->bytes.push_back(0xCCU);       // int3 -> single-step original
    }
    image->bytes.push_back(0x9DU);           // do_access: popfd
    site.displacement_offset =
        static_cast<std::uint32_t>(image->bytes.size()) +
        displacement_offset_in_access;
    image->bytes.insert(image->bytes.end(), access.begin(), access.end());
    AppendRel32(&image->bytes, 0xE9U);       // fallthrough jump
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.fallthrough_target,
                             static_cast<std::uint32_t>(image->bytes.size() - 4U),
                             false});
    if (enable_hybrid_dispatch)
    {
        site.dispatch_cache_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        if (!EmitHleDispatchSlot(instruction, image))
        {
            return false;
        }
        const std::int32_t relative = static_cast<std::int32_t>(
            site.dispatch_cache_offset - (dispatch_jump_patch + 4U));
        std::memcpy(image->bytes.data() + dispatch_jump_patch,
                    &relative, sizeof(relative));
    }
    image->segment_override_sites.push_back(site);
    return true;
}

bool EmitGuardedSegmentPopSlot(const AotInstructionRecord& instruction,
                               AotCodeCacheImage* image)
{
    if (image == nullptr || instruction.bytes.empty() ||
        (instruction.segment_register != 0U &&
         instruction.segment_register != 3U &&
         instruction.segment_register != 4U &&
         instruction.segment_register != 5U))
    {
        return false;
    }
    AotGuardedSegmentPopSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_register;
    image->bytes.push_back(0x9CU); // pushfd
    image->bytes.push_back(0x50U); // push eax
    image->bytes.push_back(0x8CU); // mov ax, Sreg
    image->bytes.push_back(static_cast<std::uint8_t>(
        0xC0U | (instruction.segment_register << 3U)));
    image->bytes.insert(image->bytes.end(),
                        {0x66U, 0x3BU, 0x44U, 0x24U, 0x08U});
    image->bytes.insert(image->bytes.end(), {0x75U, 0x1AU});
    image->bytes.insert(image->bytes.end(), {0x66U, 0x3BU, 0x05U});
    site.shadow_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(), {0x75U, 0x11U});
    image->bytes.insert(image->bytes.end(), {0xFFU, 0x05U});
    site.success_counter_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(),
                        {0x58U, 0x9DU, 0x8DU, 0x64U, 0x24U, 0x04U});
    AppendRel32(&image->bytes, 0xE9U);
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.fallthrough_target,
                             static_cast<std::uint32_t>(image->bytes.size() - 4U),
                             false});
    image->bytes.insert(image->bytes.end(), {0xFFU, 0x05U});
    site.fallback_counter_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(), {0x58U, 0x9DU});
    site.fallback_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    image->guarded_segment_pop_sites.push_back(site);
    return true;
}

bool EmitGuardedSegmentLoadSlot(const AotInstructionRecord& instruction,
                                AotCodeCacheImage* image)
{
    if (image == nullptr || instruction.gpr_register > 7U ||
        instruction.gpr_register == 4U ||
        (instruction.segment_register != 0U &&
         instruction.segment_register != 3U &&
         instruction.segment_register != 4U &&
         instruction.segment_register != 5U))
    {
        return false;
    }
    AotGuardedSegmentLoadSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_register;
    site.gpr_register = instruction.gpr_register;
    image->bytes.push_back(0x9CU);  // pushfd
    image->bytes.push_back(0x50U);  // push eax
    image->bytes.push_back(0x66U);  // mov ax,Sreg
    image->bytes.push_back(0x8CU);
    image->bytes.push_back(static_cast<std::uint8_t>(
        0xC0U | (instruction.segment_register << 3U)));
    image->bytes.insert(image->bytes.end(), {0x66U, 0x3BU});
    if (instruction.gpr_register == 0U)
    {
        image->bytes.insert(image->bytes.end(), {0x04U, 0x24U});
    }
    else
    {
        image->bytes.push_back(static_cast<std::uint8_t>(
            0xC0U | instruction.gpr_register));
        image->bytes.push_back(0x90U);
    }
    image->bytes.insert(image->bytes.end(), {0x75U, 0x16U});
    image->bytes.insert(image->bytes.end(), {0x66U, 0x3BU, 0x05U});
    site.shadow_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(), {0x75U, 0x0DU, 0xFFU, 0x05U});
    site.success_counter_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(), {0x58U, 0x9DU});
    AppendRel32(&image->bytes, 0xE9U);
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.fallthrough_target,
                             static_cast<std::uint32_t>(image->bytes.size() - 4U),
                             false});
    image->bytes.insert(image->bytes.end(), {0xFFU, 0x05U});
    site.fallback_counter_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(), {0x58U, 0x9DU});
    site.fallback_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    image->guarded_segment_load_sites.push_back(site);
    return true;
}

bool EmitGuardedSegmentReadSlot(const AotInstructionRecord& instruction,
                                AotCodeCacheImage* image)
{
    if (image == nullptr || instruction.gpr_register > 7U ||
        (instruction.segment_register != 0U &&
         instruction.segment_register != 2U &&
         instruction.segment_register != 3U &&
         instruction.segment_register != 4U &&
         instruction.segment_register != 5U))
    {
        return false;
    }
    AotGuardedSegmentReadSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_register;
    site.gpr_register = instruction.gpr_register;

    image->bytes.insert(image->bytes.end(), {0x9CU, 0x50U});
    image->bytes.insert(image->bytes.end(), {0x66U, 0x8CU});
    image->bytes.push_back(static_cast<std::uint8_t>(
        0xC0U | (instruction.segment_register << 3U)));
    image->bytes.insert(image->bytes.end(), {0x66U, 0x3BU, 0x05U});
    site.shadow_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.insert(image->bytes.end(), {0x75U, 0x0EU, 0x58U, 0x9DU});
    image->bytes.insert(image->bytes.end(), {0x66U, 0x8BU});
    image->bytes.push_back(static_cast<std::uint8_t>(
        0x05U | (instruction.gpr_register << 3U)));
    site.load_shadow_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    AppendRel32(&image->bytes, 0xE9U);
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.fallthrough_target,
                             static_cast<std::uint32_t>(image->bytes.size() - 4U),
                             false});
    site.fallback_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.insert(image->bytes.end(), {0x58U, 0x9DU, 0xCCU});
    image->guarded_segment_read_sites.push_back(site);
    return true;
}

bool EmitHleDispatchSlot(const AotInstructionRecord& instruction,
                         AotCodeCacheImage* image)
{
    if (image == nullptr)
    {
        return false;
    }
    AotDbtHleDispatchSite site;
    site.guest_source = instruction.guest_address;
    site.dispatch_cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x68U);
    site.dispatch_address_immediate_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.push_back(0x68U);
    AppendImmediate32(&image->bytes, instruction.guest_address);
    AppendRel32(&image->bytes, 0xE9U);
    site.thunk_displacement_offset =
        static_cast<std::uint32_t>(image->bytes.size() - 4U);
    site.fallback_cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.insert(image->bytes.end(), {0x8DU, 0x64U, 0x24U, 0x04U});
    const std::uint32_t fallback_int3 =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    site.success_cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xC3U);
    image->fixups.push_back({AotFixupKind::kHleBoundary,
                             instruction.guest_address, 0U,
                             fallback_int3, false});
    image->dbt_hle_dispatch_sites.push_back(site);
    return true;
}

bool IsDirectEdgeFixup(AotFixupKind kind)
{
    return kind == AotFixupKind::kBlockFallthrough ||
        kind == AotFixupKind::kDirectCall ||
        kind == AotFixupKind::kDirectJump ||
        kind == AotFixupKind::kConditionalBranch;
}

bool EmitUnresolvedDirectEdgeDispatch(AotCodeCacheFixup* fixup,
                                      AotCodeCacheImage* image)
{
    if (fixup == nullptr || image == nullptr ||
        !IsDirectEdgeFixup(fixup->kind) ||
        fixup->cache_patch_offset + 4U > image->bytes.size())
    {
        return false;
    }
    const std::size_t original_size = image->bytes.size();
    AotDbtDirectEdgeDispatchSite site;
    site.guest_source = fixup->guest_source;
    site.guest_target = fixup->guest_target;
    site.dispatch_cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x68U);
    site.dispatch_address_immediate_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    image->bytes.push_back(0x68U);
    AppendImmediate32(&image->bytes, fixup->guest_target);
    AppendRel32(&image->bytes, 0xE9U);
    site.thunk_displacement_offset =
        static_cast<std::uint32_t>(image->bytes.size() - 4U);
    image->bytes.insert(image->bytes.end(), {0x8DU, 0x64U, 0x24U, 0x04U});
    site.fallback_cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    site.success_cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xC3U);
    if (!PatchRel32(&image->bytes, fixup->cache_patch_offset,
                    site.dispatch_cache_offset))
    {
        image->bytes.resize(original_size);
        return false;
    }
    fixup->resolved = true;
    image->dbt_direct_edge_dispatch_sites.push_back(site);
    return true;
}
}  // namespace

// Task 499. Fourteen instructions that resolve a megamorphic return without
// crossing to the host. Entered with the site's pushfd still on the stack:
// [esp] = flags, [esp+4] = the guest return target.
//
// The resolved target travels through the guest stack slot rather than a global
// scratch word, because the host injects timer interrupts asynchronously and
// could otherwise overwrite a global between the store and the jump. The
// closing RET reproduces the original instruction's stack effect exactly, the
// same technique the existing success continuation uses.
bool EmitAotDirectReturnProbe(std::vector<std::uint8_t>* bytes,
                              const std::uint32_t guest_source,
                              const std::uint32_t pop_bytes,
                              AotDirectReturnProbeSite* site)
{
    if (bytes == nullptr || site == nullptr || pop_bytes < 4U ||
        pop_bytes > 0xFFFFU + 4U)
    {
        return false;
    }
    *site = AotDirectReturnProbeSite{};
    site->guest_source = guest_source;
    site->cache_offset = static_cast<std::uint32_t>(bytes->size());
    bytes->push_back(0x50U);  // push eax
    bytes->push_back(0x51U);  // push ecx
    // mov eax, [esp+12] -- the guest return target under ecx, eax, and flags.
    bytes->insert(bytes->end(), {0x8BU, 0x44U, 0x24U, 0x0CU});
    bytes->insert(bytes->end(), {0x8BU, 0xC8U});         // mov ecx, eax
    bytes->insert(bytes->end(), {0xC1U, 0xE9U, 0x0DU});  // shr ecx, 13
    bytes->insert(bytes->end(), {0x33U, 0xC8U});         // xor ecx, eax
    bytes->insert(bytes->end(), {0x81U, 0xE1U});         // and ecx, imm32
    site->mask_immediate_offset = static_cast<std::uint32_t>(bytes->size());
    AppendImmediate32(bytes, 0U);
    // cmp [ecx*8 + table], eax
    bytes->insert(bytes->end(), {0x39U, 0x04U, 0xCDU});
    site->key_address_offset = static_cast<std::uint32_t>(bytes->size());
    AppendImmediate32(bytes, 0U);
    bytes->push_back(0x75U);  // jne .miss
    const std::size_t miss_rel8_offset = bytes->size();
    bytes->push_back(0U);
    // mov ecx, [ecx*8 + table + 4]
    bytes->insert(bytes->end(), {0x8BU, 0x0CU, 0xCDU});
    site->target_address_offset = static_cast<std::uint32_t>(bytes->size());
    AppendImmediate32(bytes, 0U);
    // mov [esp+12], ecx -- overwrite the guest return slot with the cache
    // target so the RET below jumps there.
    bytes->insert(bytes->end(), {0x89U, 0x4CU, 0x24U, 0x0CU});
    bytes->insert(bytes->end(), {0xFFU, 0x05U});  // inc dword ptr [counter]
    site->hit_counter_address_offset =
        static_cast<std::uint32_t>(bytes->size());
    AppendImmediate32(bytes, 0U);
    bytes->push_back(0x59U);  // pop ecx
    bytes->push_back(0x58U);  // pop eax
    bytes->push_back(0x9DU);  // popfd
    if (pop_bytes == 4U)
    {
        bytes->push_back(0xC3U);
    }
    else
    {
        const std::uint32_t immediate = pop_bytes - 4U;
        bytes->push_back(0xC2U);
        bytes->push_back(static_cast<std::uint8_t>(immediate));
        bytes->push_back(static_cast<std::uint8_t>(immediate >> 8U));
    }
    const std::size_t miss_offset = bytes->size();
    bytes->push_back(0x59U);  // pop ecx
    bytes->push_back(0x58U);  // pop eax
    const std::ptrdiff_t relative = static_cast<std::ptrdiff_t>(miss_offset) -
        static_cast<std::ptrdiff_t>(miss_rel8_offset + 1U);
    if (relative < 0 || relative > 127)
    {
        return false;
    }
    (*bytes)[miss_rel8_offset] = static_cast<std::uint8_t>(relative);
    return true;
}

bool PatchAotDirectReturnProbe(std::uint8_t* bytes,
                               const std::size_t byte_count,
                               const AotDirectReturnProbeSite& site,
                               const std::uint32_t key_address,
                               const std::uint32_t mask,
                               const std::uint32_t hit_counter_address)
{
    if (bytes == nullptr || site.mask_immediate_offset + 4U > byte_count ||
        site.key_address_offset + 4U > byte_count ||
        site.target_address_offset + 4U > byte_count ||
        site.hit_counter_address_offset + 4U > byte_count)
    {
        return false;
    }
    const std::uint32_t target_address = key_address + 4U;
    std::memcpy(bytes + site.mask_immediate_offset, &mask, sizeof(mask));
    std::memcpy(bytes + site.key_address_offset, &key_address,
                sizeof(key_address));
    std::memcpy(bytes + site.target_address_offset, &target_address,
                sizeof(target_address));
    std::memcpy(bytes + site.hit_counter_address_offset, &hit_counter_address,
                sizeof(hit_counter_address));
    return true;
}


bool BuildAotCodeCacheImage(const AotTranslationPlan& plan,
                            AotCodeCacheImage* image)
{
    return BuildAotCodeCacheImage(plan, AotCodeCacheBuildOptions{}, image);
}

bool BuildAotCodeCacheImage(const AotTranslationPlan& plan,
                            const AotCodeCacheBuildOptions& options,
                            AotCodeCacheImage* image)
{
    if (image == nullptr || !plan.valid ||
        options.indirect_inline_cache_entry_count == 0U ||
        options.indirect_inline_cache_entry_count > kInlineCacheEntryCount)
    {
        return false;
    }
    *image = AotCodeCacheImage{};
    image->indirect_inline_cache_entry_count =
        options.indirect_inline_cache_entry_count;
    image->dbt_return_miss_dispatch_enabled =
        options.enable_dbt_return_miss_dispatch;
    image->direct_return_table_enabled = options.enable_direct_return_table;
    image->direct_return_table_bits = options.direct_return_table_bits;
    image->dbt_hle_dispatch_enabled =
        options.enable_dbt_hle_dispatch;
    image->dbt_port_io_dispatch_enabled =
        options.enable_dbt_port_io_dispatch;
    image->dbt_segment_override_dispatch_enabled =
        options.enable_dbt_segment_override_dispatch;
    image->dbt_indirect_miss_dispatch_enabled =
        options.enable_dbt_indirect_miss_dispatch;
    image->dbt_direct_edge_dispatch_enabled =
        options.enable_dbt_direct_edge_dispatch;
    image->timer_safe_points_enabled = options.enable_timer_safe_points;
    image->long_mode_emission_enabled = options.enable_long_mode_emission;
    const auto started = std::chrono::steady_clock::now();
    image->guarded_segment_pop_enabled =
        options.enable_guarded_segment_pop;
    image->guarded_segment_read_enabled =
        options.enable_guarded_segment_read;
    image->guarded_segment_load_enabled =
        options.enable_guarded_segment_load;
    std::unordered_map<std::uint32_t, std::uint32_t> guest_to_cache;

    for (const AotBasicBlock& block : plan.blocks)
    {
        for (std::size_t instruction_index = 0;
             instruction_index < block.instructions.size(); ++instruction_index)
        {
            const AotInstructionRecord& instruction =
                block.instructions[instruction_index];
            const std::uint32_t cache_offset =
                static_cast<std::uint32_t>(image->bytes.size());
            if (!guest_to_cache.emplace(
                    instruction.guest_address, cache_offset).second)
            {
                continue;
            }
            AotAddressMapEntry map;
            map.guest_address = instruction.guest_address;
            map.cache_offset = cache_offset;
            map.guest_length = instruction.length;
            // Task 553. On a long-mode host the emitter's subset is `kCopy`
            // alone. Every other kind's slot is a hand-built 32-bit sequence,
            // and long mode reinterprets several of them without raising, so
            // they reach the boundary here rather than being emitted wrong.
            //
            // The whole long-mode decision lives in this one branch rather than
            // being spread through the cases below, so that reading the switch
            // still shows the i386 emitter exactly as it was.
            if (options.enable_long_mode_emission)
            {
                if (instruction.kind != AotInstructionKind::kCopy ||
                    !EmitLongModeCopy(instruction, image))
                {
                    ++image->long_mode_refused_count;
                    image->bytes.push_back(0xCCU);
                    image->fixups.push_back({AotFixupKind::kHleBoundary,
                                             instruction.guest_address, 0U,
                                             cache_offset, false});
                }
                map.emitted_length = static_cast<std::uint8_t>(
                    image->bytes.size() - cache_offset);
                image->address_map.push_back(map);
                continue;
            }
            switch (instruction.kind)
            {
                case AotInstructionKind::kCopy:
                    image->bytes.insert(image->bytes.end(),
                                        instruction.bytes.begin(),
                                        instruction.bytes.end());
                    break;
                case AotInstructionKind::kReturn:
                    if (!EmitReturnInlineCacheSlot(
                            instruction,
                            options.enable_dbt_return_miss_dispatch,
                            options.enable_direct_return_table, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kIndirectExit,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kDirectCall:
                case AotInstructionKind::kDirectJump:
                {
                    if (instruction.kind == AotInstructionKind::kDirectCall)
                    {
                        image->bytes.push_back(0x68U);
                        AppendImmediate32(&image->bytes,
                                          instruction.fallthrough_target);
                        AppendRel32(&image->bytes, 0xE9U);
                        image->fixups.push_back({
                            AotFixupKind::kDirectCall,
                            instruction.guest_address,
                            instruction.direct_target,
                            cache_offset + 6U, false});
                    }
                    else
                    {
                        if (options.enable_timer_safe_points &&
                            IsBackwardEdge(instruction))
                        {
                            EmitTimerSafePoint(instruction, image);
                        }
                        const std::uint32_t branch_offset =
                            static_cast<std::uint32_t>(image->bytes.size());
                        AppendRel32(&image->bytes, 0xE9U);
                        image->fixups.push_back({
                            AotFixupKind::kDirectJump,
                            instruction.guest_address,
                            instruction.direct_target,
                            branch_offset + 1U, false});
                    }
                    break;
                }
                case AotInstructionKind::kConditionalBranch:
                {
                    std::uint8_t opcode = 0;
                    if (!ReadConditionOpcode(instruction.mnemonic, &opcode))
                    {
                        image->bytes.push_back(0xCCU);
                        ++image->unsupported_branch_count;
                        image->fixups.push_back({
                            AotFixupKind::kConditionalBranch,
                            instruction.guest_address,
                            instruction.direct_target, cache_offset, false});
                        break;
                    }
                    if (options.enable_timer_safe_points &&
                        IsBackwardEdge(instruction))
                    {
                        EmitTimerSafePoint(instruction, image);
                    }
                    const std::uint32_t branch_offset =
                        static_cast<std::uint32_t>(image->bytes.size());
                    image->bytes.push_back(0x0FU);
                    image->bytes.push_back(opcode);
                    image->bytes.insert(image->bytes.end(), 4U, 0U);
                    image->fixups.push_back({
                        AotFixupKind::kConditionalBranch,
                        instruction.guest_address, instruction.direct_target,
                        branch_offset + 2U, false});
                    AppendRel32(&image->bytes, 0xE9U);
                    image->fixups.push_back({
                        AotFixupKind::kDirectJump,
                        instruction.guest_address,
                        instruction.fallthrough_target,
                        branch_offset + 7U, false});
                    break;
                }
                case AotInstructionKind::kHleBoundary:
                    if (!options.enable_dbt_hle_dispatch ||
                        !EmitHleDispatchSlot(instruction, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({
                            AotFixupKind::kHleBoundary,
                            instruction.guest_address, 0U,
                            cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kPortIo:
                    if ((!options.enable_dbt_hle_dispatch &&
                         !options.enable_dbt_port_io_dispatch) ||
                        !EmitHleDispatchSlot(instruction, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({
                            AotFixupKind::kHleBoundary,
                            instruction.guest_address, 0U,
                            cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kSegmentOverrideMem:
                    if (!EmitSegmentOverrideSlot(
                            instruction, image,
                            options.enable_dbt_segment_override_dispatch))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kHleBoundary,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kGuardedSegmentPop:
                    if (!options.enable_guarded_segment_pop ||
                        !EmitGuardedSegmentPopSlot(instruction, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kHleBoundary,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kGuardedSegmentLoad:
                    if (!options.enable_guarded_segment_load ||
                        !EmitGuardedSegmentLoadSlot(instruction, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kHleBoundary,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kGuardedSegmentRead:
                    if (!options.enable_guarded_segment_read ||
                        !EmitGuardedSegmentReadSlot(instruction, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kHleBoundary,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kIndirectExit:
                    if (!EmitIndirectInlineCacheSlot(
                            instruction,
                            options.indirect_inline_cache_entry_count,
                            options.enable_dbt_indirect_miss_dispatch &&
                                options.enable_dbt_indirect_dispatch_calls,
                            options.enable_dbt_indirect_miss_dispatch &&
                                options.enable_dbt_indirect_dispatch_jumps,
                            image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kIndirectExit,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
                case AotInstructionKind::kJumpTable:
                    if (!EmitJumpTableSlot(instruction, image))
                    {
                        image->bytes.push_back(0xCCU);
                        image->fixups.push_back({AotFixupKind::kIndirectExit,
                                                 instruction.guest_address, 0U,
                                                 cache_offset, false});
                    }
                    break;
            }
            map.emitted_length = static_cast<std::uint8_t>(
                image->bytes.size() - cache_offset);
            image->address_map.push_back(map);
        }
        if (block.instructions.empty() ||
            block.instructions.back().kind != AotInstructionKind::kCopy ||
            image->address_map.empty() ||
            image->address_map.back().guest_address !=
                block.instructions.back().guest_address)
        {
            continue;
        }
        const AotInstructionRecord& tail = block.instructions.back();
        const std::uint32_t target = tail.guest_address + tail.length;
        // The `E9 rel32` below is emitted in both modes: its encoding and its
        // meaning are the same in long mode. The timer safe point in front of
        // it is not -- it is a hand-built 32-bit `pushfd`/`popfd` sequence, so
        // a long-mode image goes without one.
        if (options.enable_timer_safe_points &&
            !options.enable_long_mode_emission &&
            target <= tail.guest_address)
        {
            EmitTimerSafePoint(tail, image);
        }
        image->bytes.push_back(0xE9U);
        const std::uint32_t patch_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        image->bytes.insert(image->bytes.end(), 4U, 0U);
        image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                                 tail.guest_address, target,
                                 patch_offset, false});
    }

    for (AotCodeCacheFixup& fixup : image->fixups)
    {
        if (fixup.kind == AotFixupKind::kHleBoundary ||
            fixup.kind == AotFixupKind::kIndirectExit ||
            (fixup.kind == AotFixupKind::kConditionalBranch &&
             image->bytes[fixup.cache_patch_offset] == 0xCCU))
        {
            ++image->external_fixup_count;
            continue;
        }
        const auto target = guest_to_cache.find(fixup.guest_target);
        if (target == guest_to_cache.end() ||
            !PatchRel32(&image->bytes, fixup.cache_patch_offset,
                        target->second))
        {
            if (IsDirectEdgeFixup(fixup.kind))
            {
                if (options.enable_dbt_direct_edge_dispatch &&
                    EmitUnresolvedDirectEdgeDispatch(&fixup, image))
                {
                    ++image->resolved_fixup_count;
                    ++image->external_fixup_count;
                    continue;
                }
                image->message =
                    "direct control-flow target is outside the cache";
                return false;
            }
            ++image->external_fixup_count;
            continue;
        }
        fixup.resolved = true;
        ++image->resolved_fixup_count;
    }

    const auto entry = guest_to_cache.find(plan.entry_address);
    if (entry == guest_to_cache.end() || image->bytes.empty())
    {
        image->message = "code cache has no mapped entry point";
        return false;
    }
    image->entry_cache_offset = entry->second;

    // The mode the emitted bytes are for, which is not always the mode the
    // guest's bytes came from. A lowered instruction carries a `0x67`, which
    // means a 16-bit address size in 32-bit mode, so a 32-bit decoder reads a
    // long-mode image as different instructions than the ones emitted
    // (Task 553). It does not necessarily *say* so -- see the count check
    // below, which is what makes this mode switch observable.
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder,
                     options.enable_long_mode_emission
                         ? ZYDIS_MACHINE_MODE_LONG_64
                         : ZYDIS_MACHINE_MODE_LEGACY_32,
                     options.enable_long_mode_emission
                         ? ZYDIS_STACK_WIDTH_64
                         : ZYDIS_STACK_WIDTH_32);
    for (const AotAddressMapEntry& map : image->address_map)
    {
        std::uint32_t decoded_bytes = 0;
        std::uint32_t decoded_instructions = 0;
        while (decoded_bytes < map.emitted_length)
        {
            ZydisDecodedInstruction instruction{};
            const std::size_t available =
                map.emitted_length - decoded_bytes;
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(
                    &decoder, nullptr,
                    image->bytes.data() + map.cache_offset + decoded_bytes,
                    available, &instruction)) ||
                instruction.length == 0U)
            {
                break;
            }
            decoded_bytes += instruction.length;
            ++decoded_instructions;
        }
        // Task 553. Covering the bytes is not the same as decoding them as
        // intended, and this loop was measured failing to tell the difference:
        // the SIB absolute form `67 8B 04 25 <disp32>` reads in 32-bit mode as
        // a three-byte `mov` followed by a five-byte `and`, which covers all
        // eight bytes and reports nothing. Total length is a weak check by
        // itself.
        //
        // Under long-mode emission there is a stronger one available, because
        // every entry on that path is exactly one instruction -- a copy, a
        // lowering, or one `0xCC`. Nothing on it emits a sequence. So the count
        // is checked as well as the coverage, and a byte string that decodes as
        // two instructions of the right total length is caught.
        if (decoded_bytes != map.emitted_length ||
            (options.enable_long_mode_emission && decoded_instructions != 1U))
        {
            ++image->decode_failure_count;
        }
    }
    image->elapsed_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    image->valid = image->decode_failure_count == 0U;
    image->message = image->valid
        ? "non-executable AOT code cache image is ready"
        : "emitted code cache failed decode verification";
    return image->valid;
}

bool ValidateAotCodeCacheHleCoverage(
    const AotTranslationPlan& plan,
    const AotCodeCacheImage& image,
    std::uint32_t* failure_guest_address)
{
    if (failure_guest_address != nullptr)
    {
        *failure_guest_address = 0U;
    }
    if (!plan.valid || !image.valid)
    {
        return false;
    }
    const auto fail = [failure_guest_address](std::uint32_t guest) {
        if (failure_guest_address != nullptr)
        {
            *failure_guest_address = guest;
        }
        return false;
    };
    std::uint32_t checked = 0U;
    for (const AotBasicBlock& block : plan.blocks)
    {
        for (const AotInstructionRecord& instruction : block.instructions)
        {
            if (instruction.kind != AotInstructionKind::kHleBoundary &&
                instruction.kind != AotInstructionKind::kPortIo &&
                instruction.kind != AotInstructionKind::kSegmentOverrideMem &&
                instruction.kind != AotInstructionKind::kGuardedSegmentPop &&
                instruction.kind != AotInstructionKind::kGuardedSegmentRead &&
                instruction.kind != AotInstructionKind::kGuardedSegmentLoad)
            {
                continue;
            }
            ++checked;
            const auto map = std::find_if(
                image.address_map.begin(), image.address_map.end(),
                [&instruction](const AotAddressMapEntry& entry) {
                    return entry.guest_address == instruction.guest_address;
                });
            if (map == image.address_map.end() ||
                map->cache_offset >= image.bytes.size())
            {
                return fail(instruction.guest_address);
            }
            if (image.bytes[map->cache_offset] == 0xCCU)
            {
                continue;
            }
            if (instruction.kind == AotInstructionKind::kGuardedSegmentPop)
            {
                const auto pop_site = std::find_if(
                    image.guarded_segment_pop_sites.begin(),
                    image.guarded_segment_pop_sites.end(),
                    [&instruction](const AotGuardedSegmentPopSite& candidate) {
                        return candidate.guest_source ==
                            instruction.guest_address;
                    });
                const std::uint32_t slot = pop_site !=
                    image.guarded_segment_pop_sites.end()
                        ? pop_site->cache_offset : 0U;
                const auto fallthrough_fixup = std::find_if(
                    image.fixups.begin(), image.fixups.end(),
                    [&instruction, slot](const AotCodeCacheFixup& fixup) {
                        return fixup.kind == AotFixupKind::kBlockFallthrough &&
                            fixup.guest_source == instruction.guest_address &&
                            fixup.guest_target ==
                                instruction.fallthrough_target &&
                            fixup.cache_patch_offset == slot + 33U &&
                            fixup.resolved;
                    });
                const std::uint8_t expected_modrm = static_cast<std::uint8_t>(
                    0xC0U | (instruction.segment_register << 3U));
                if (pop_site == image.guarded_segment_pop_sites.end() ||
                    fallthrough_fixup == image.fixups.end() ||
                    slot != map->cache_offset || map->emitted_length != 46U ||
                    slot + 46U > image.bytes.size() ||
                    pop_site->shadow_address_offset != slot + 14U ||
                    pop_site->success_counter_address_offset != slot + 22U ||
                    pop_site->fallback_counter_address_offset != slot + 39U ||
                    pop_site->fallback_offset != slot + 45U ||
                    image.bytes[slot] != 0x9CU ||
                    image.bytes[slot + 1U] != 0x50U ||
                    image.bytes[slot + 2U] != 0x8CU ||
                    image.bytes[slot + 3U] != expected_modrm ||
                    image.bytes[slot + 4U] != 0x66U ||
                    image.bytes[slot + 5U] != 0x3BU ||
                    image.bytes[slot + 6U] != 0x44U ||
                    image.bytes[slot + 7U] != 0x24U ||
                    image.bytes[slot + 8U] != 0x08U ||
                    image.bytes[slot + 9U] != 0x75U ||
                    image.bytes[slot + 10U] != 0x1AU ||
                    image.bytes[slot + 11U] != 0x66U ||
                    image.bytes[slot + 12U] != 0x3BU ||
                    image.bytes[slot + 13U] != 0x05U ||
                    image.bytes[slot + 18U] != 0x75U ||
                    image.bytes[slot + 19U] != 0x11U ||
                    image.bytes[slot + 20U] != 0xFFU ||
                    image.bytes[slot + 21U] != 0x05U ||
                    image.bytes[slot + 26U] != 0x58U ||
                    image.bytes[slot + 27U] != 0x9DU ||
                    image.bytes[slot + 28U] != 0x8DU ||
                    image.bytes[slot + 29U] != 0x64U ||
                    image.bytes[slot + 30U] != 0x24U ||
                    image.bytes[slot + 31U] != 0x04U ||
                    image.bytes[slot + 32U] != 0xE9U ||
                    image.bytes[slot + 37U] != 0xFFU ||
                    image.bytes[slot + 38U] != 0x05U ||
                    image.bytes[slot + 43U] != 0x58U ||
                    image.bytes[slot + 44U] != 0x9DU ||
                    image.bytes[slot + 45U] != 0xCCU)
                {
                    return fail(instruction.guest_address);
                }
                continue;
            }
            if (instruction.kind == AotInstructionKind::kGuardedSegmentLoad)
            {
                const auto site = std::find_if(
                    image.guarded_segment_load_sites.begin(),
                    image.guarded_segment_load_sites.end(),
                    [&instruction](const AotGuardedSegmentLoadSite& candidate) {
                        return candidate.guest_source ==
                            instruction.guest_address;
                    });
                const std::uint32_t slot = site !=
                    image.guarded_segment_load_sites.end()
                        ? site->cache_offset : 0U;
                const auto fallthrough_fixup = std::find_if(
                    image.fixups.begin(), image.fixups.end(),
                    [&instruction, slot](const AotCodeCacheFixup& fixup) {
                        return fixup.kind == AotFixupKind::kBlockFallthrough &&
                            fixup.guest_source == instruction.guest_address &&
                            fixup.guest_target ==
                                instruction.fallthrough_target &&
                            fixup.cache_patch_offset == slot + 29U &&
                            fixup.resolved;
                    });
                const std::uint8_t expected_physical_modrm =
                    static_cast<std::uint8_t>(
                        0xC0U | (instruction.segment_register << 3U));
                const std::uint8_t expected_source_modrm =
                    instruction.gpr_register == 0U
                        ? 0x04U
                        : static_cast<std::uint8_t>(
                            0xC0U | instruction.gpr_register);
                const std::uint8_t expected_source_tail =
                    instruction.gpr_register == 0U ? 0x24U : 0x90U;
                if (site == image.guarded_segment_load_sites.end() ||
                    fallthrough_fixup == image.fixups.end() ||
                    slot != map->cache_offset || map->emitted_length != 42U ||
                    slot + 42U > image.bytes.size() ||
                    site->shadow_address_offset != slot + 14U ||
                    site->success_counter_address_offset != slot + 22U ||
                    site->fallback_counter_address_offset != slot + 35U ||
                    site->fallback_offset != slot + 41U ||
                    image.bytes[slot] != 0x9CU ||
                    image.bytes[slot + 1U] != 0x50U ||
                    image.bytes[slot + 2U] != 0x66U ||
                    image.bytes[slot + 3U] != 0x8CU ||
                    image.bytes[slot + 4U] != expected_physical_modrm ||
                    image.bytes[slot + 5U] != 0x66U ||
                    image.bytes[slot + 6U] != 0x3BU ||
                    image.bytes[slot + 7U] != expected_source_modrm ||
                    image.bytes[slot + 8U] != expected_source_tail ||
                    image.bytes[slot + 9U] != 0x75U ||
                    image.bytes[slot + 10U] != 0x16U ||
                    image.bytes[slot + 11U] != 0x66U ||
                    image.bytes[slot + 12U] != 0x3BU ||
                    image.bytes[slot + 13U] != 0x05U ||
                    image.bytes[slot + 18U] != 0x75U ||
                    image.bytes[slot + 19U] != 0x0DU ||
                    image.bytes[slot + 20U] != 0xFFU ||
                    image.bytes[slot + 21U] != 0x05U ||
                    image.bytes[slot + 26U] != 0x58U ||
                    image.bytes[slot + 27U] != 0x9DU ||
                    image.bytes[slot + 28U] != 0xE9U ||
                    image.bytes[slot + 33U] != 0xFFU ||
                    image.bytes[slot + 34U] != 0x05U ||
                    image.bytes[slot + 39U] != 0x58U ||
                    image.bytes[slot + 40U] != 0x9DU ||
                    image.bytes[slot + 41U] != 0xCCU)
                {
                    return fail(instruction.guest_address);
                }
                continue;
            }
            if (instruction.kind == AotInstructionKind::kGuardedSegmentRead)
            {
                const auto site = std::find_if(
                    image.guarded_segment_read_sites.begin(),
                    image.guarded_segment_read_sites.end(),
                    [&instruction](const AotGuardedSegmentReadSite& candidate) {
                        return candidate.guest_source == instruction.guest_address;
                    });
                const std::uint32_t slot = site !=
                    image.guarded_segment_read_sites.end()
                        ? site->cache_offset : 0U;
                const auto fallthrough_fixup = std::find_if(
                    image.fixups.begin(), image.fixups.end(),
                    [&instruction, slot](const AotCodeCacheFixup& fixup) {
                        return fixup.kind == AotFixupKind::kBlockFallthrough &&
                            fixup.guest_source == instruction.guest_address &&
                            fixup.guest_target == instruction.fallthrough_target &&
                            fixup.cache_patch_offset == slot + 24U &&
                            fixup.resolved;
                    });
                const std::uint8_t expected_physical_modrm =
                    static_cast<std::uint8_t>(
                        0xC0U | (instruction.segment_register << 3U));
                const std::uint8_t expected_load_modrm =
                    static_cast<std::uint8_t>(
                        0x05U | (instruction.gpr_register << 3U));
                if (site == image.guarded_segment_read_sites.end() ||
                    fallthrough_fixup == image.fixups.end() ||
                    slot != map->cache_offset || map->emitted_length != 31U ||
                    slot + 31U > image.bytes.size() ||
                    site->shadow_address_offset != slot + 8U ||
                    site->load_shadow_address_offset != slot + 19U ||
                    site->fallback_offset != slot + 28U ||
                    image.bytes[slot] != 0x9CU ||
                    image.bytes[slot + 1U] != 0x50U ||
                    image.bytes[slot + 2U] != 0x66U ||
                    image.bytes[slot + 3U] != 0x8CU ||
                    image.bytes[slot + 4U] != expected_physical_modrm ||
                    image.bytes[slot + 5U] != 0x66U ||
                    image.bytes[slot + 6U] != 0x3BU ||
                    image.bytes[slot + 7U] != 0x05U ||
                    image.bytes[slot + 12U] != 0x75U ||
                    image.bytes[slot + 13U] != 0x0EU ||
                    image.bytes[slot + 14U] != 0x58U ||
                    image.bytes[slot + 15U] != 0x9DU ||
                    image.bytes[slot + 16U] != 0x66U ||
                    image.bytes[slot + 17U] != 0x8BU ||
                    image.bytes[slot + 18U] != expected_load_modrm ||
                    image.bytes[slot + 23U] != 0xE9U ||
                    image.bytes[slot + 28U] != 0x58U ||
                    image.bytes[slot + 29U] != 0x9DU ||
                    image.bytes[slot + 30U] != 0xCCU)
                {
                    return fail(instruction.guest_address);
                }
                continue;
            }
            if (instruction.kind == AotInstructionKind::kHleBoundary ||
                instruction.kind == AotInstructionKind::kPortIo)
            {
                const auto site = std::find_if(
                    image.dbt_hle_dispatch_sites.begin(),
                    image.dbt_hle_dispatch_sites.end(),
                    [&instruction](const AotDbtHleDispatchSite& candidate) {
                        return candidate.guest_source ==
                            instruction.guest_address;
                    });
                const auto fallback_fixup = std::find_if(
                    image.fixups.begin(), image.fixups.end(),
                    [&instruction, &map](const AotCodeCacheFixup& fixup) {
                        return fixup.kind == AotFixupKind::kHleBoundary &&
                            fixup.guest_source == instruction.guest_address &&
                            fixup.cache_patch_offset ==
                                map->cache_offset + 19U;
                    });
                const std::uint32_t slot = map->cache_offset;
                if (site == image.dbt_hle_dispatch_sites.end() ||
                    fallback_fixup == image.fixups.end() ||
                    site->dispatch_cache_offset != slot ||
                    site->dispatch_address_immediate_offset != slot + 1U ||
                    site->thunk_displacement_offset != slot + 11U ||
                    site->fallback_cache_offset != slot + 15U ||
                    site->success_cache_offset != slot + 20U ||
                    map->emitted_length != 21U ||
                    slot + 21U > image.bytes.size() ||
                    image.bytes[slot] != 0x68U ||
                    image.bytes[slot + 5U] != 0x68U ||
                    image.bytes[slot + 10U] != 0xE9U ||
                    image.bytes[slot + 15U] != 0x8DU ||
                    image.bytes[slot + 16U] != 0x64U ||
                    image.bytes[slot + 17U] != 0x24U ||
                    image.bytes[slot + 18U] != 0x04U ||
                    image.bytes[slot + 19U] != 0xCCU ||
                    image.bytes[slot + 20U] != 0xC3U)
                {
                    return fail(instruction.guest_address);
                }
                continue;
            }
            if (instruction.kind != AotInstructionKind::kSegmentOverrideMem)
            {
                return fail(instruction.guest_address);
            }
            const auto site = std::find_if(
                image.segment_override_sites.begin(),
                image.segment_override_sites.end(),
                [&instruction](const AotSegmentOverrideSite& candidate) {
                    return candidate.guest_source == instruction.guest_address;
                });
            if (site == image.segment_override_sites.end() ||
                site->cache_offset != map->cache_offset ||
                site->guard_selector_offset + 5U >= image.bytes.size() ||
                image.bytes[site->cache_offset] != 0x9CU ||
                image.bytes[site->guard_selector_offset + 2U] != 0x74U ||
                image.bytes[site->guard_selector_offset + 4U] != 0x9DU)
            {
                return fail(instruction.guest_address);
            }
            if (!image.dbt_segment_override_dispatch_enabled)
            {
                if (site->dispatch_cache_offset != 0U ||
                    image.bytes[site->guard_selector_offset + 3U] != 0x02U ||
                    image.bytes[site->guard_selector_offset + 5U] != 0xCCU)
                {
                    return fail(instruction.guest_address);
                }
                continue;
            }
            const auto dispatch = std::find_if(
                image.dbt_hle_dispatch_sites.begin(),
                image.dbt_hle_dispatch_sites.end(),
                [&instruction, &site](const AotDbtHleDispatchSite& candidate) {
                    return candidate.guest_source == instruction.guest_address &&
                        candidate.dispatch_cache_offset ==
                            site->dispatch_cache_offset;
                });
            const auto fallback_fixup = std::find_if(
                image.fixups.begin(), image.fixups.end(),
                [&instruction, &dispatch, &image](
                    const AotCodeCacheFixup& fixup) {
                    return dispatch != image.dbt_hle_dispatch_sites.end() &&
                        fixup.kind == AotFixupKind::kHleBoundary &&
                        fixup.guest_source == instruction.guest_address &&
                        fixup.cache_patch_offset ==
                            dispatch->fallback_cache_offset + 4U;
                });
            std::int32_t dispatch_relative = 0;
            std::memcpy(&dispatch_relative,
                        image.bytes.data() + site->guard_selector_offset + 6U,
                        sizeof(dispatch_relative));
            const std::uint32_t dispatch_target = static_cast<std::uint32_t>(
                site->guard_selector_offset + 10U + dispatch_relative);
            if (dispatch == image.dbt_hle_dispatch_sites.end() ||
                fallback_fixup == image.fixups.end() ||
                site->dispatch_cache_offset == 0U ||
                dispatch_target != site->dispatch_cache_offset ||
                image.bytes[site->guard_selector_offset + 3U] != 0x06U ||
                image.bytes[site->guard_selector_offset + 5U] != 0xE9U ||
                dispatch->dispatch_address_immediate_offset !=
                    site->dispatch_cache_offset + 1U ||
                dispatch->thunk_displacement_offset !=
                    site->dispatch_cache_offset + 11U ||
                dispatch->fallback_cache_offset !=
                    site->dispatch_cache_offset + 15U ||
                dispatch->success_cache_offset !=
                    site->dispatch_cache_offset + 20U ||
                site->dispatch_cache_offset + 21U > image.bytes.size() ||
                image.bytes[site->dispatch_cache_offset] != 0x68U ||
                image.bytes[site->dispatch_cache_offset + 5U] != 0x68U ||
                image.bytes[site->dispatch_cache_offset + 10U] != 0xE9U ||
                image.bytes[site->dispatch_cache_offset + 19U] != 0xCCU ||
                image.bytes[site->dispatch_cache_offset + 20U] != 0xC3U)
            {
                return fail(instruction.guest_address);
            }
        }
    }
    return checked == plan.hle_boundary_count;
}

}  // namespace repiu::runtime
