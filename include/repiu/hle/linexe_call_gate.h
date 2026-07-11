#ifndef REPIU_HLE_LINEXE_CALL_GATE_H_
#define REPIU_HLE_LINEXE_CALL_GATE_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace repiu::hle
{

enum class LinexeService : std::uint8_t
{
    kLoadModule,
    kFreeModule,
    kGetLoadTable,
    kGetLoadName,
    kGetModuleHandle,
    kGetProcedureAddress,
    kRelocate,
    kUnrelocate,
};

struct LinexeCallGate
{
    LinexeService service = LinexeService::kLoadModule;
    const char* export_name = nullptr;
    std::uint16_t original_offset = 0;
    std::uint16_t gate_offset = 0;
};

struct LinexeCallGatePlan
{
    bool valid = false;
    std::uint16_t client_data_selector = 0;
    std::uint16_t linexe_code_selector = 0;
    std::uint16_t linexe_data_selector = 0;
    std::uint16_t private_root_offset = 0;
    std::array<LinexeCallGate, 8> gates{};
    std::vector<std::uint8_t> client_data_image;
    std::vector<std::uint8_t> private_data_image;
    std::vector<std::uint8_t> gate_image;
    std::string message;
};

struct LinexeArenaLayout
{
    bool valid = false;
    std::uint32_t client_data_base = 0;
    std::uint32_t private_data_base = 0;
    std::uint32_t gate_code_base = 0;
    std::uint32_t dynamic_allocator_base = 0;
    std::uint32_t dynamic_allocator_end = 0;
    std::uint32_t arena_end = 0;
    std::string message;
};

constexpr std::uint8_t kLinexeGateTrapOpcode0 = 0x0F;
constexpr std::uint8_t kLinexeGateTrapOpcode1 = 0x0B;
constexpr std::uint32_t kLinexeGateStride = 8;

bool BuildLinexeCallGatePlan(LinexeCallGatePlan* plan);

bool BuildLinexeArenaLayout(std::uint32_t relocated_hle_reserve_base,
                            std::uint32_t arena_end,
                            LinexeArenaLayout* layout);

bool DecodeLinexeCallGate(const LinexeCallGatePlan& plan,
                          std::uint32_t gate_offset,
                          LinexeService* service);

}  // namespace repiu::hle

#endif  // REPIU_HLE_LINEXE_CALL_GATE_H_
