#ifndef REPIU_STORAGE_NVRAM_PATH_H_
#define REPIU_STORAGE_NVRAM_PATH_H_

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::storage
{

// Host path policy for persistent state the guest itself writes.
//
// Kept apart from repiu::config: a config file is written by the user and read
// by the program, while NVRAM is written and read by the guest. Later guest
// state such as saves belongs here too.

struct EepromPathResult
{
    std::filesystem::path path;

    // True when REPIU_EEPROM_PATH supplied the path verbatim. Benchmark
    // scripts and the measurement guides rely on that override to isolate the
    // EEPROM per run, so it wins over every rule below it.
    bool from_override = false;

    // True when a shared eeprom.dat from the working directory was copied into
    // this ROM set's directory. Happens at most once per ROM set.
    bool carried_legacy_file = false;

    // Formatted for logging. Never fatal: a missing EEPROM initializes to
    // 0xFFFF, so failing to place the file loses settings but not the run.
    std::vector<std::string> warnings;
};

// Resolves where this ROM set's 93C46 image lives and makes sure the directory
// exists.
//
// Creating the directory is this function's job, not the caller's. Eeprom93c46
// writes a fresh image from its constructor when the file is missing, and that
// write fails silently if the parent directory is not there -- which would
// reset the EEPROM on every run and report nothing.
//
// An empty `rom_set_id` keeps the historical working-directory `eeprom.dat`,
// because a profile without a ROM set is not a cabinet.
EepromPathResult ResolveEepromPath(
    std::string_view rom_set_id,
    const std::filesystem::path& executable_directory);

}  // namespace repiu::storage

#endif  // REPIU_STORAGE_NVRAM_PATH_H_
