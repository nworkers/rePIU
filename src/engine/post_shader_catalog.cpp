#include "repiu/engine/post_shader_catalog.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace repiu::engine
{
namespace
{

// Generated at configure time from src/engine/post_shaders/*.glsl; defines
// kPostShaderBuiltinCrt and kPostShaderBuiltinScanline.
#include "repiu/engine/post_shader_builtins.inc"

struct BuiltinShader
{
    const char* id;
    std::string_view text;
};

constexpr BuiltinShader kBuiltinShaders[] = {
    {"crt", kPostShaderBuiltinCrt},
    {"scanline", kPostShaderBuiltinScanline},
};

constexpr const char* kShaderDirectoryName = "shaders";
constexpr const char* kShaderExtension = ".glsl";

}  // namespace

std::filesystem::path ResolvePostShaderDirectory(
    const std::filesystem::path& executable_directory)
{
    std::error_code error;
    const std::filesystem::path working_candidate = kShaderDirectoryName;
    if (std::filesystem::is_directory(working_candidate, error))
    {
        return working_candidate;
    }
    if (!executable_directory.empty())
    {
        const std::filesystem::path executable_candidate =
            executable_directory / kShaderDirectoryName;
        if (std::filesystem::is_directory(executable_candidate, error))
        {
            return executable_candidate;
        }
    }
    return working_candidate;
}

std::vector<PostShaderEntry> ListPostShaders(
    const std::filesystem::path& directory)
{
    std::vector<PostShaderEntry> entries;
    for (const BuiltinShader& builtin : kBuiltinShaders)
    {
        PostShaderEntry entry;
        entry.id = builtin.id;
        entry.builtin = true;
        entries.push_back(std::move(entry));
    }

    std::vector<PostShaderEntry> files;
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end)
    {
        const std::filesystem::directory_entry& item = *iterator;
        std::error_code status_error;
        if (item.is_regular_file(status_error) &&
            item.path().extension() == kShaderExtension)
        {
            PostShaderEntry entry;
            entry.id = item.path().filename().string();
            entry.path = item.path();
            files.push_back(std::move(entry));
        }
        iterator.increment(error);
    }
    std::sort(files.begin(), files.end(),
              [](const PostShaderEntry& left, const PostShaderEntry& right)
              { return left.id < right.id; });
    for (PostShaderEntry& entry : files)
    {
        entries.push_back(std::move(entry));
    }
    return entries;
}

const PostShaderEntry* FindPostShader(
    const std::vector<PostShaderEntry>& entries, std::string_view id)
{
    for (const PostShaderEntry& entry : entries)
    {
        if (entry.id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

bool LoadPostShaderText(const PostShaderEntry& entry, std::string* text,
                        std::string* error)
{
    if (text == nullptr)
    {
        return false;
    }
    if (entry.builtin)
    {
        for (const BuiltinShader& builtin : kBuiltinShaders)
        {
            if (entry.id == builtin.id)
            {
                text->assign(builtin.text);
                return true;
            }
        }
        if (error != nullptr)
        {
            *error = "no built-in shader named " + entry.id;
        }
        return false;
    }
    std::ifstream stream(entry.path, std::ios::binary);
    if (!stream)
    {
        if (error != nullptr)
        {
            *error = "cannot open " + entry.path.string();
        }
        return false;
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    *text = contents.str();
    return true;
}

}  // namespace repiu::engine
