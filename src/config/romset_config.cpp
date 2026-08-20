#include "repiu/config/romset_config.h"

#include "repiu/config/ini_document.h"
#include "repiu/config/romset_config_template.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace repiu::config
{
namespace
{

std::filesystem::path ConfigFilePath(const std::filesystem::path& directory,
                                     std::string_view rom_set_id)
{
    return directory / (std::string(rom_set_id) + ".ini");
}

bool ReadTextFile(const std::filesystem::path& path, std::string* text)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    *text = buffer.str();
    return true;
}

bool IsEnvironmentFlagDisabled(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
    {
        return false;
    }
    return std::strcmp(value, "0") == 0 || std::strcmp(value, "off") == 0 ||
           std::strcmp(value, "false") == 0;
}

// The first candidate that already exists wins. When none does, the working
// directory candidate is returned anyway so first-run generation has a place
// to create; that matches how `roms` and `build/runtime_mounts` are resolved.
std::filesystem::path DiscoverCfgDirectory(
    const RomSetConfigRequest& request)
{
    if (!request.cfg_directory_override.empty())
    {
        return request.cfg_directory_override;
    }

    std::error_code error;
    const std::filesystem::path working_directory_candidate = "cfg";
    if (std::filesystem::is_directory(working_directory_candidate, error))
    {
        return working_directory_candidate;
    }

    if (!request.executable_directory.empty())
    {
        const std::filesystem::path executable_candidate =
            request.executable_directory / "cfg";
        if (std::filesystem::is_directory(executable_candidate, error))
        {
            return executable_candidate;
        }
    }

    return working_directory_candidate;
}

// Writes through a temporary and renames, so an interrupted write cannot leave
// a partial file that the next run would then treat as the user's own and
// refuse to replace.
bool WriteFileAtomically(const std::filesystem::path& path,
                         const std::string& text, std::string* error)
{
    std::filesystem::path temporary_path = path;
    temporary_path += ".tmp";

    {
        std::ofstream stream(temporary_path, std::ios::binary |
                                                 std::ios::trunc);
        if (!stream)
        {
            *error = "could not create " + temporary_path.string();
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            *error = "could not write " + temporary_path.string();
            return false;
        }
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary_path, path, rename_error);
    if (rename_error)
    {
        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);
        *error = "could not move " + temporary_path.string() + " to " +
                 path.string() + ": " + rename_error.message();
        return false;
    }
    return true;
}

void GenerateDefaultFile(const RomSetConfigRequest& request,
                         const std::filesystem::path& path,
                         std::string_view rom_set_id,
                         RomSetConfigResult* result)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error && !std::filesystem::is_directory(path.parent_path()))
    {
        result->warnings.push_back("could not create config directory " +
                                   path.parent_path().string() + ": " +
                                   error.message());
        return;
    }

    const std::string text =
        RenderRomSetConfigTemplate(rom_set_id, result->bindings);
    std::string write_error;
    if (!WriteFileAtomically(path, text, &write_error))
    {
        // A read-only location is a perfectly normal way to run. Losing the
        // convenience of a generated file is not a reason to stop.
        result->warnings.push_back(write_error);
        return;
    }

    result->generated_default_file = true;
    result->generated_file = path;
    (void)request;
}

}  // namespace

std::vector<std::string> BuildRomSetLayerIds(std::string_view rom_set_id,
                                             const ParentRomSetLookup& lookup)
{
    std::vector<std::string> chain;
    if (rom_set_id.empty())
    {
        return chain;
    }

    // Collected leaf first, then reversed, because the lookup only walks
    // upwards. The depth cap doubles as the cycle guard.
    std::string current(rom_set_id);
    while (chain.size() < kMaxRomSetLayerCount)
    {
        chain.push_back(current);
        if (!lookup)
        {
            break;
        }
        const std::string_view parent = lookup(current);
        if (parent.empty())
        {
            break;
        }
        std::string next(parent);
        bool already_seen = false;
        for (const std::string& visited : chain)
        {
            if (visited == next)
            {
                already_seen = true;
                break;
            }
        }
        if (already_seen)
        {
            break;
        }
        current = std::move(next);
    }

    std::vector<std::string> ordered;
    ordered.reserve(chain.size());
    for (std::size_t index = chain.size(); index > 0; --index)
    {
        ordered.push_back(chain[index - 1]);
    }
    return ordered;
}

RomSetConfigRequest MakeRomSetConfigRequestFromEnvironment(
    const std::filesystem::path& executable_directory)
{
    RomSetConfigRequest request;
    request.executable_directory = executable_directory;

    const char* directory = std::getenv("REPIU_CFG_DIR");
    if (directory != nullptr && directory[0] != '\0')
    {
        request.cfg_directory_override = directory;
    }
    request.write_default_when_missing =
        !IsEnvironmentFlagDisabled("REPIU_CFG_WRITE_DEFAULT");
    return request;
}

RomSetConfigResult LoadRomSetConfig(const RomSetConfigRequest& request)
{
    RomSetConfigResult result;
    result.bindings = input::DefaultJammaBindings();
    if (request.layer_ids.empty())
    {
        return result;
    }

    result.cfg_directory = DiscoverCfgDirectory(request);

    bool leaf_file_exists = false;
    for (std::size_t index = 0; index < request.layer_ids.size(); ++index)
    {
        const std::string& layer_id = request.layer_ids[index];
        const bool is_leaf = index + 1 == request.layer_ids.size();
        const std::filesystem::path path =
            ConfigFilePath(result.cfg_directory, layer_id);

        std::string text;
        if (!ReadTextFile(path, &text))
        {
            // A missing layer is ordinary. Most ROM sets name `pumpitup` as
            // their parent and no such file has to exist.
            continue;
        }

        leaf_file_exists = leaf_file_exists || is_leaf;
        result.applied_files.push_back(path);

        const IniDocument document =
            IniDocument::Parse(text, path.string());
        for (const std::string& warning : document.warnings())
        {
            result.warnings.push_back(warning);
        }
        input::ApplyJammaInputSection(document, &result.bindings,
                                      &result.warnings);
    }

    input::FinalizeJammaBindings(&result.bindings);

    if (!leaf_file_exists && request.write_default_when_missing)
    {
        const std::string& leaf_id = request.layer_ids.back();
        GenerateDefaultFile(request,
                            ConfigFilePath(result.cfg_directory, leaf_id),
                            leaf_id, &result);
    }

    return result;
}

}  // namespace repiu::config
