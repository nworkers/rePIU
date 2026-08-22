#ifndef REPIU_LAUNCHER_LAUNCHER_SETTINGS_H_
#define REPIU_LAUNCHER_LAUNCHER_SETTINGS_H_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace repiu::launcher
{

// Operator-facing settings the launcher stores in cfg/repiu.ini.
//
// Each option that affects the run is delivered to the existing consumers by
// publishing it as the environment variable they already read, rather than by
// threading a new parameter through every subsystem. That keeps the change
// small and makes the precedence rule exact: an environment variable the caller
// already set is never overwritten, so benchmark scripts and diagnostic
// procedures keep winning over anything chosen in the GUI.
struct LauncherSettings
{
    // Present flags distinguish "not stored" from "stored as the default",
    // because an absent key must leave the environment untouched.
    bool has_swap_interval = false;
    std::int32_t swap_interval = 0;
    bool has_ymz_volume = false;
    float ymz_volume = 1.0F;
    // Where the selection cursor starts next time. Not applied to the run.
    std::string last_rom_set;
};

struct LauncherSettingsLoad
{
    LauncherSettings settings;
    bool file_present = false;
    // Parse problems are reported, never fatal: a typo in this file must not
    // stop the launcher from opening.
    std::vector<std::string> warnings;
};

[[nodiscard]] std::filesystem::path LauncherSettingsPath(
    const std::filesystem::path& config_directory);

[[nodiscard]] LauncherSettingsLoad LoadLauncherSettings(
    const std::filesystem::path& config_directory);

// Writes every stored key, creating the directory when needed. Returns false
// only when the file could not be written.
bool SaveLauncherSettings(const std::filesystem::path& config_directory,
                          const LauncherSettings& settings);

// Which options the environment already defines, and therefore which stored
// values must not be published. Takes the raw values so a probe can exercise
// the rule without touching the process environment.
struct LauncherEnvironmentOverrides
{
    bool swap_interval = false;
    bool ymz_volume = false;
};

[[nodiscard]] LauncherEnvironmentOverrides ResolveLauncherEnvironmentOverrides(
    const char* swap_interval_value, const char* ymz_volume_value);

struct LauncherSettingsApplication
{
    bool swap_interval_published = false;
    bool ymz_volume_published = false;
};

// Publishes the stored values the environment has not already claimed.
// `publish` receives the variable name and its value.
LauncherSettingsApplication ApplyLauncherSettings(
    const LauncherSettings& settings,
    const LauncherEnvironmentOverrides& overrides,
    const std::function<void(const char*, const std::string&)>& publish);

// The command line that carries a launcher selection into a fresh process.
//
// The launcher must load a GPU driver to draw, and that driver claims low
// address space the guest needs, so the selected ROM set runs in a new process
// whose address space is intact. Quoting is done here, and probed here, because
// an executable path containing a space is the classic way this breaks in the
// field and never in a developer's own tree.
[[nodiscard]] std::string BuildLauncherChildCommandLine(
    const std::string& executable_path, const std::string& rom_set_id);

// The environment variable names the launcher publishes into, exposed so the
// probe asserts the launcher and the consumers agree on them.
inline constexpr const char* kLauncherSwapIntervalVariable =
    "REPIU_GLIDE_SWAP_INTERVAL";
inline constexpr const char* kLauncherYmzVolumeVariable = "REPIU_YMZ_VOLUME";

}  // namespace repiu::launcher

#endif  // REPIU_LAUNCHER_LAUNCHER_SETTINGS_H_
