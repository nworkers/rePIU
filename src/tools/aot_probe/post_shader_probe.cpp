#include "post_shader_probe.h"

#include "repiu/engine/post_shader_catalog.h"
#include "repiu/engine/post_shader_source.h"
#include "repiu/launcher/launcher_settings.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace repiu::tools
{
namespace
{

using engine::BuildPostShaderProgramSource;
using engine::FindPostShader;
using engine::ListPostShaders;
using engine::LoadPostShaderText;
using engine::PostShaderEntry;
using engine::PostShaderProgramSource;

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.0001F;
}

bool StartsWith(const std::string& text, const std::string& prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

// The stage define lands first, or right after a `#version` line, and the
// rest of the text follows untouched.
bool ProbeAssembly()
{
    PostShaderProgramSource plain;
    std::string error;
    const bool plain_built = BuildPostShaderProgramSource(
        "#if defined(VERTEX)\nvoid main(){}\n#elif defined(FRAGMENT)\n"
        "void main(){}\n#endif\n",
        &plain, &error);
    const bool plain_ok = plain_built &&
        StartsWith(plain.vertex,
                   "#define VERTEX\n#define PARAMETER_UNIFORM\n#if defined") &&
        StartsWith(plain.fragment,
                   "#define FRAGMENT\n#define PARAMETER_UNIFORM\n#if ") &&
        plain.parameters.empty();

    PostShaderProgramSource versioned;
    const bool versioned_built = BuildPostShaderProgramSource(
        "#version 120\r\n#if defined(VERTEX)\n#elif defined(FRAGMENT)\n#endif",
        &versioned, &error);
    const bool versioned_ok = versioned_built &&
        StartsWith(versioned.vertex,
                   "#version 120\r\n#define VERTEX\n#define PARAMETER_UNIFORM\n"
                   "#if defined(VERTEX)") &&
        StartsWith(versioned.fragment, "#version 120\r\n#define FRAGMENT\n");

    // A file of a `#version` line alone still gets a line break before the
    // defines.
    PostShaderProgramSource version_only;
    const bool version_only_ok = BuildPostShaderProgramSource(
        "  #version 110 // VERTEX FRAGMENT", &version_only, &error) &&
        StartsWith(version_only.vertex,
                   "  #version 110 // VERTEX FRAGMENT\n#define VERTEX\n");

    return plain_ok && versioned_ok && version_only_ok;
}

bool ProbeParameters()
{
    PostShaderProgramSource source;
    std::string error;
    const bool built = BuildPostShaderProgramSource(
        "#pragma parameter STRENGTH \"Line strength\" 0.5 0.0 1.0 0.05\n"
        "  #pragma  parameter GLOW \"Glow, \\ odd chars\" 1 0 2\r\n"
        "#pragma parameter STRENGTH \"Again\" 0.9 0.0 1.0 0.1\n"
        "#pragma optimize(on)\n"
        "#if defined(VERTEX)\n#elif defined(FRAGMENT)\n#endif\n",
        &source, &error);
    const bool parsed = built && source.parameters.size() == 2U &&
        source.parameters[0].name == "STRENGTH" &&
        source.parameters[0].description == "Line strength" &&
        Near(source.parameters[0].initial, 0.5F) &&
        Near(source.parameters[0].value, 0.5F) &&
        Near(source.parameters[0].minimum, 0.0F) &&
        Near(source.parameters[0].maximum, 1.0F) &&
        Near(source.parameters[0].step, 0.05F) &&
        source.parameters[1].name == "GLOW" &&
        source.parameters[1].description == "Glow, \\ odd chars" &&
        Near(source.parameters[1].maximum, 2.0F) &&
        Near(source.parameters[1].step, 0.0F);

    // Every malformed shape fails with a reason rather than guessing.
    const char* const malformed[] = {
        "#pragma parameter 9BAD \"x\" 0 0 1\nVERTEX FRAGMENT",
        "#pragma parameter NAME unquoted 0 0 1\nVERTEX FRAGMENT",
        "#pragma parameter NAME \"open 0 0 1\nVERTEX FRAGMENT",
        "#pragma parameter NAME \"x\" 0 0\nVERTEX FRAGMENT",
        "#pragma parameter NAME \"x\" 0 2 1\nVERTEX FRAGMENT",
    };
    bool rejected = true;
    for (const char* text : malformed)
    {
        PostShaderProgramSource ignored;
        std::string reason;
        rejected = rejected &&
            !BuildPostShaderProgramSource(text, &ignored, &reason) &&
            !reason.empty();
    }
    PostShaderProgramSource ignored;
    const bool empty_rejected =
        !BuildPostShaderProgramSource("", &ignored, &error) &&
        !BuildPostShaderProgramSource("void main(){}", &ignored, &error) &&
        !BuildPostShaderProgramSource("VERTEX FRAGMENT", nullptr, &error);
    return parsed && rejected && empty_rejected;
}

bool ProbeBuiltins()
{
    const std::vector<PostShaderEntry> entries =
        ListPostShaders(std::filesystem::path("repiu_no_such_shader_dir"));
    bool ok = entries.size() == 2U && entries[0].id == "crt" &&
        entries[0].builtin && entries[1].id == "scanline" &&
        entries[1].builtin;
    for (const PostShaderEntry& entry : entries)
    {
        std::string text;
        std::string error;
        PostShaderProgramSource source;
        ok = ok && LoadPostShaderText(entry, &text, &error) &&
            text.find("BSD 3-Clause") != std::string::npos &&
            BuildPostShaderProgramSource(text, &source, &error) &&
            !source.parameters.empty();
    }
    return ok;
}

bool ProbeCatalog()
{
    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) / "repiu_post_shader_probe";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "nested.glsl", error);
    const auto write = [&root](const char* name, const char* text) {
        std::ofstream stream(root / name, std::ios::binary | std::ios::trunc);
        stream << text;
    };
    write("zeta.glsl", "zeta");
    write("alpha.glsl", "alpha");
    write("readme.txt", "not a shader");

    const std::vector<PostShaderEntry> entries = ListPostShaders(root);
    const bool listed = entries.size() == 4U && entries[0].id == "crt" &&
        entries[1].id == "scanline" && entries[2].id == "alpha.glsl" &&
        !entries[2].builtin && entries[3].id == "zeta.glsl";

    std::string text;
    std::string load_error;
    const PostShaderEntry* alpha = FindPostShader(entries, "alpha.glsl");
    const bool found = alpha != nullptr &&
        LoadPostShaderText(*alpha, &text, &load_error) && text == "alpha" &&
        FindPostShader(entries, "crt") == &entries[0] &&
        FindPostShader(entries, engine::kPostShaderNoneId) == nullptr &&
        FindPostShader(entries, "alpha") == nullptr;

    PostShaderEntry missing;
    missing.id = "gone.glsl";
    missing.path = root / "gone.glsl";
    const bool missing_reported =
        !LoadPostShaderText(missing, &text, &load_error) && !load_error.empty();

    std::filesystem::remove_all(root, error);
    return listed && found && missing_reported;
}

}  // namespace

bool RunPostShaderProbe()
{
    const bool assembly = ProbeAssembly();
    const bool parameters = ProbeParameters();
    const bool builtins = ProbeBuiltins();
    const bool catalog = ProbeCatalog();
    // The launcher publishes into the variable the engine reads.
    const bool variable = std::strcmp(launcher::kLauncherPostShaderVariable,
                                      engine::kPostShaderVariable) == 0;
    const bool all = assembly && parameters && builtins && catalog && variable;
    std::cout << "post_shader_assembly=" << (assembly ? "true" : "false")
              << "\npost_shader_parameters=" << (parameters ? "true" : "false")
              << "\npost_shader_builtins=" << (builtins ? "true" : "false")
              << "\npost_shader_catalog=" << (catalog ? "true" : "false")
              << "\npost_shader_variable=" << (variable ? "true" : "false")
              << "\npost_shader_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
