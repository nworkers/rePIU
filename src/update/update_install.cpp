#include "repiu/update/update_install.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace repiu::update
{
namespace
{

constexpr const char* kOldSuffix = ".repiu-old";
constexpr const char* kLeftoverList = ".repiu-old-files";

std::filesystem::path AsideName(const std::filesystem::path& target)
{
    std::filesystem::path aside = target;
    aside += kOldSuffix;
    return aside;
}

std::vector<std::filesystem::path> ReadLeftovers(
    const std::filesystem::path& list)
{
    std::vector<std::filesystem::path> leftovers;
    std::ifstream stream(list, std::ios::binary);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            leftovers.emplace_back(std::u8string(
                reinterpret_cast<const char8_t*>(line.data()), line.size()));
        }
    }
    return leftovers;
}

bool WriteLeftovers(const std::filesystem::path& list,
                    const std::vector<std::filesystem::path>& leftovers)
{
    std::error_code error;
    if (leftovers.empty())
    {
        std::filesystem::remove(list, error);
        return true;
    }
    std::ofstream stream(list, std::ios::binary | std::ios::trunc);
    for (const auto& path : leftovers)
    {
        const std::u8string text = path.generic_u8string();
        stream.write(reinterpret_cast<const char*>(text.data()),
                     static_cast<std::streamsize>(text.size()));
        stream.put('\n');
    }
    return static_cast<bool>(stream);
}

}  // namespace

bool CanInstallInto(const std::filesystem::path& install_folder,
                    const SemanticVersion& build_version, std::string* reason)
{
    std::ifstream stream(install_folder / "VERSION", std::ios::binary);
    if (!stream)
    {
        *reason = "not a release install (no VERSION next to the executable)";
        return false;
    }
    std::ostringstream text;
    text << stream.rdbuf();
    const auto version = ParseSemanticVersion(text.str());
    if (!version.has_value() || *version != build_version)
    {
        *reason = "VERSION next to the executable does not match this build";
        return false;
    }
    const std::filesystem::path probe = install_folder / ".repiu-write-test";
    {
        std::ofstream write(probe, std::ios::binary | std::ios::trunc);
        if (!write)
        {
            *reason = "the install folder cannot be written";
            return false;
        }
    }
    std::error_code error;
    std::filesystem::remove(probe, error);
    return true;
}

bool InstallStagedFiles(const std::filesystem::path& staging,
                        const std::vector<std::filesystem::path>& files,
                        const std::filesystem::path& install_folder,
                        std::string* error)
{
    struct Step
    {
        std::filesystem::path target;
        bool moved_aside = false;
        bool placed = false;
    };
    std::vector<Step> steps;
    steps.reserve(files.size());
    std::error_code fs_error;
    bool ok = true;
    for (const auto& relative : files)
    {
        Step step;
        step.target = install_folder / relative;
        const std::filesystem::path source = staging / relative;
        const std::filesystem::path aside = AsideName(step.target);
        std::filesystem::create_directories(step.target.parent_path(), fs_error);
        // A leftover from an earlier install that could not be deleted then.
        std::filesystem::remove(aside, fs_error);
        if (std::filesystem::exists(step.target, fs_error))
        {
            std::filesystem::rename(step.target, aside, fs_error);
            if (fs_error)
            {
                *error = "cannot move aside " + step.target.string() + ": " +
                    fs_error.message();
                ok = false;
                break;
            }
            step.moved_aside = true;
        }
        std::filesystem::rename(source, step.target, fs_error);
        if (fs_error)
        {
            *error = "cannot place " + step.target.string() + ": " +
                fs_error.message();
            steps.push_back(step);
            ok = false;
            break;
        }
        step.placed = true;
        steps.push_back(step);
    }

    if (!ok)
    {
        // Back out in reverse: the new file goes back to staging, the old one
        // back to its name.
        for (auto it = steps.rbegin(); it != steps.rend(); ++it)
        {
            const std::filesystem::path relative =
                it->target.lexically_relative(install_folder);
            if (it->placed)
            {
                std::filesystem::rename(it->target, staging / relative, fs_error);
            }
            if (it->moved_aside)
            {
                std::filesystem::rename(AsideName(it->target), it->target,
                                        fs_error);
            }
        }
        return false;
    }

    const std::filesystem::path list = install_folder / kLeftoverList;
    std::vector<std::filesystem::path> leftovers = ReadLeftovers(list);
    for (const auto& step : steps)
    {
        if (step.moved_aside)
        {
            leftovers.push_back(
                AsideName(step.target).lexically_relative(install_folder));
        }
    }
    WriteLeftovers(list, leftovers);
    std::filesystem::remove_all(staging, fs_error);
    return true;
}

std::size_t RemovePreviousInstallLeftovers(
    const std::filesystem::path& install_folder)
{
    const std::filesystem::path list = install_folder / kLeftoverList;
    std::vector<std::filesystem::path> remaining;
    std::size_t removed = 0;
    for (const auto& relative : ReadLeftovers(list))
    {
        // Only names this code wrote: relative, inside the folder, and ending
        // in the aside suffix.
        const std::string name = relative.filename().string();
        if (relative.is_absolute() ||
            relative.lexically_normal().string().rfind("..", 0) == 0 ||
            name.size() <= std::char_traits<char>::length(kOldSuffix) ||
            name.compare(name.size() - std::char_traits<char>::length(kOldSuffix),
                         std::string::npos, kOldSuffix) != 0)
        {
            continue;
        }
        std::error_code error;
        const std::filesystem::path target = install_folder / relative;
        if (!std::filesystem::exists(target, error))
        {
            continue;
        }
        if (std::filesystem::remove(target, error))
        {
            ++removed;
        }
        else
        {
            remaining.push_back(relative);
        }
    }
    WriteLeftovers(list, remaining);
    return removed;
}

}  // namespace repiu::update
