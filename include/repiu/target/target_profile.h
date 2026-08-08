#ifndef REPIU_TARGET_TARGET_PROFILE_H_
#define REPIU_TARGET_TARGET_PROFILE_H_

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace repiu::target
{

enum class ExecutableFormatHint
{
    kDos4gwLe,
};

struct TargetRuntimeReservationHint
{
    bool valid = false;
    std::uint32_t base_address = 0;
    std::uint32_t reserve_size = 0;
};

struct TargetProfile
{
    std::string_view id;
    std::string_view display_name;
    std::filesystem::path executable_path;
    std::filesystem::path working_directory;
    std::filesystem::path asset_root;
    ExecutableFormatHint format_hint = ExecutableFormatHint::kDos4gwLe;
    std::string_view hle_profile_id;
    std::string_view rom_set_id;
    TargetRuntimeReservationHint runtime_reservation_hint;
    bool enable_piu10_isa_board = false;
};

const std::vector<TargetProfile>& GetBuiltInTargetProfiles();

const TargetProfile* FindTargetProfileById(std::string_view id);

std::string_view ExecutableFormatHintName(ExecutableFormatHint format_hint);

}  // namespace repiu::target

#endif  // REPIU_TARGET_TARGET_PROFILE_H_
