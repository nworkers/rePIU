#include "repiu/runtime/aot_code_cache.h"

#include <Zydis.h>

#include <chrono>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace repiu::runtime
{
namespace
{

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
                                 AotCodeCacheImage* image)
{
    if (image == nullptr || instruction.bytes.size() < 2U ||
        instruction.bytes[0] != 0xFFU)
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
    image->bytes.push_back(0x9CU);  // pushfd
    image->bytes.insert(image->bytes.end(), compare.begin(), compare.end());
    site.target_immediate_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    site.guard_offset = static_cast<std::uint32_t>(image->bytes.size());
    AppendRel32(&image->bytes, 0xE9U);  // initially always miss
    image->bytes.push_back(0x90U);      // JNE uses the same six bytes
    image->bytes.push_back(0x9DU);      // popfd
    if (site.is_call)
    {
        image->bytes.push_back(0x68U);
        AppendImmediate32(&image->bytes,
                          instruction.guest_address + instruction.length);
    }
    AppendRel32(&image->bytes, 0xE9U);
    site.jump_displacement_offset =
        static_cast<std::uint32_t>(image->bytes.size() - 4U);
    site.miss_cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x9DU);  // popfd
    image->bytes.push_back(0xCCU);  // dispatcher miss
    if (!PatchRel32(&image->bytes, site.guard_offset + 1U,
                    site.miss_cache_offset))
    {
        return false;
    }
    image->indirect_inline_cache_sites.push_back(site);
    return true;
}

bool EmitReturnInlineCacheSlot(const AotInstructionRecord& instruction,
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
    AotIndirectInlineCacheSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.is_return = true;
    image->bytes.push_back(0x9CU);  // pushfd
    image->bytes.insert(image->bytes.end(),
                        {0x81U, 0x7CU, 0x24U, 0x04U});
    site.target_immediate_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    site.guard_offset = static_cast<std::uint32_t>(image->bytes.size());
    AppendRel32(&image->bytes, 0xE9U);
    image->bytes.push_back(0x90U);
    image->bytes.push_back(0x9DU);  // popfd
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
    site.jump_displacement_offset =
        static_cast<std::uint32_t>(image->bytes.size() - 4U);
    site.miss_cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x9DU);
    image->bytes.push_back(0xCCU);
    if (!PatchRel32(&image->bytes, site.guard_offset + 1U,
                    site.miss_cache_offset))
    {
        return false;
    }
    image->indirect_inline_cache_sites.push_back(site);
    return true;
}

}  // namespace

bool BuildAotCodeCacheImage(const AotTranslationPlan& plan,
                            AotCodeCacheImage* image)
{
    if (image == nullptr || !plan.valid)
    {
        return false;
    }
    *image = AotCodeCacheImage{};
    const auto started = std::chrono::steady_clock::now();
    std::unordered_map<std::uint32_t, std::uint32_t> guest_to_cache;

    for (const AotBasicBlock& block : plan.blocks)
    {
        for (const AotInstructionRecord& instruction : block.instructions)
        {
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
            switch (instruction.kind)
            {
                case AotInstructionKind::kCopy:
                    image->bytes.insert(image->bytes.end(),
                                        instruction.bytes.begin(),
                                        instruction.bytes.end());
                    break;
                case AotInstructionKind::kReturn:
                    if (!EmitReturnInlineCacheSlot(instruction, image))
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
                        AppendRel32(&image->bytes, 0xE9U);
                        image->fixups.push_back({
                            AotFixupKind::kDirectJump,
                            instruction.guest_address,
                            instruction.direct_target,
                            cache_offset + 1U, false});
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
                    image->bytes.push_back(0x0FU);
                    image->bytes.push_back(opcode);
                    image->bytes.insert(image->bytes.end(), 4U, 0U);
                    image->fixups.push_back({
                        AotFixupKind::kConditionalBranch,
                        instruction.guest_address, instruction.direct_target,
                        cache_offset + 2U, false});
                    AppendRel32(&image->bytes, 0xE9U);
                    image->fixups.push_back({
                        AotFixupKind::kDirectJump,
                        instruction.guest_address,
                        instruction.fallthrough_target,
                        cache_offset + 7U, false});
                    break;
                }
                case AotInstructionKind::kHleBoundary:
                    image->bytes.push_back(0xCCU);
                    image->fixups.push_back({AotFixupKind::kHleBoundary,
                                             instruction.guest_address, 0U,
                                             cache_offset, false});
                    break;
                case AotInstructionKind::kIndirectExit:
                    if (!EmitIndirectInlineCacheSlot(instruction, image))
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
            if (fixup.kind == AotFixupKind::kBlockFallthrough ||
                fixup.kind == AotFixupKind::kDirectCall ||
                fixup.kind == AotFixupKind::kDirectJump ||
                fixup.kind == AotFixupKind::kConditionalBranch)
            {
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

    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                     ZYDIS_STACK_WIDTH_32);
    for (const AotAddressMapEntry& map : image->address_map)
    {
        std::uint32_t decoded_bytes = 0;
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
        }
        if (decoded_bytes != map.emitted_length)
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

}  // namespace repiu::runtime
