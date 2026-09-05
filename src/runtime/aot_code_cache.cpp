#include "repiu/runtime/aot_code_cache.h"

#include "repiu/runtime/aot_long_mode_compatibility.h"

#if !defined(_WIN32) && defined(__x86_64__)
#include "repiu/platform/linux_x64_aot_dispatch.h"
#endif

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
                      AotCodeCacheImage* image,
                      std::size_t* const emitted_instructions)
{
    *emitted_instructions = 0U;
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
        *emitted_instructions = 1U;
        return true;
    }
    if (verdict.lowering == LongModeLowering::kNone)
    {
        return false;
    }
    std::uint8_t lowered[kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    std::size_t lowered_instructions = 0U;
    if (!LowerLongModeBytes(instruction.bytes.data(), instruction.bytes.size(),
                            lowered, &lowered_count, &lowered_instructions) ||
        lowered_count == 0U || lowered_instructions == 0U)
    {
        // A named lowering that the rewriter declines is still a refusal. It
        // happens for real encodings -- `kAbsoluteToSib` needs the disp32 to be
        // the instruction's tail, which `C7 05 disp32 imm32` is not -- so this
        // is an expected outcome rather than an internal error.
        return false;
    }
    image->bytes.insert(image->bytes.end(), lowered, lowered + lowered_count);
    ++image->long_mode_lowered_count;
    *emitted_instructions = lowered_instructions;
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


// Task 562. Where an emitted return goes to ask where a guest address lives.
//
// Zero everywhere the thunk does not exist, and zero is the answer that matters
// rather than a missing case: a host without it emits the boundary it emitted
// before, so nothing changes for i386 or Windows by this returning nothing.
std::uintptr_t LongModeReturnThunkAddress()
{
#if !defined(_WIN32) && defined(__x86_64__)
    return repiu::platform::LinuxX64ReturnThunkAddress();
#else
    return 0U;
#endif
}

// Task 560. The two control-flow kinds a long-mode host can emit as they are.
//
// `E9 rel32` and `0F 8x rel32` mean the same thing in long mode as in 32-bit
// mode, and their displacement is relative to a point inside one cache image
// far smaller than rel32's range. Nothing about them needs the guest stack or
// the dispatch resolver, which is what separates them from `kDirectCall` and
// `kReturn` -- and by count they are 6,865 of the 12,856 records the long-mode
// emitter was refusing.
//
// The timer safe point the i386 path puts in front of a backward edge is not
// emitted here. It is a hand-built 32-bit sequence and long mode reinterprets
// three of its pieces at once: `9C`/`9D` become eight-byte `pushfq`/`popfq`
// against the *host* stack pointer, and `cmp dword ptr [abs32],0` becomes
// RIP-relative. None of the three raises. The block fallthrough already leaves
// it out for exactly this reason, so this is that decision extended rather than
// a new one.
bool EmitLongModeDirectBranch(const AotInstructionRecord& instruction,
                              AotCodeCacheImage* image,
                              std::size_t* const emitted_instructions)
{
    const std::uint32_t cache_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    if (instruction.kind == AotInstructionKind::kDirectJump)
    {
        AppendRel32(&image->bytes, 0xE9U);
        image->fixups.push_back({AotFixupKind::kDirectJump,
                                 instruction.guest_address,
                                 instruction.direct_target,
                                 cache_offset + 1U, false});
        ++image->long_mode_branch_count;
        *emitted_instructions = 1U;
        return true;
    }
    // Task 562. A return, which is the first edge whose target is not known
    // until the guest runs.
    //
    // The slot pops the guest return address into the scratch, advances guest
    // ESP, and jumps to the thunk that asks a resolver where that guest address
    // lives in the cache. `LEA` again, for Task 559's reason: a guest `ret`
    // changes no flags.
    //
    // The thunk's address goes into R12 through `movabs` rather than a rel32,
    // because nothing guarantees the distance from the cache to the engine
    // image, and rather than `jmp qword ptr [rip+disp]`, because that leaves
    // eight bytes of data inside an entry the verifier decodes. R12 rather than
    // R13: R14D is already the emitter's scratch and R13 is the execution
    // harness's state pointer.
    if (instruction.kind == AotInstructionKind::kReturn)
    {
        const std::uintptr_t thunk = LongModeReturnThunkAddress();
        if (thunk == 0U)
        {
            return false;
        }
        // mov r14d, dword ptr [r15]
        image->bytes.insert(image->bytes.end(), {0x45U, 0x8BU, 0x37U});
        // lea r15d, [r15+4]
        image->bytes.insert(image->bytes.end(),
                            {0x45U, 0x8DU, 0x7FU, 0x04U});
        // movabs r12, <thunk>
        image->bytes.insert(image->bytes.end(), {0x49U, 0xBCU});
        for (std::size_t index = 0; index < 8U; ++index)
        {
            image->bytes.push_back(static_cast<std::uint8_t>(
                (static_cast<std::uint64_t>(thunk) >> (index * 8U)) & 0xFFU));
        }
        // jmp r12
        image->bytes.insert(image->bytes.end(), {0x41U, 0xFFU, 0xE4U});
        ++image->long_mode_return_count;
        *emitted_instructions = 4U;
        return true;
    }
    // Task 603. `kFarReturn` is deliberately not sent through the near-return
    // resolver. Its selector consumption and stack effect depend on the guest
    // code/stack descriptors, which the current x64 thunk does not carry.
    // Returning false leaves the caller's fail-closed INT3 boundary in place.
    if (instruction.kind == AotInstructionKind::kFarReturn)
    {
        return false;
    }
    // Task 561. A direct call is the push of a guest return address followed by
    // the same direct edge above.
    //
    // The push is synthesised as `68 <fallthrough>` and put through the stack
    // lowering rather than written out here. Task 559's sequence adjusts guest
    // ESP with `LEA` precisely because a guest `PUSH` changes no flags, and that
    // was found by getting it wrong; a second copy of the sequence in this file
    // is a second place for that to be got wrong again.
    if (instruction.kind == AotInstructionKind::kDirectCall)
    {
        std::uint8_t push[5] = {0x68U, 0U, 0U, 0U, 0U};
        for (std::size_t index = 0; index < 4U; ++index)
        {
            push[index + 1U] = static_cast<std::uint8_t>(
                (instruction.fallthrough_target >> (index * 8U)) & 0xFFU);
        }
        std::uint8_t lowered[kMaxLoweredBytes] = {};
        std::size_t lowered_count = 0U;
        std::size_t lowered_instructions = 0U;
        if (!LowerLongModeBytes(push, sizeof(push), lowered, &lowered_count,
                                &lowered_instructions) ||
            lowered_count == 0U || lowered_instructions == 0U)
        {
            return false;
        }
        image->bytes.insert(image->bytes.end(), lowered,
                            lowered + lowered_count);
        const std::uint32_t branch_offset =
            static_cast<std::uint32_t>(image->bytes.size());
        AppendRel32(&image->bytes, 0xE9U);
        image->fixups.push_back({AotFixupKind::kDirectCall,
                                 instruction.guest_address,
                                 instruction.direct_target,
                                 branch_offset + 1U, false});
        ++image->long_mode_branch_count;
        *emitted_instructions = lowered_instructions + 1U;
        return true;
    }
    if (instruction.kind != AotInstructionKind::kConditionalBranch)
    {
        return false;
    }
    std::uint8_t opcode = 0;
    if (!ReadConditionOpcode(instruction.mnemonic, &opcode))
    {
        // The same answer i386 gives: a condition this does not know how to
        // spell is a boundary, not a guess.
        return false;
    }
    image->bytes.push_back(0x0FU);
    AppendRel32(&image->bytes, opcode);
    image->fixups.push_back({AotFixupKind::kConditionalBranch,
                             instruction.guest_address,
                             instruction.direct_target,
                             cache_offset + 2U, false});
    ++image->long_mode_branch_count;
    *emitted_instructions = 1U;
    return true;
}

// Turns a long-mode branch slot whose target fell outside the cache back into a
// boundary.
//
// The whole slot is filled with INT3 rather than only its first byte, and the
// entry's intended instruction count is set to match. Writing one INT3 and
// leaving the rest was the first attempt and the verifier refused the image:
// an entry that says "one instruction" whose bytes are a trap followed by four
// stray ones is exactly the disagreement Task 559 taught that check to find.
// A slot of N traps decodes as N instructions and says what it is.
//
// The i386 path answers this case with `EmitUnresolvedDirectEdgeDispatch`, a
// `68 imm32` sequence x64 cannot use -- and with `enable_dbt_direct_edge_dispatch`
// defaulting to false it answers it by failing the whole image build instead.
// Neither is wanted here: Task 553's rule is that anything long mode cannot
// emit reaches the boundary, and an image that builds today must not stop
// building because branches were opened.
//
// The slot's bounds come from its address-map entry rather than being derived
// from the patch offset by kind, and the patch site is required to lie inside
// those bounds. Deriving them worked while a slot was one instruction and
// stopped working the moment a call became a push followed by a jump; asking
// the entry works for both, and mismatched bounds refuse rather than overwrite
// something else.
bool NeutraliseLongModeBranch(const AotCodeCacheFixup& fixup,
                              AotCodeCacheImage* image,
                              std::vector<std::uint32_t>* entry_instructions)
{
    // The block fallthrough is the one slot that is not an address-map entry of
    // its own: it is appended after the tail instruction's entry ends, so the
    // verifier never reads it and one INT3 over its `E9` is enough.
    if (fixup.kind == AotFixupKind::kBlockFallthrough)
    {
        if (fixup.cache_patch_offset < 1U ||
            fixup.cache_patch_offset - 1U >= image->bytes.size() ||
            image->bytes[fixup.cache_patch_offset - 1U] != 0xE9U)
        {
            return false;
        }
        image->bytes[fixup.cache_patch_offset - 1U] = 0xCCU;
        return true;
    }
    if (fixup.kind != AotFixupKind::kDirectJump &&
        fixup.kind != AotFixupKind::kDirectCall &&
        fixup.kind != AotFixupKind::kConditionalBranch)
    {
        return false;
    }
    // Everything else emitted by the long-mode branch path is one address-map
    // entry, and the entry is what says where the slot begins. For a call that
    // matters twice over: its push comes before its jump, so an INT3 written at
    // the jump would let guest ESP move and a return address be stored before
    // the trap -- and the boundary's handler resumes at the guest's own `call`,
    // which would push a second time.
    if (image->address_map.size() != entry_instructions->size())
    {
        return false;
    }
    for (std::size_t index = 0; index < image->address_map.size(); ++index)
    {
        const AotAddressMapEntry& entry = image->address_map[index];
        if (entry.guest_address != fixup.guest_source)
        {
            continue;
        }
        const std::size_t start = entry.cache_offset;
        const std::size_t length = entry.emitted_length;
        // The slot has to contain its own patch site, or this is not the entry
        // that emitted it.
        if (length == 0U || start + length > image->bytes.size() ||
            fixup.cache_patch_offset < start ||
            fixup.cache_patch_offset + 4U > start + length)
        {
            return false;
        }
        for (std::size_t offset = 0; offset < length; ++offset)
        {
            image->bytes[start + offset] = 0xCCU;
        }
        (*entry_instructions)[index] = static_cast<std::uint32_t>(length);
        return true;
    }
    return false;
}

void AppendImmediate32(std::vector<std::uint8_t>* bytes, std::uint32_t value)
{
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U)
    {
        bytes->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

// Task 568. Snapshot a segment-override slot's opening bytes so re-resolution
// can restore what HLE routing overwrites.
//
// The snapshot is taken from the image rather than each emitter listing its own
// head a second time. A second listing is another copy of the same fact, which
// is what this whole change exists to remove; reading the buffer back cannot
// disagree with what was written. Call once the slot's head is emitted.
template <typename Site>
void RecordGuardPrologue(const AotCodeCacheImage& image, Site* const site)
{
    if (site->cache_offset > image.bytes.size())
    {
        return;
    }
    const std::size_t available = image.bytes.size() - site->cache_offset;
    const std::size_t count =
        std::min<std::size_t>(available, kAotSegmentGuardPrologueBytes);
    std::memcpy(site->guard_prologue,
                image.bytes.data() + site->cache_offset, count);
    site->guard_prologue_size = static_cast<std::uint8_t>(count);
}

bool LongModeGuardedSegmentLoadEmittableImpl(
    const AotInstructionRecord& instruction)
{
    if (instruction.kind != AotInstructionKind::kGuardedSegmentLoad ||
        instruction.gpr_register > 7U || instruction.gpr_register == 4U)
    {
        return false;
    }
    return instruction.segment_register == 0U ||
        instruction.segment_register == 3U ||
        instruction.segment_register == 4U ||
        instruction.segment_register == 5U;
}

bool LongModeGuardedSegmentPopEmittableImpl(
    const AotInstructionRecord& instruction)
{
    if (instruction.kind != AotInstructionKind::kGuardedSegmentPop)
    {
        return false;
    }
    // The i386 slot's set. This one never installs a selector into a host
    // segment register, so FS and GS need no separate refusal here -- the same
    // reasoning Task 569 applied to the register-source load.
    return instruction.segment_register == 0U ||
        instruction.segment_register == 3U ||
        instruction.segment_register == 4U ||
        instruction.segment_register == 5U;
}

bool IsLongModeStackPointerRegister(const ZydisRegister reg)
{
    return reg == ZYDIS_REGISTER_RSP || reg == ZYDIS_REGISTER_ESP ||
        reg == ZYDIS_REGISTER_SP || reg == ZYDIS_REGISTER_SPL;
}

// Task 573. The instruction that reads an indirect call's target into R14D.
//
// It is not written out here. The operand is the guest's own, and moving a
// memory operand into long mode -- the `0x67`, and the ModRM-to-SIB rewrite for
// the absolute form -- is exactly what `LowerLongModeBytes` does. So the guest's
// `FF /2` is turned into the 32-bit `8B /r` that reads the same operand into
// ESI, and that is handed to the lowering. Task 561 synthesised its push the
// same way and for the same reason: a second copy of a rewrite is a second
// place to get it wrong.
//
// ESI because its number is `110`, which is R14's low three bits. The lowered
// bytes then need one REX.R to mean R14 instead, and nothing else changes.
//
// Returns the lowered bytes *without* the REX; the caller inserts it, because
// where it goes is a fact about the lowering's output that this function
// verifies rather than assumes.
bool LowerLongModeIndirectTargetLoad(const AotInstructionRecord& instruction,
                                     std::uint8_t* const lowered,
                                     std::size_t* const lowered_count)
{
    if (instruction.kind != AotInstructionKind::kIndirectExit ||
        instruction.bytes.size() < 2U || instruction.bytes[0] != 0xFFU)
    {
        return false;
    }
    const std::uint8_t modrm = instruction.bytes[1];
    if (((modrm >> 3U) & 0x07U) != 2U)
    {
        return false;  // not `/2`, so not a near indirect call
    }
    if ((modrm >> 6U) == 3U)
    {
        // The register form. Left closed: it does not appear in the measured
        // stops, and opening a form the census has not costed is what Task 570
        // declined to do.
        return false;
    }

    // Opcode `FF` becomes `8B`, and ModRM's `reg` goes from `010` to `110`.
    // Everything that describes the address -- mod, rm, SIB, displacement --
    // is carried over untouched, which is the point.
    std::vector<std::uint8_t> synthesised(instruction.bytes.begin(),
                                          instruction.bytes.end());
    synthesised[0] = 0x8BU;
    synthesised[1] = static_cast<std::uint8_t>((modrm & 0xC7U) | 0x30U);

    // An operand naming guest ESP takes the `kStackPointerToR15` path, which
    // inserts a REX of its own. There is only one REX, so that case cannot also
    // take the REX.R this needs; merging the two intentions is separate work
    // and the form stays refused (design decision 4).
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        synthesised.data(), synthesised.size());
    if (verdict.lowering != LongModeLowering::kAddressSizePrefix &&
        verdict.lowering != LongModeLowering::kAbsoluteToSib)
    {
        return false;
    }

    std::size_t produced = 0U;
    if (!LowerLongModeBytes(synthesised.data(), synthesised.size(), lowered,
                            &produced, nullptr) ||
        produced < 2U || produced + 1U > kMaxLoweredBytes)
    {
        return false;
    }
    // The synthesised instruction carries no prefixes of its own, so the
    // lowering's output begins with the `0x67` it prepends and then the opcode.
    // Asserted rather than assumed: it is what makes the REX's insertion point
    // a fact instead of a guess.
    if (lowered[0] != 0x67U || lowered[1] != 0x8BU)
    {
        return false;
    }
    *lowered_count = produced;
    return true;
}

bool LongModeIndirectCallEmittableImpl(
    const AotInstructionRecord& instruction)
{
    std::uint8_t lowered[kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    return LongModeReturnThunkAddress() != 0U &&
        LowerLongModeIndirectTargetLoad(instruction, lowered, &lowered_count);
}

// Task 568. Whether the long-mode segment-override slot can be emitted for this
// record, asked in one place.
//
// The emitter admits one shape out of many, and anything that needs to know how
// many records that covers -- the census, above all -- must ask rather than
// reimplement the test. Twice already a copy of an emission rule has gone stale
// and been caught by `agrees=`; this exists so there is no copy to go stale.
bool LongModeSegmentOverrideEmittable(const AotInstructionRecord& instruction,
                                      std::uint8_t* const segment_prefix,
                                      ZydisDecodedInstruction* const decoded)
{
    // FS and GS are refused: host TLS uses FS, and Task 546's decision 5 says
    // raw guest segments are never installed into host FS or GS.
    std::uint8_t prefix = 0;
    switch (instruction.segment_override_register)
    {
        case 0U: prefix = 0x26U; break;  // ES
        case 2U: prefix = 0x36U; break;  // SS
        case 3U: prefix = 0x3EU; break;  // DS
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
    if (instruction.bytes.empty() ||
        !ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder,
                                             instruction.bytes.data(),
                                             instruction.bytes.size(), &insn,
                                             operands)) ||
        insn.length != instruction.bytes.size())
    {
        return false;
    }
    // Task 570. Keep the proven absolute form and admit the first base form the
    // reachable chain needs: non-SIB mod=01 disp8. It is widened to mod=10
    // disp32 by the emitter so the live segment base has somewhere to be
    // folded. Other ModRM shapes stay closed until reachability asks for one.
    const bool absolute_disp32 = insn.raw.modrm.mod == 0U &&
        insn.raw.modrm.rm == 5U && insn.raw.disp.size == 32U;
    const bool base_disp8 = insn.raw.modrm.mod == 1U &&
        insn.raw.modrm.rm != 4U && insn.raw.disp.size == 8U;
    if ((insn.attributes & ZYDIS_ATTRIB_HAS_MODRM) == 0U ||
        (!absolute_disp32 && !base_disp8) ||
        insn.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT ||
        insn.raw.disp.offset + insn.raw.disp.size / 8U > insn.length)
    {
        return false;
    }
    // Guest ESP lives in R15D. A segment access that names ESP in another
    // operand would otherwise read or write host RSP even though its memory
    // address happened to be safe.
    for (std::uint8_t index = 0; index < insn.operand_count; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if ((operand.type == ZYDIS_OPERAND_TYPE_REGISTER &&
             IsLongModeStackPointerRegister(operand.reg.value)) ||
            (operand.type == ZYDIS_OPERAND_TYPE_MEMORY &&
             (IsLongModeStackPointerRegister(operand.mem.base) ||
              IsLongModeStackPointerRegister(operand.mem.index))))
        {
            return false;
        }
    }
    if (insn.raw.modrm.offset < insn.raw.prefix_count ||
        insn.raw.modrm.offset >= insn.length)
    {
        return false;
    }
    if (segment_prefix != nullptr)
    {
        *segment_prefix = prefix;
    }
    if (decoded != nullptr)
    {
        *decoded = insn;
    }
    return true;
}

// Task 567. The x64 segment-override slot.
//
// Task 566 measured the guest's segment bases and they are the relocated object
// bases, not zero -- so long mode ignoring the `CS`/`DS`/`ES`/`SS` overrides is
// not a convenience but the wrong address, silently. The i386 slot's answer is
// the right one here too: drop the prefix, fold the base into a `disp32`, and
// guard on the shadow selector still being what the fold assumed.
//
// What differs is that three of that slot's pieces are 32-bit-only, and each
// already has an answer this port built earlier:
//
//   `9C`/`9D`            eight-byte `pushfq`/`popfq` against the host stack
//                        -> Task 559's flags sequence through `R15D`
//   `cmp [abs32]`        RIP-relative in long mode
//                        -> Task 552's SIB absolute form
//   the access's ModRM   likewise
//                        -> the same
//
// So this composes three known rewrites rather than inventing one.
//
// The displacement and the guard's two operands are zero here and patched
// later through `AotSegmentOverrideSite`. That is not an implementation detail
// to gloss: a slot emitted and never patched reads with a base of zero, which
// is wrong in exactly the way dropping the prefix is wrong, while the census
// counts it as emitted. Emission and correctness are different things.
bool EmitLongModeSegmentOverride(const AotInstructionRecord& instruction,
                                 AotCodeCacheImage* image,
                                 std::size_t* const emitted_instructions)
{
    // Task 568. The admission test lives beside the census that also needs it.
    std::uint8_t segment_prefix = 0;
    ZydisDecodedInstruction insn{};
    if (!LongModeSegmentOverrideEmittable(instruction, &segment_prefix, &insn))
    {
        return false;
    }
    const std::uint32_t prefix_count = insn.raw.prefix_count;
    const std::uint32_t modrm_offset = insn.raw.modrm.offset;

    std::uint8_t flags_save[kMaxLoweredBytes] = {};
    std::uint8_t flags_restore[kMaxLoweredBytes] = {};
    std::size_t save_count = 0U;
    std::size_t restore_count = 0U;
    std::size_t save_instructions = 0U;
    std::size_t restore_instructions = 0U;
    const std::uint8_t pushfd = 0x9CU;
    const std::uint8_t popfd = 0x9DU;
    if (!LowerLongModeBytes(&pushfd, 1U, flags_save, &save_count,
                            &save_instructions) ||
        !LowerLongModeBytes(&popfd, 1U, flags_restore, &restore_count,
                            &restore_instructions))
    {
        return false;
    }

    AotSegmentOverrideSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_override_register;
    if (insn.raw.disp.size == 32U)
    {
        std::memcpy(&site.original_displacement,
                    instruction.bytes.data() + insn.raw.disp.offset,
                    sizeof(site.original_displacement));
    }
    else
    {
        site.original_displacement = static_cast<std::int32_t>(
            static_cast<std::int8_t>(
                instruction.bytes[insn.raw.disp.offset]));
    }

    std::size_t instructions = 0U;
    image->bytes.insert(image->bytes.end(), flags_save,
                        flags_save + save_count);
    instructions += save_instructions;

    // cmp word ptr [shadow_selector], S -- the SIB absolute form, because
    // `81 /7` with `mod=00 rm=101` would be RIP-relative here.
    image->bytes.push_back(0x67U);
    image->bytes.push_back(0x66U);
    image->bytes.push_back(0x81U);
    image->bytes.push_back(0x3CU);  // mod=00, reg=111 (/7 cmp), rm=100 (SIB)
    image->bytes.push_back(0x25U);  // scale=0, index=100 (none), base=101
    site.guard_address_offset = static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    site.guard_selector_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0x00U);
    image->bytes.push_back(0x00U);
    ++instructions;

    // je do_access, over the fallback's flags restore and its INT3.
    image->bytes.push_back(0x74U);
    image->bytes.push_back(static_cast<std::uint8_t>(restore_count + 1U));
    ++instructions;

    // The selector moved, so the fold no longer holds: restore flags and reach
    // the boundary. Task 553's rule, in the one place this slot can be wrong.
    image->bytes.insert(image->bytes.end(), flags_restore,
                        flags_restore + restore_count);
    instructions += restore_instructions;
    const std::uint32_t boundary_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    ++instructions;
    image->fixups.push_back({AotFixupKind::kHleBoundary,
                             instruction.guest_address, 0U, boundary_offset,
                             false});

    // do_access:
    image->bytes.insert(image->bytes.end(), flags_restore,
                        flags_restore + restore_count);
    instructions += restore_instructions;

    // The access itself: every prefix but the override, then `0x67`, then the
    // opcode. The absolute form moves `rm` to a no-base SIB so it does not turn
    // RIP-relative. Task 570's base+disp8 form keeps `rm` and widens `mod` to
    // disp32, preserving the guest base register.
    for (std::uint32_t index = 0; index < prefix_count; ++index)
    {
        if (instruction.bytes[index] != segment_prefix)
        {
            image->bytes.push_back(instruction.bytes[index]);
        }
    }
    image->bytes.push_back(0x67U);
    for (std::uint32_t index = prefix_count; index < modrm_offset; ++index)
    {
        image->bytes.push_back(instruction.bytes[index]);
    }
    const bool absolute_disp32 = insn.raw.modrm.mod == 0U;
    if (absolute_disp32)
    {
        image->bytes.push_back(static_cast<std::uint8_t>(
            (instruction.bytes[modrm_offset] & 0x38U) | 0x04U));
        image->bytes.push_back(0x25U);
    }
    else
    {
        image->bytes.push_back(static_cast<std::uint8_t>(
            (instruction.bytes[modrm_offset] & 0x3FU) | 0x80U));
    }
    site.displacement_offset = static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    const std::size_t original_displacement_bytes = insn.raw.disp.size / 8U;
    for (std::size_t index =
             insn.raw.disp.offset + original_displacement_bytes;
         index < instruction.bytes.size(); ++index)
    {
        image->bytes.push_back(instruction.bytes[index]);
    }
    ++instructions;

    // The way on. Its target is the next guest instruction rather than the
    // record's `fallthrough_target`, which is only set for records that end a
    // block and this one usually does not.
    const std::uint32_t branch_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendRel32(&image->bytes, 0xE9U);
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.guest_address + instruction.length,
                             branch_offset + 1U, false});
    ++instructions;

    RecordGuardPrologue(*image, &site);
    image->segment_override_sites.push_back(site);
    ++image->long_mode_segment_override_count;
    *emitted_instructions = instructions;
    return true;
}

// Task 569. A register-source segment load that changes no guest-visible
// selector state. Long mode never installs guest selectors into host segment
// registers, so the i386 physical-selector comparison is not meaningful here.
// Equality with the shadow proves this load is a no-op; a mismatch restores
// flags and reaches the existing HLE boundary.
bool EmitLongModeGuardedSegmentLoad(
    const AotInstructionRecord& instruction,
    AotCodeCacheImage* const image,
    std::size_t* const emitted_instructions)
{
    if (image == nullptr || emitted_instructions == nullptr ||
        !LongModeGuardedSegmentLoadEmittableImpl(instruction))
    {
        return false;
    }

    std::uint8_t flags_save[kMaxLoweredBytes] = {};
    std::uint8_t flags_restore[kMaxLoweredBytes] = {};
    std::size_t save_count = 0U;
    std::size_t restore_count = 0U;
    std::size_t save_instructions = 0U;
    std::size_t restore_instructions = 0U;
    const std::uint8_t pushfd = 0x9CU;
    const std::uint8_t popfd = 0x9DU;
    if (!LowerLongModeBytes(&pushfd, 1U, flags_save, &save_count,
                            &save_instructions) ||
        !LowerLongModeBytes(&popfd, 1U, flags_restore, &restore_count,
                            &restore_instructions))
    {
        return false;
    }

    AotGuardedSegmentLoadSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_register;
    site.gpr_register = instruction.gpr_register;

    std::size_t instructions = 0U;
    image->bytes.insert(image->bytes.end(), flags_save,
                        flags_save + save_count);
    instructions += save_instructions;

    // cmp word ptr [abs32], source-r16. The address-size override and absolute
    // SIB form prevent long mode from interpreting the disp32 as RIP relative.
    image->bytes.insert(image->bytes.end(), {0x67U, 0x66U, 0x39U});
    image->bytes.push_back(static_cast<std::uint8_t>(
        0x04U | (instruction.gpr_register << 3U)));
    image->bytes.push_back(0x25U);
    site.shadow_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    ++instructions;

    // Equality skips the mismatch restore and its INT3.
    image->bytes.push_back(0x74U);
    image->bytes.push_back(static_cast<std::uint8_t>(restore_count + 1U));
    ++instructions;

    image->bytes.insert(image->bytes.end(), flags_restore,
                        flags_restore + restore_count);
    instructions += restore_instructions;
    site.fallback_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    ++instructions;
    image->fixups.push_back({AotFixupKind::kHleBoundary,
                             instruction.guest_address, 0U,
                             site.fallback_offset, false});

    image->bytes.insert(image->bytes.end(), flags_restore,
                        flags_restore + restore_count);
    instructions += restore_instructions;
    const std::uint32_t branch_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendRel32(&image->bytes, 0xE9U);
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.guest_address + instruction.length,
                             branch_offset + 1U, false});
    ++instructions;

    RecordGuardPrologue(*image, &site);
    image->guarded_segment_load_sites.push_back(site);
    ++image->long_mode_guarded_segment_load_count;
    *emitted_instructions = instructions;
    return true;
}

// Task 571. A stack-sourced segment load that changes no guest-visible selector
// state. Same admission as Task 569's register-source load -- equality with the
// shadow proves the load is a no-op -- with one difference that matters: this
// instruction still has an effect after the selector is ruled out, because it
// pops. Success therefore advances guest ESP by four; a mismatch leaves the
// stack word in place so the HLE can re-execute the original instruction.
bool EmitLongModeGuardedSegmentPop(
    const AotInstructionRecord& instruction,
    AotCodeCacheImage* const image,
    std::size_t* const emitted_instructions)
{
    if (image == nullptr || emitted_instructions == nullptr ||
        !LongModeGuardedSegmentPopEmittableImpl(instruction))
    {
        return false;
    }

    std::uint8_t flags_save[kMaxLoweredBytes] = {};
    std::uint8_t flags_restore[kMaxLoweredBytes] = {};
    std::size_t save_count = 0U;
    std::size_t restore_count = 0U;
    std::size_t save_instructions = 0U;
    std::size_t restore_instructions = 0U;
    const std::uint8_t pushfd = 0x9CU;
    const std::uint8_t popfd = 0x9DU;
    if (!LowerLongModeBytes(&pushfd, 1U, flags_save, &save_count,
                            &save_instructions) ||
        !LowerLongModeBytes(&popfd, 1U, flags_restore, &restore_count,
                            &restore_instructions))
    {
        return false;
    }

    AotGuardedSegmentPopSite site;
    site.guest_source = instruction.guest_address;
    site.cache_offset = static_cast<std::uint32_t>(image->bytes.size());
    site.segment_register = instruction.segment_register;

    std::size_t instructions = 0U;
    image->bytes.insert(image->bytes.end(), flags_save,
                        flags_save + save_count);
    instructions += save_instructions;

    // mov r14d, [r15+4]. The lowered PUSHFD above pushes onto the *guest*
    // stack, so guest ESP is already four below where it entered and the word
    // this instruction pops is one slot further up. Reading [r15] here would
    // compare the saved flags against a selector.
    image->bytes.insert(image->bytes.end(), {0x45U, 0x8BU, 0x77U, 0x04U});
    ++instructions;

    // cmp r14w, word ptr [abs32]. The address-size override and absolute SIB
    // form prevent long mode from reading the disp32 as RIP relative.
    image->bytes.insert(image->bytes.end(),
                        {0x67U, 0x66U, 0x44U, 0x3BU, 0x34U, 0x25U});
    site.shadow_address_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendImmediate32(&image->bytes, 0U);
    ++instructions;

    // Equality skips the mismatch restore and its INT3.
    image->bytes.push_back(0x74U);
    image->bytes.push_back(static_cast<std::uint8_t>(restore_count + 1U));
    ++instructions;

    image->bytes.insert(image->bytes.end(), flags_restore,
                        flags_restore + restore_count);
    instructions += restore_instructions;
    site.fallback_offset = static_cast<std::uint32_t>(image->bytes.size());
    image->bytes.push_back(0xCCU);
    ++instructions;
    image->fixups.push_back({AotFixupKind::kHleBoundary,
                             instruction.guest_address, 0U,
                             site.fallback_offset, false});

    image->bytes.insert(image->bytes.end(), flags_restore,
                        flags_restore + restore_count);
    instructions += restore_instructions;
    // lea r15d, [r15+4]. The pop itself, and a LEA rather than an ADD because
    // the guest's next branch reads the flags this slot just restored.
    image->bytes.insert(image->bytes.end(), {0x45U, 0x8DU, 0x7FU, 0x04U});
    ++instructions;

    // Unlike the register-source load, this record ends its block in the
    // planner, so `fallthrough_target` is the address that was set for it.
    const std::uint32_t branch_offset =
        static_cast<std::uint32_t>(image->bytes.size());
    AppendRel32(&image->bytes, 0xE9U);
    image->fixups.push_back({AotFixupKind::kBlockFallthrough,
                             instruction.guest_address,
                             instruction.fallthrough_target,
                             branch_offset + 1U, false});
    ++instructions;

    RecordGuardPrologue(*image, &site);
    image->guarded_segment_pop_sites.push_back(site);
    ++image->long_mode_guarded_segment_pop_count;
    *emitted_instructions = instructions;
    return true;
}

// Task 573. The long-mode indirect call.
//
// Three pieces, all of which already existed: Task 572's address rewrite reads
// the target, Task 559's stack sequence pushes the return address, and Task
// 562's thunk asks a resolver where a guest address lives in the cache. That
// thunk's contract -- R14D holds a guest address, R15D is guest ESP, guest GPRs
// are in host GPRs -- has nothing return-specific in it; the name records who
// built it, not who may use it.
//
// **The order is load, push, jump, and that is required rather than tidy.**
// x86's `CALL r/m32` computes its target before pushing, so an operand that is
// ESP-relative, or that points at the word the push is about to overwrite,
// reads a different value if the push goes first.
//
// That order in turn rests on the push leaving R14D alone. `PUSH imm32` lowers
// to a `LEA` on R15D and a store, and touches no R14D -- but Task 559's
// `PUSHFD` in the same file does use R14D as scratch, so this is a premise to
// pin rather than a property to assume. The probe pins it by value.
bool EmitLongModeIndirectCall(const AotInstructionRecord& instruction,
                              AotCodeCacheImage* const image,
                              std::size_t* const emitted_instructions)
{
    const std::uintptr_t thunk = LongModeReturnThunkAddress();
    std::uint8_t target_load[kMaxLoweredBytes] = {};
    std::size_t target_load_count = 0U;
    if (thunk == 0U ||
        !LowerLongModeIndirectTargetLoad(instruction, target_load,
                                         &target_load_count))
    {
        return false;
    }

    // The return address the guest would push. The planner leaves
    // `fallthrough_target` at zero for an indirect exit -- it does not continue
    // the walk past one -- so it is computed here rather than read.
    const std::uint32_t return_address =
        instruction.guest_address + instruction.length;
    std::uint8_t push[5] = {0x68U, 0U, 0U, 0U, 0U};
    for (std::size_t index = 0; index < 4U; ++index)
    {
        push[index + 1U] = static_cast<std::uint8_t>(
            (return_address >> (index * 8U)) & 0xFFU);
    }
    std::uint8_t push_lowered[kMaxLoweredBytes] = {};
    std::size_t push_count = 0U;
    std::size_t push_instructions = 0U;
    if (!LowerLongModeBytes(push, sizeof(push), push_lowered, &push_count,
                            &push_instructions) ||
        push_count == 0U || push_instructions == 0U)
    {
        return false;
    }

    // 1. The target, into R14D. The REX.R goes after the `0x67` and before the
    // opcode, which is where REX belongs and where the verified prefix layout
    // puts it.
    image->bytes.push_back(target_load[0]);  // 0x67
    image->bytes.push_back(0x44U);           // REX.R, so reg 110 means R14
    image->bytes.insert(image->bytes.end(), target_load + 1U,
                        target_load + target_load_count);

    // 2. The return address, onto the guest stack.
    image->bytes.insert(image->bytes.end(), push_lowered,
                        push_lowered + push_count);

    // 3. The transfer, the same bytes Task 562's return slot uses.
    image->bytes.insert(image->bytes.end(), {0x49U, 0xBCU});  // movabs r12
    for (std::size_t index = 0; index < 8U; ++index)
    {
        image->bytes.push_back(static_cast<std::uint8_t>(
            (static_cast<std::uint64_t>(thunk) >> (index * 8U)) & 0xFFU));
    }
    image->bytes.insert(image->bytes.end(), {0x41U, 0xFFU, 0xE4U});  // jmp r12

    ++image->long_mode_indirect_call_count;
    *emitted_instructions = 1U + push_instructions + 2U;
    return true;
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
    RecordGuardPrologue(*image, &site);
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
    site.has_counter_operands = true;
    RecordGuardPrologue(*image, &site);
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
    site.has_counter_operands = true;
    RecordGuardPrologue(*image, &site);
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
    RecordGuardPrologue(*image, &site);
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


bool LongModeReturnDispatchAvailable()
{
    return LongModeReturnThunkAddress() != 0U;
}

bool LongModeSegmentOverrideEmittable(const AotInstructionRecord& instruction)
{
    return LongModeSegmentOverrideEmittable(instruction, nullptr, nullptr);
}

bool LongModeGuardedSegmentLoadEmittable(
    const AotInstructionRecord& instruction)
{
    return LongModeGuardedSegmentLoadEmittableImpl(instruction);
}

bool LongModeGuardedSegmentPopEmittable(
    const AotInstructionRecord& instruction)
{
    return LongModeGuardedSegmentPopEmittableImpl(instruction);
}

bool LongModeIndirectCallEmittable(const AotInstructionRecord& instruction)
{
    return LongModeIndirectCallEmittableImpl(instruction);
}

bool HostRequiresLongModeEmission()
{
#if defined(__x86_64__) || defined(_M_X64)
    return true;
#else
    return false;
#endif
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
    // Task 559. How many instructions the emitter meant each long-mode entry to
    // be, parallel to `address_map`. It stays local rather than becoming a
    // field on `AotAddressMapEntry`, because verification runs a few dozen
    // lines below in this same function and nothing after placement wants it.
    std::vector<std::uint32_t> long_mode_entry_instructions;

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
            // Task 553. On a long-mode host `kCopy` goes through the byte
            // classifier, while each non-copy kind needs its own long-mode
            // slot. Reusing a hand-built i386 slot is forbidden because long
            // mode reinterprets several of them without raising. Branches,
            // returns, segment overrides, and guarded loads were opened here
            // only after gaining dedicated sequences and execution probes.
            //
            // The whole long-mode decision lives in this one branch rather than
            // being spread through the cases below, so that reading the switch
            // still shows the i386 emitter exactly as it was.
            if (options.enable_long_mode_emission)
            {
                std::size_t emitted_instructions = 0U;
                if (EmitLongModeDirectBranch(instruction, image,
                                             &emitted_instructions) ||
                    (instruction.kind ==
                         AotInstructionKind::kGuardedSegmentLoad &&
                     EmitLongModeGuardedSegmentLoad(
                         instruction, image, &emitted_instructions)) ||
                    (instruction.kind ==
                         AotInstructionKind::kGuardedSegmentPop &&
                     EmitLongModeGuardedSegmentPop(
                         instruction, image, &emitted_instructions)) ||
                    (instruction.kind ==
                         AotInstructionKind::kIndirectExit &&
                     EmitLongModeIndirectCall(instruction, image,
                                              &emitted_instructions)) ||
                    (instruction.kind ==
                         AotInstructionKind::kSegmentOverrideMem &&
                     options.enable_long_mode_segment_override &&
                     EmitLongModeSegmentOverride(instruction, image,
                                                 &emitted_instructions)))
                {
                    map.emitted_length = static_cast<std::uint8_t>(
                        image->bytes.size() - cache_offset);
                    image->address_map.push_back(map);
                    long_mode_entry_instructions.push_back(
                        static_cast<std::uint32_t>(emitted_instructions));
                    continue;
                }
                if (instruction.kind != AotInstructionKind::kCopy ||
                    !EmitLongModeCopy(instruction, image,
                                      &emitted_instructions))
                {
                    ++image->long_mode_refused_count;
                    image->bytes.push_back(0xCCU);
                    image->fixups.push_back({AotFixupKind::kHleBoundary,
                                             instruction.guest_address, 0U,
                                             cache_offset, false});
                    emitted_instructions = 1U;  // the INT3
                }
                map.emitted_length = static_cast<std::uint8_t>(
                    image->bytes.size() - cache_offset);
                image->address_map.push_back(map);
                long_mode_entry_instructions.push_back(
                    static_cast<std::uint32_t>(emitted_instructions));
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
                case AotInstructionKind::kFarReturn:
                    image->bytes.push_back(0xCCU);
                    image->fixups.push_back({AotFixupKind::kHleBoundary,
                                             instruction.guest_address, 0U,
                                             cache_offset, false});
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
                // Task 560. On a long-mode host the slot becomes a boundary
                // rather than the 32-bit dispatch stub below, and the image
                // still builds. The refusal is counted where every other
                // long-mode refusal is counted, so the census keeps agreeing
                // with the emitter.
                if (options.enable_long_mode_emission)
                {
                    if (!NeutraliseLongModeBranch(
                            fixup, image, &long_mode_entry_instructions))
                    {
                        image->message =
                            "long-mode branch slot could not be neutralised";
                        return false;
                    }
                    ++image->long_mode_unresolved_branch_count;
                    ++image->external_fixup_count;
                    continue;
                }
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
    for (std::size_t entry_index = 0;
         entry_index < image->address_map.size(); ++entry_index)
    {
        const AotAddressMapEntry& map = image->address_map[entry_index];
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
        // the emitter knows how many instructions it meant each entry to be. So
        // the count is checked as well as the coverage, and a byte string that
        // decodes as a different number of instructions of the right total
        // length is caught.
        //
        // Task 553 wrote this as "exactly one", which held while every entry
        // was a copy, a single-instruction lowering, or one `0xCC`. Task 559's
        // stack sequences are several instructions, so the rule became the
        // emitter's own count rather than being dropped -- weakening it back to
        // total length would restore the hole 553 measured.
        const std::uint32_t expected_instructions =
            entry_index < long_mode_entry_instructions.size()
                ? long_mode_entry_instructions[entry_index]
                : 0U;
        if (decoded_bytes != map.emitted_length ||
            (options.enable_long_mode_emission &&
             decoded_instructions != expected_instructions))
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
                if (image.long_mode_emission_enabled)
                {
                    std::uint8_t flags_save[kMaxLoweredBytes] = {};
                    std::uint8_t flags_restore[kMaxLoweredBytes] = {};
                    std::size_t save_count = 0U;
                    std::size_t restore_count = 0U;
                    const std::uint8_t pushfd = 0x9CU;
                    const std::uint8_t popfd = 0x9DU;
                    if (!LowerLongModeBytes(&pushfd, 1U, flags_save,
                                            &save_count, nullptr) ||
                        !LowerLongModeBytes(&popfd, 1U, flags_restore,
                                            &restore_count, nullptr))
                    {
                        return fail(instruction.guest_address);
                    }
                    const std::uint32_t selector_load_offset =
                        slot + static_cast<std::uint32_t>(save_count);
                    const std::uint32_t shadow_offset =
                        selector_load_offset + 10U;
                    const std::uint32_t mismatch_branch_offset =
                        shadow_offset + 4U;
                    const std::uint32_t fallback_offset =
                        mismatch_branch_offset + 2U +
                        static_cast<std::uint32_t>(restore_count);
                    const std::uint32_t success_restore_offset =
                        fallback_offset + 1U;
                    const std::uint32_t stack_advance_offset =
                        success_restore_offset +
                        static_cast<std::uint32_t>(restore_count);
                    const std::uint32_t jump_offset = stack_advance_offset + 4U;
                    const std::uint32_t slot_end = jump_offset + 5U;
                    const auto fallthrough_fixup = std::find_if(
                        image.fixups.begin(), image.fixups.end(),
                        [&instruction, jump_offset](
                            const AotCodeCacheFixup& fixup) {
                            return fixup.kind ==
                                    AotFixupKind::kBlockFallthrough &&
                                fixup.guest_source == instruction.guest_address &&
                                fixup.guest_target ==
                                    instruction.fallthrough_target &&
                                fixup.cache_patch_offset == jump_offset + 1U &&
                                fixup.resolved;
                        });
                    const bool fallthrough_matches =
                        fallthrough_fixup != image.fixups.end() &&
                        fallthrough_fixup->cache_patch_offset ==
                            jump_offset + 1U;
                    if (pop_site == image.guarded_segment_pop_sites.end() ||
                        !fallthrough_matches || slot != map->cache_offset ||
                        map->emitted_length != slot_end - slot ||
                        slot_end > image.bytes.size() ||
                        pop_site->shadow_address_offset != shadow_offset ||
                        pop_site->fallback_offset != fallback_offset ||
                        pop_site->has_counter_operands ||
                        !std::equal(flags_save, flags_save + save_count,
                                    image.bytes.data() + slot) ||
                        image.bytes[selector_load_offset] != 0x45U ||
                        image.bytes[selector_load_offset + 1U] != 0x8BU ||
                        image.bytes[selector_load_offset + 2U] != 0x77U ||
                        image.bytes[selector_load_offset + 3U] != 0x04U ||
                        image.bytes[selector_load_offset + 4U] != 0x67U ||
                        image.bytes[selector_load_offset + 5U] != 0x66U ||
                        image.bytes[selector_load_offset + 6U] != 0x44U ||
                        image.bytes[selector_load_offset + 7U] != 0x3BU ||
                        image.bytes[selector_load_offset + 8U] != 0x34U ||
                        image.bytes[selector_load_offset + 9U] != 0x25U ||
                        image.bytes[mismatch_branch_offset] != 0x74U ||
                        image.bytes[mismatch_branch_offset + 1U] !=
                            static_cast<std::uint8_t>(restore_count + 1U) ||
                        !std::equal(flags_restore,
                                    flags_restore + restore_count,
                                    image.bytes.data() +
                                        mismatch_branch_offset + 2U) ||
                        image.bytes[fallback_offset] != 0xCCU ||
                        !std::equal(flags_restore,
                                    flags_restore + restore_count,
                                    image.bytes.data() + success_restore_offset) ||
                        image.bytes[stack_advance_offset] != 0x45U ||
                        image.bytes[stack_advance_offset + 1U] != 0x8DU ||
                        image.bytes[stack_advance_offset + 2U] != 0x7FU ||
                        image.bytes[stack_advance_offset + 3U] != 0x04U ||
                        image.bytes[jump_offset] != 0xE9U)
                    {
                        return fail(instruction.guest_address);
                    }
                    continue;
                }
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
