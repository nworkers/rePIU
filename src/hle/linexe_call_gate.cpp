#include "repiu/hle/linexe_call_gate.h"

#include <cstddef>

namespace repiu::hle
{
namespace
{

constexpr std::uint16_t kClientDataSelector = 0x0020;
constexpr std::uint16_t kLinexeCodeSelector = 0x0080;
constexpr std::uint16_t kLinexeDataSelector = 0x0090;
constexpr std::uint16_t kPrivateRootOffset = 0x0042;
constexpr std::uint16_t kFirstGateOffset = 0x0100;
constexpr std::uint32_t kPageSize = 0x1000;
constexpr std::uint16_t kModuleOffset = 0x059A;
constexpr std::uint16_t kModuleNameOffset = 0x0504;
constexpr std::uint16_t kExportTableOffset = 0x0522;

void Write16(std::vector<std::uint8_t>* image,
             std::uint32_t offset,
             std::uint16_t value)
{
    (*image)[offset] = static_cast<std::uint8_t>(value);
    (*image)[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void WriteFar(std::vector<std::uint8_t>* image,
              std::uint32_t offset,
              std::uint16_t target_offset,
              std::uint16_t selector)
{
    Write16(image, offset, target_offset);
    Write16(image, offset + 2, selector);
}

void WriteString(std::vector<std::uint8_t>* image,
                 std::uint32_t offset,
                 const char* text)
{
    while (*text != '\0')
    {
        (*image)[offset++] = static_cast<std::uint8_t>(*text++);
    }
}

constexpr std::array<LinexeCallGate, 8> kRecoveredGates = {{
    {LinexeService::kLoadModule, "LINEXE_LOADMODULE", 0x1B28, 0},
    {LinexeService::kFreeModule, "LINEXE_FREEMODULE", 0x1B43, 0},
    {LinexeService::kGetLoadTable, "GETLOADTABLE", 0x26B9, 0},
    {LinexeService::kGetLoadName, "GETLOADNAME", 0x271F, 0},
    {LinexeService::kGetModuleHandle, "LINEXE_GETMODHANDLE", 0x1BAB, 0},
    {LinexeService::kGetProcedureAddress, "LINEXE_GETPROCADDR", 0x1B5A, 0},
    {LinexeService::kRelocate, "REL", 0x1543, 0},
    {LinexeService::kUnrelocate, "UNREL", 0x1609, 0},
}};

}  // namespace

bool BuildLinexeArenaLayout(std::uint32_t relocated_hle_reserve_base,
                            std::uint32_t arena_end,
                            LinexeArenaLayout* layout)
{
    if (layout == nullptr)
    {
        return false;
    }

    *layout = LinexeArenaLayout{};
    const std::uint64_t aligned_base =
        (static_cast<std::uint64_t>(relocated_hle_reserve_base) +
         kPageSize - 1) & ~(static_cast<std::uint64_t>(kPageSize) - 1);
    const std::uint64_t aligned_end =
        static_cast<std::uint64_t>(arena_end) &
        ~(static_cast<std::uint64_t>(kPageSize) - 1);
    const std::uint64_t client_base =
        aligned_end >= 3U * kPageSize
            ? aligned_end - 3U * kPageSize
            : 0;
    if (relocated_hle_reserve_base == 0 || arena_end == 0 ||
        aligned_base > UINT32_MAX || client_base < aligned_base ||
        aligned_end > UINT32_MAX)
    {
        layout->message = "LINEXE HLE pages do not fit in the runtime arena";
        return false;
    }

    layout->client_data_base = static_cast<std::uint32_t>(client_base);
    layout->private_data_base = layout->client_data_base + kPageSize;
    layout->gate_code_base = layout->private_data_base + kPageSize;
    layout->dynamic_allocator_base = static_cast<std::uint32_t>(aligned_base);
    layout->dynamic_allocator_end = layout->client_data_base;
    layout->arena_end = arena_end;
    layout->valid = true;
    layout->message =
        "dynamic allocator range precedes three LINEXE HLE top pages";
    return true;
}

bool BuildLinexeCallGatePlan(LinexeCallGatePlan* plan)
{
    if (plan == nullptr)
    {
        return false;
    }

    *plan = LinexeCallGatePlan{};
    plan->client_data_selector = kClientDataSelector;
    plan->linexe_code_selector = kLinexeCodeSelector;
    plan->linexe_data_selector = kLinexeDataSelector;
    plan->private_root_offset = kPrivateRootOffset;
    plan->gates = kRecoveredGates;
    plan->client_data_image.assign(kPageSize, 0);
    plan->private_data_image.assign(kPageSize, 0);
    plan->gate_image.assign(
        kFirstGateOffset + kRecoveredGates.size() * kLinexeGateStride,
        0x90);

    WriteFar(&plan->client_data_image,
             kPrivateRootOffset,
             kModuleOffset,
             kLinexeDataSelector);
    WriteString(&plan->private_data_image,
                kModuleNameOffset,
                "LINEXE_LOADER");
    WriteFar(&plan->private_data_image,
             kModuleOffset + 4,
             kModuleNameOffset,
             kLinexeDataSelector);
    Write16(&plan->private_data_image,
            kModuleOffset + 0x10,
            static_cast<std::uint16_t>(plan->gates.size()));
    WriteFar(&plan->private_data_image,
             kModuleOffset + 0x12,
             kExportTableOffset,
             kLinexeDataSelector);

    std::uint16_t next_name_offset = 0x0600;

    for (std::size_t index = 0; index < plan->gates.size(); ++index)
    {
        LinexeCallGate& gate = plan->gates[index];
        gate.gate_offset = static_cast<std::uint16_t>(
            kFirstGateOffset + index * kLinexeGateStride);
        const std::size_t image_offset = gate.gate_offset;
        plan->gate_image[image_offset] = kLinexeGateTrapOpcode0;
        plan->gate_image[image_offset + 1] = kLinexeGateTrapOpcode1;
        plan->gate_image[image_offset + 2] = static_cast<std::uint8_t>(index);
        const std::uint32_t export_offset =
            kExportTableOffset + index * 8;
        WriteFar(&plan->private_data_image,
                 export_offset,
                 next_name_offset,
                 kLinexeDataSelector);
        WriteFar(&plan->private_data_image,
                 export_offset + 4,
                 gate.gate_offset,
                 kLinexeCodeSelector);
        WriteString(&plan->private_data_image,
                    next_name_offset,
                    gate.export_name);
        while (plan->private_data_image[next_name_offset] != 0)
        {
            ++next_name_offset;
        }
        ++next_name_offset;
    }

    plan->valid = true;
    plan->message = "eight LINEXE HLE call gates are planned atomically";
    return true;
}

bool DecodeLinexeCallGate(const LinexeCallGatePlan& plan,
                          std::uint32_t gate_offset,
                          LinexeService* service)
{
    if (!plan.valid || service == nullptr)
    {
        return false;
    }

    for (const LinexeCallGate& gate : plan.gates)
    {
        if (gate.gate_offset == gate_offset)
        {
            *service = gate.service;
            return true;
        }
    }
    return false;
}

}  // namespace repiu::hle
