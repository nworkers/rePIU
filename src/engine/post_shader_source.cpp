#include "repiu/engine/post_shader_source.h"

#include <cctype>
#include <locale>
#include <sstream>

namespace repiu::engine
{
namespace
{

std::string_view TrimLeft(std::string_view text)
{
    std::size_t index = 0;
    while (index < text.size() &&
           (text[index] == ' ' || text[index] == '\t'))
    {
        ++index;
    }
    return text.substr(index);
}

bool StartsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() &&
        text.substr(0, prefix.size()) == prefix;
}

bool IsIdentifier(std::string_view text)
{
    if (text.empty() ||
        !(std::isalpha(static_cast<unsigned char>(text.front())) != 0 ||
          text.front() == '_'))
    {
        return false;
    }
    for (const char character : text)
    {
        if (std::isalnum(static_cast<unsigned char>(character)) == 0 &&
            character != '_')
        {
            return false;
        }
    }
    return true;
}

// `#pragma parameter NAME "Description" initial minimum maximum [step]`.
bool ParseParameterLine(std::string_view line, PostShaderParameter* parameter,
                        std::string* error)
{
    std::istringstream stream{std::string(line)};
    stream.imbue(std::locale::classic());
    std::string pragma;
    std::string keyword;
    stream >> pragma >> keyword >> parameter->name;
    if (!IsIdentifier(parameter->name))
    {
        *error = "parameter name is not an identifier: " + std::string(line);
        return false;
    }
    stream >> std::ws;
    if (stream.peek() != '"')
    {
        *error = "parameter description is not quoted: " + std::string(line);
        return false;
    }
    stream.get();
    if (!std::getline(stream, parameter->description, '"'))
    {
        *error = "parameter description is not closed: " + std::string(line);
        return false;
    }
    if (!(stream >> parameter->initial >> parameter->minimum >>
          parameter->maximum))
    {
        *error = "parameter needs initial, minimum and maximum values: " +
            std::string(line);
        return false;
    }
    if (!(stream >> parameter->step))
    {
        parameter->step = 0.0F;
    }
    if (parameter->minimum > parameter->maximum)
    {
        *error = "parameter minimum exceeds its maximum: " + std::string(line);
        return false;
    }
    parameter->value = parameter->initial;
    return true;
}

std::string AssembleStage(std::string_view text, std::string_view stage_define)
{
    std::string defines = "#define ";
    defines.append(stage_define);
    defines.append("\n#define PARAMETER_UNIFORM\n");

    const std::string_view first_line =
        text.substr(0, text.find('\n'));
    std::string result;
    result.reserve(text.size() + defines.size() + 1U);
    if (StartsWith(TrimLeft(first_line), "#version"))
    {
        const std::size_t line_end =
            first_line.size() < text.size() ? first_line.size() + 1U
                                            : text.size();
        result.append(text.substr(0, line_end));
        if (line_end == text.size() &&
            (result.empty() || result.back() != '\n'))
        {
            result.push_back('\n');
        }
        result.append(defines);
        result.append(text.substr(line_end));
    }
    else
    {
        result.append(defines);
        result.append(text);
    }
    return result;
}

}  // namespace

bool BuildPostShaderProgramSource(std::string_view text,
                                  PostShaderProgramSource* source,
                                  std::string* error)
{
    std::string scratch;
    if (error == nullptr)
    {
        error = &scratch;
    }
    if (source == nullptr)
    {
        *error = "no destination for the shader source";
        return false;
    }
    if (text.empty())
    {
        *error = "shader text is empty";
        return false;
    }
    if (text.find("VERTEX") == std::string_view::npos ||
        text.find("FRAGMENT") == std::string_view::npos)
    {
        *error = "shader must guard a VERTEX and a FRAGMENT stage in one file";
        return false;
    }

    std::vector<PostShaderParameter> parameters;
    std::size_t line_start = 0;
    while (line_start < text.size())
    {
        std::size_t line_end = text.find('\n', line_start);
        if (line_end == std::string_view::npos)
        {
            line_end = text.size();
        }
        std::string_view line =
            text.substr(line_start, line_end - line_start);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        const std::string_view trimmed = TrimLeft(line);
        if (StartsWith(trimmed, "#pragma") &&
            StartsWith(TrimLeft(trimmed.substr(7)), "parameter"))
        {
            PostShaderParameter parameter;
            if (!ParseParameterLine(trimmed, &parameter, error))
            {
                return false;
            }
            bool duplicate = false;
            for (const PostShaderParameter& existing : parameters)
            {
                duplicate = duplicate || existing.name == parameter.name;
            }
            if (!duplicate)
            {
                parameters.push_back(std::move(parameter));
            }
        }
        line_start = line_end + 1U;
    }

    source->vertex = AssembleStage(text, "VERTEX");
    source->fragment = AssembleStage(text, "FRAGMENT");
    source->parameters = std::move(parameters);
    return true;
}

}  // namespace repiu::engine
