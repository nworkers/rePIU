#include "repiu/storage/nvram_path.h"

#include <cstdlib>

namespace repiu::storage
{
namespace
{

constexpr const char* kLegacyEepromFileName = "eeprom.dat";
constexpr const char* kEepromFileName = "eeprom.dat";

std::filesystem::path EnvironmentPath(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
    {
        return std::filesystem::path();
    }
    return std::filesystem::path(value);
}

// Mirrors how the cfg directory is located, so the two per-ROM-set trees are
// found by the same rule: the first candidate that exists wins, and the
// working-directory one is the fallback the caller may create.
std::filesystem::path DiscoverNvramRoot(
    const std::filesystem::path& executable_directory)
{
    const std::filesystem::path override_root =
        EnvironmentPath("REPIU_NVRAM_DIR");
    if (!override_root.empty())
    {
        return override_root;
    }

    std::error_code error;
    const std::filesystem::path working_directory_candidate = "nvram";
    if (std::filesystem::is_directory(working_directory_candidate, error))
    {
        return working_directory_candidate;
    }

    if (!executable_directory.empty())
    {
        const std::filesystem::path executable_candidate =
            executable_directory / "nvram";
        if (std::filesystem::is_directory(executable_candidate, error))
        {
            return executable_candidate;
        }
    }

    return working_directory_candidate;
}

}  // namespace

EepromPathResult ResolveEepromPath(
    std::string_view rom_set_id,
    const std::filesystem::path& executable_directory)
{
    EepromPathResult result;

    const std::filesystem::path override_path =
        EnvironmentPath("REPIU_EEPROM_PATH");
    if (!override_path.empty())
    {
        result.path = override_path;
        result.from_override = true;
        // The override names a file outright, so its directory is the caller's
        // responsibility -- benchmark scripts already create it before copying
        // their fixture in.
        return result;
    }

    if (rom_set_id.empty())
    {
        result.path = kLegacyEepromFileName;
        return result;
    }

    const std::filesystem::path directory =
        DiscoverNvramRoot(executable_directory) / std::string(rom_set_id);
    result.path = directory / kEepromFileName;

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error && !std::filesystem::is_directory(directory))
    {
        result.warnings.push_back("could not create NVRAM directory " +
                                  directory.string() + ": " + error.message());
        return result;
    }

    if (std::filesystem::exists(result.path, error))
    {
        return result;
    }

    // Every ROM set shared one eeprom.dat before this layout existed, so that
    // file holds the cabinet settings for whichever set is being launched.
    // Copying it forward is what keeps those settings from silently resetting.
    // The original stays put, which leaves a way back and lets other ROM sets
    // inherit the same starting point.
    const std::filesystem::path legacy_path = kLegacyEepromFileName;
    if (!std::filesystem::is_regular_file(legacy_path, error))
    {
        return result;
    }

    std::error_code copy_error;
    std::filesystem::copy_file(legacy_path, result.path, copy_error);
    if (copy_error)
    {
        result.warnings.push_back("could not carry " + legacy_path.string() +
                                  " forward to " + result.path.string() +
                                  ": " + copy_error.message());
        return result;
    }

    result.carried_legacy_file = true;
    return result;
}

}  // namespace repiu::storage
