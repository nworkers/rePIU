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
constexpr const char* kPostShaderKey = "post_shader";
constexpr const char* kTextureFullPrecisionKey = "texture_full_precision";
constexpr const char* kFullscreenKey = "fullscreen";
constexpr const char* kKeepAspectKey = "keep_aspect";
constexpr const char* kLastRomSetKey = "last_rom_set";
constexpr const char* kCheckUpdatesKey = "check_updates";

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

// Reads a `[Video]` key that holds 0 or 1, warning about anything else.
void LoadVideoSwitch(const config::IniDocument& document,
                     const std::string& origin, const char* key,
                     bool* has_value, bool* value,
                     std::vector<std::string>* warnings)
{
    const std::string* text = document.FindLast(kVideoSection, key);
    if (text == nullptr)
    {
        return;
    }
    if (*text == "0" || *text == "1")
    {
        *has_value = true;
        *value = *text == "1";
        return;
    }
    warnings->push_back(origin + ": [Video] " + key + " is not 0 or 1: " +
                        *text);
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
            document.FindLast(kVideoSection, kPostShaderKey))
    {
        // An empty value is the same as an absent key: nothing to publish.
        if (!value->empty())
        {
            load.settings.has_post_shader = true;
            load.settings.post_shader = *value;
        }
    }
    LoadVideoSwitch(document, origin, kTextureFullPrecisionKey,
                    &load.settings.has_texture_full_precision,
                    &load.settings.texture_full_precision, &load.warnings);
    LoadVideoSwitch(document, origin, kFullscreenKey,
                    &load.settings.has_fullscreen, &load.settings.fullscreen,
                    &load.warnings);
    LoadVideoSwitch(document, origin, kKeepAspectKey,
                    &load.settings.has_keep_aspect,
                    &load.settings.keep_aspect, &load.warnings);
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
    if (const std::string* value =
            document.FindLast(kLauncherSection, kCheckUpdatesKey))
    {
        if (*value == "0" || *value == "1")
        {
            load.settings.has_check_updates = true;
            load.settings.check_updates = *value == "1";
        }
        else
        {
            load.warnings.push_back(origin + ": [Launcher] " +
                                    kCheckUpdatesKey + " is not 0 or 1: " +
                                    *value);
        }
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
    if (settings.has_post_shader && !settings.post_shader.empty())
    {
        stream << kPostShaderKey << " = " << settings.post_shader << "\n";
    }
    if (settings.has_texture_full_precision)
    {
        stream << kTextureFullPrecisionKey << " = "
               << (settings.texture_full_precision ? 1 : 0) << "\n";
    }
    if (settings.has_fullscreen)
    {
        stream << kFullscreenKey << " = " << (settings.fullscreen ? 1 : 0)
               << "\n";
    }
    if (settings.has_keep_aspect)
    {
        stream << kKeepAspectKey << " = " << (settings.keep_aspect ? 1 : 0)
               << "\n";
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
    if (settings.has_check_updates)
    {
        stream << kCheckUpdatesKey << " = " << (settings.check_updates ? 1 : 0)
               << "\n";
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
    const char* swap_interval_value, const char* ymz_volume_value,
    const char* post_shader_value,
    const char* texture_full_precision_value, const char* fullscreen_value,
    const char* keep_aspect_value)
{
    LauncherEnvironmentOverrides overrides;
    // An empty value still counts as set: the caller chose to define it, and
    // the consumers decide what an empty value means.
    overrides.swap_interval = swap_interval_value != nullptr;
    overrides.ymz_volume = ymz_volume_value != nullptr;
    overrides.post_shader = post_shader_value != nullptr;
    overrides.texture_full_precision = texture_full_precision_value != nullptr;
    overrides.fullscreen = fullscreen_value != nullptr;
    overrides.keep_aspect = keep_aspect_value != nullptr;
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
    if (settings.has_post_shader && !settings.post_shader.empty() &&
        !overrides.post_shader)
    {
        publish(kLauncherPostShaderVariable, settings.post_shader);
        application.post_shader_published = true;
    }
    if (settings.has_texture_full_precision &&
        !overrides.texture_full_precision)
    {
        publish(kLauncherTextureFullPrecisionVariable,
                settings.texture_full_precision ? "1" : "0");
        application.texture_full_precision_published = true;
    }
    if (settings.has_fullscreen && !overrides.fullscreen)
    {
        publish(kLauncherFullscreenVariable, settings.fullscreen ? "1" : "0");
        application.fullscreen_published = true;
    }
    if (settings.has_keep_aspect && !overrides.keep_aspect)
    {
        publish(kLauncherKeepAspectVariable,
                settings.keep_aspect ? "1" : "0");
        application.keep_aspect_published = true;
    }
    return application;
}

}  // namespace repiu::launcher
