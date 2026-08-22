#include "nvram_path_probe.h"

#include "repiu/storage/nvram_path.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace repiu::tools
{
namespace
{

std::uint32_t g_check_count = 0;
std::uint32_t g_failure_count = 0;

void Check(bool condition, std::string_view label)
{
    ++g_check_count;
    if (condition)
    {
        return;
    }
    ++g_failure_count;
    std::cout << "[nvram-path] FAIL " << label << "\n";
}

// Task 501: the two platforms spell this differently, and the probe now builds
// on both. Windows removes a variable by assigning it an empty value; POSIX has
// a dedicated call for it. Either way the variable must actually disappear
// between cases, or one case leaks into the next.
void SetEnvironment(const char* name, const char* value)
{
#if defined(_WIN32)
    std::string assignment(name);
    assignment.push_back('=');
    if (value != nullptr)
    {
        assignment.append(value);
    }
    _putenv(assignment.c_str());
#else
    if (value == nullptr)
    {
        unsetenv(name);
    }
    else
    {
        setenv(name, value, 1);
    }
#endif
}

void WriteFile(const std::filesystem::path& path, std::string_view text)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
}

}  // namespace

bool RunNvramPathProbe()
{
    g_check_count = 0;
    g_failure_count = 0;

    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) / "repiu_nvram_path_probe";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);

    const std::filesystem::path nvram_root = root / "nvram";
    const std::filesystem::path executable_directory = root / "bin";

    // 1. The full-path override wins over everything. Benchmark scripts and
    // the measurement guides depend on this to isolate the EEPROM per run.
    const std::filesystem::path override_path = root / "fixture" / "iso.dat";
    SetEnvironment("REPIU_EEPROM_PATH", override_path.string().c_str());
    SetEnvironment("REPIU_NVRAM_DIR", nvram_root.string().c_str());
    {
        const storage::EepromPathResult result =
            storage::ResolveEepromPath("pumpit1", executable_directory);
        Check(result.from_override && result.path == override_path,
              "REPIU_EEPROM_PATH is used verbatim");
        Check(!result.carried_legacy_file,
              "the override does not carry a legacy file forward");
    }
    SetEnvironment("REPIU_EEPROM_PATH", nullptr);

    // 2. The ROM-set path, and the directory really existing afterwards. The
    // EEPROM device writes from its constructor, so a missing directory would
    // reset the image on every run without reporting anything.
    {
        const storage::EepromPathResult result =
            storage::ResolveEepromPath("pumpit1", executable_directory);
        Check(result.path == nvram_root / "pumpit1" / "eeprom.dat",
              "the ROM-set path is nvram/<rom set>/eeprom.dat");
        Check(!result.from_override, "no override was reported");
        Check(std::filesystem::is_directory(nvram_root / "pumpit1"),
              "the ROM-set directory is created");
        Check(result.warnings.empty(), "no warnings on the ordinary path");
    }

    // 6. Different ROM sets do not share an image.
    {
        const storage::EepromPathResult other =
            storage::ResolveEepromPath("pumpipx3", executable_directory);
        Check(other.path == nvram_root / "pumpipx3" / "eeprom.dat",
              "another ROM set resolves to its own path");
        Check(other.path != nvram_root / "pumpit1" / "eeprom.dat",
              "ROM sets do not share one image");
    }

    // 5. A profile with no ROM set is not a cabinet and keeps the historical
    // working-directory file.
    {
        const storage::EepromPathResult none =
            storage::ResolveEepromPath("", executable_directory);
        Check(none.path == std::filesystem::path("eeprom.dat"),
              "an empty ROM set id keeps the working-directory eeprom.dat");
    }

    // 3/4. Carrying a shared eeprom.dat forward. The working directory is what
    // the legacy path is relative to, so the probe moves into a scratch
    // directory rather than writing beside the repository's own file.
    const std::filesystem::path previous_directory =
        std::filesystem::current_path(error);
    const std::filesystem::path legacy_root = root / "legacy";
    std::filesystem::create_directories(legacy_root, error);
    std::filesystem::current_path(legacy_root, error);

    const std::string legacy_contents = "legacy-eeprom-image";
    WriteFile(legacy_root / "eeprom.dat", legacy_contents);
    const std::filesystem::path legacy_nvram = legacy_root / "nvram";
    SetEnvironment("REPIU_NVRAM_DIR", legacy_nvram.string().c_str());

    {
        const storage::EepromPathResult carried =
            storage::ResolveEepromPath("pumpit3", executable_directory);
        Check(carried.carried_legacy_file, "a shared eeprom.dat is carried "
                                           "forward");
        Check(ReadFile(carried.path) == legacy_contents,
              "the carried image keeps its contents");
        Check(std::filesystem::is_regular_file(legacy_root / "eeprom.dat"),
              "the original is left in place");
    }

    // Once the ROM-set file exists it is authoritative; a later run must not
    // copy over the settings the user has since changed.
    WriteFile(legacy_nvram / "pumpit3" / "eeprom.dat", "rom-set-image");
    {
        const storage::EepromPathResult again =
            storage::ResolveEepromPath("pumpit3", executable_directory);
        Check(!again.carried_legacy_file,
              "an existing ROM-set image is not overwritten");
        Check(ReadFile(again.path) == "rom-set-image",
              "the ROM-set image survives a later run");
    }

    std::filesystem::current_path(previous_directory, error);
    SetEnvironment("REPIU_NVRAM_DIR", nullptr);
    std::filesystem::remove_all(root, error);

    std::cout << "[nvram-path] checks=" << g_check_count
              << " failures=" << g_failure_count << "\n";
    return g_failure_count == 0;
}

}  // namespace repiu::tools
