#include "repiu/launcher/launcher_settings.h"

#include "repiu/config/ini_document.h"

#include <charconv>
#include <fstream>
#include <string>
#include <sstream>
#include <system_error>

namespace repiu::launcher
{
namespace
{

constexpr const char* kVideoSection = "Video";
constexpr const char* kAudioSection = "Audio";
constexpr const char* kLauncherSection = "Launcher";
constexpr const char* kSwapIntervalKey = "swap_interval";
constexpr const char* kYmzVolumeKey = "ymz_volume";
constexpr const char* kLastRomSetKey = "last_rom_set";

bool ParseInt32(const std::string& text, std::int32_t* value)
{
    const char* first = text.data();
    const char* last = text.data() + text.size();
    if (first != last && *first == '+')
    {
        ++first;
    }
    const auto result = std::from_chars(first, last, *value);
    return result.ec == std::errc{} && result.ptr == last;
}

bool ParseFloat(const std::string& text, float* value)
{
    // from_chars for floating point is not available everywhere this builds,
    // so the stream is used with the classic locale to keep '.' as the
    // separator regardless of the host locale.
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    float parsed = 0.0F;
    stream >> parsed;
    if (!stream || !stream.eof())
    {
        return false;
    }
    *value = parsed;
    return true;
}

}  // namespace

std::filesystem::path LauncherSettingsPath(
    const std::filesystem::path& config_directory)
{
    return config_directory / "repiu.ini";
}

LauncherSettingsLoad LoadLauncherSettings(
    const std::filesystem::path& config_directory)
{
    LauncherSettingsLoad load;
    const std::filesystem::path path = LauncherSettingsPath(config_directory);
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return load;
    }
    load.file_present = true;
    std::ostringstream text;
    text << stream.rdbuf();
    const std::string origin = path.string();
    const config::IniDocument document =
        config::IniDocument::Parse(text.str(), origin);
    for (const std::string& warning : document.warnings())
    {
        load.warnings.push_back(warning);
    }

    if (const std::string* value =
            document.FindLast(kVideoSection, kSwapIntervalKey))
    {
        std::int32_t parsed = 0;
        if (ParseInt32(*value, &parsed))
        {
            load.settings.has_swap_interval = true;
            load.settings.swap_interval = parsed;
        }
        else
        {
            load.warnings.push_back(origin + ": [Video] " +
                                    kSwapIntervalKey +
                                    " is not an integer: " + *value);
        }
    }
    if (const std::string* value =
            document.FindLast(kAudioSection, kYmzVolumeKey))
    {
        float parsed = 0.0F;
        if (ParseFloat(*value, &parsed))
        {
            load.settings.has_ymz_volume = true;
            load.settings.ymz_volume = parsed;
        }
        else
        {
            load.warnings.push_back(origin + ": [Audio] " + kYmzVolumeKey +
                                    " is not a number: " + *value);
        }
    }
    if (const std::string* value =
            document.FindLast(kLauncherSection, kLastRomSetKey))
    {
        load.settings.last_rom_set = *value;
    }
    return load;
}

bool SaveLauncherSettings(const std::filesystem::path& config_directory,
                          const LauncherSettings& settings)
{
    std::error_code directory_error;
    std::filesystem::create_directories(config_directory, directory_error);
    const std::filesystem::path path = LauncherSettingsPath(config_directory);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }
    stream << "; rePIU launcher settings.\n"
           << "; An environment variable of the same meaning always wins over\n"
           << "; a value stored here, so measurement scripts keep control.\n"
           << "\n[" << kVideoSection << "]\n";
    if (settings.has_swap_interval)
    {
        stream << kSwapIntervalKey << " = " << settings.swap_interval << "\n";
    }
    stream << "\n[" << kAudioSection << "]\n";
    if (settings.has_ymz_volume)
    {
        std::ostringstream volume;
        volume.imbue(std::locale::classic());
        volume << settings.ymz_volume;
        stream << kYmzVolumeKey << " = " << volume.str() << "\n";
    }
    stream << "\n[" << kLauncherSection << "]\n";
    if (!settings.last_rom_set.empty())
    {
        stream << kLastRomSetKey << " = " << settings.last_rom_set << "\n";
    }
    return static_cast<bool>(stream);
}

std::string BuildLauncherChildCommandLine(const std::string& executable_path,
                                          const std::string& rom_set_id)
{
    std::string command_line;
    command_line.push_back('"');
    command_line.append(executable_path);
    command_line.push_back('"');
    if (!rom_set_id.empty())
    {
        command_line.push_back(' ');
        command_line.append(rom_set_id);
    }
    return command_line;
}

LauncherEnvironmentOverrides ResolveLauncherEnvironmentOverrides(
    const char* swap_interval_value, const char* ymz_volume_value)
{
    LauncherEnvironmentOverrides overrides;
    // An empty value still counts as set: the caller chose to define it, and
    // the consumers decide what an empty value means.
    overrides.swap_interval = swap_interval_value != nullptr;
    overrides.ymz_volume = ymz_volume_value != nullptr;
    return overrides;
}

LauncherSettingsApplication ApplyLauncherSettings(
    const LauncherSettings& settings,
    const LauncherEnvironmentOverrides& overrides,
    const std::function<void(const char*, const std::string&)>& publish)
{
    LauncherSettingsApplication application;
    if (!publish)
    {
        return application;
    }
    if (settings.has_swap_interval && !overrides.swap_interval)
    {
        publish(kLauncherSwapIntervalVariable,
                std::to_string(settings.swap_interval));
        application.swap_interval_published = true;
    }
    if (settings.has_ymz_volume && !overrides.ymz_volume)
    {
        std::ostringstream volume;
        volume.imbue(std::locale::classic());
        volume << settings.ymz_volume;
        publish(kLauncherYmzVolumeVariable, volume.str());
        application.ymz_volume_published = true;
    }
    return application;
}

}  // namespace repiu::launcher
