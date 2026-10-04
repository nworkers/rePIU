#ifndef REPIU_ENGINE_POST_SHADER_SOURCE_H_
#define REPIU_ENGINE_POST_SHADER_SOURCE_H_

#include <string>
#include <string_view>
#include <vector>

namespace repiu::engine
{

// Task 768. One `#pragma parameter` line of a post-processing shader: a float
// uniform the OSD exposes as a slider. `value` starts at `initial` and is what
// the pass uploads every frame.
struct PostShaderParameter
{
    std::string name;
    std::string description;
    float initial = 0.0F;
    float minimum = 0.0F;
    float maximum = 0.0F;
    float step = 0.0F;
    float value = 0.0F;
};

// The two stages assembled from one single-pass shader file in the libretro
// GLSL layout: the same text compiled once with `VERTEX` and once with
// `FRAGMENT` defined, `PARAMETER_UNIFORM` defined in both so parameters become
// uniforms rather than constants.
struct PostShaderProgramSource
{
    std::string vertex;
    std::string fragment;
    std::vector<PostShaderParameter> parameters;
};

// Builds both stages from a shader file's text. The defines go after the first
// line when that line is a `#version` directive, which GLSL requires to come
// first, and before everything otherwise. Fails, with `error` saying why, on
// an empty text, a text with neither stage guarded, or a malformed
// `#pragma parameter` line; a shader the driver would reject is left to the
// compiler, which reports it better.
[[nodiscard]] bool BuildPostShaderProgramSource(std::string_view text,
                                                PostShaderProgramSource* source,
                                                std::string* error);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_POST_SHADER_SOURCE_H_
