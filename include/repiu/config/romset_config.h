#ifndef REPIU_CONFIG_ROMSET_CONFIG_H_
#define REPIU_CONFIG_ROMSET_CONFIG_H_

#include "repiu/input/jamma_input_bindings.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::config
{

// How far the parent chain is followed. Bounds both a cycle in the profile
// data and a chain longer than the catalog could reasonably have; the deepest
// real one today is three (pumpit3a, pumpit3, pumpitup).
constexpr std::uint32_t kMaxRomSetLayerCount = 4;

// Returns the parent ROM-set id of `rom_set_id`, or an empty view at the root.
using ParentRomSetLookup =
    std::function<std::string_view(std::string_view rom_set_id)>;

// Builds the layer ids root first, e.g. {"pumpitup", "pumpit3", "pumpit3a"}.
// The launched ROM set is always last, which is what makes it win.
//
// The lookup is injected rather than read directly so this module does not
// depend on the target profile catalog; the caller supplies the data source.
std::vector<std::string> BuildRomSetLayerIds(std::string_view rom_set_id,
                                             const ParentRomSetLookup& lookup);

struct RomSetConfigRequest
{
    // Root first; the last entry is the launched ROM set and the only one a
    // default file is ever generated for.
    std::vector<std::string> layer_ids;

    // Empty means "discover": REPIU_CFG_DIR, then cfg under the working
    // directory, then cfg next to the host executable.
    std::filesystem::path cfg_directory_override;
    std::filesystem::path executable_directory;

    // Cleared by REPIU_CFG_WRITE_DEFAULT=0 for automated runs and probes.
    bool write_default_when_missing = true;
};

struct RomSetConfigResult
{
    input::ResolvedJammaBindings bindings;

    std::filesystem::path cfg_directory;
    // Files that existed and were applied, in layer order.
    std::vector<std::filesystem::path> applied_files;

    bool generated_default_file = false;
    std::filesystem::path generated_file;

    // Config-file problems, already formatted for logging. Never fatal: a typo
    // in a config file must not stop the game from running.
    std::vector<std::string> warnings;
};

// Fills a request from the process environment, leaving `layer_ids` for the
// caller to supply.
RomSetConfigRequest MakeRomSetConfigRequestFromEnvironment(
    const std::filesystem::path& executable_directory);

// Resolves bindings for a ROM set: built-in defaults, then each layer's file
// in order, then finalization. When the launched ROM set has no file yet and
// generation is enabled, writes one as a commented-out template.
//
// Never fails. With no files present the result is the built-in defaults,
// which is byte for byte the mapping the emulator had before configuration
// existed.
RomSetConfigResult LoadRomSetConfig(const RomSetConfigRequest& request);

}  // namespace repiu::config

#endif  // REPIU_CONFIG_ROMSET_CONFIG_H_
