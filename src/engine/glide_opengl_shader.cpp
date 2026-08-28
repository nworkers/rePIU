#include "repiu/engine/glide_opengl_shader.h"

#include "repiu/engine/glide_gl_error_policy.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <sstream>

namespace repiu::engine
{

struct GlideOpenGlShader::Implementation
{
    using CreateShader = GLuint(APIENTRY*)(GLenum);
    using ShaderSource = void(APIENTRY*)(GLuint, GLsizei, const char* const*,
                                         const GLint*);
    using CompileShader = void(APIENTRY*)(GLuint);
    using GetShaderiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteShader = void(APIENTRY*)(GLuint);
    using CreateProgram = GLuint(APIENTRY*)();
    using AttachShader = void(APIENTRY*)(GLuint, GLuint);
    using LinkProgram = void(APIENTRY*)(GLuint);
    using GetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteProgram = void(APIENTRY*)(GLuint);
    using UseProgram = void(APIENTRY*)(GLuint);
    using GetUniformLocation = GLint(APIENTRY*)(GLuint, const char*);
    using Uniform1i = void(APIENTRY*)(GLint, GLint);
    using Uniform1fv = void(APIENTRY*)(GLint, GLsizei, const GLfloat*);
    using Uniform4f = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);

    CreateShader create_shader = nullptr;
    ShaderSource shader_source = nullptr;
    CompileShader compile_shader = nullptr;
    GetShaderiv get_shader_iv = nullptr;
    GetShaderInfoLog get_shader_info_log = nullptr;
    DeleteShader delete_shader = nullptr;
    CreateProgram create_program = nullptr;
    AttachShader attach_shader = nullptr;
    LinkProgram link_program = nullptr;
    GetProgramiv get_program_iv = nullptr;
    GetProgramInfoLog get_program_info_log = nullptr;
    DeleteProgram delete_program = nullptr;
    UseProgram use_program = nullptr;
    GetUniformLocation get_uniform_location = nullptr;
    Uniform1i uniform_1i = nullptr;
    Uniform1fv uniform_1fv = nullptr;
    Uniform4f uniform_4f = nullptr;
    GLuint program = 0;
    GLint alpha_function = -1;
    GLint alpha_factor = -1;
    GLint alpha_local = -1;
    GLint alpha_other = -1;
    GLint alpha_invert = -1;
    GLint color_function = -1;
    GLint color_factor = -1;
    GLint color_local = -1;
    GLint color_other = -1;
    GLint color_invert = -1;
    GLint texture_enable = -1;
    GLint texture_sampler = -1;
    GLint blit_mode = -1;
    GLint constant_color = -1;
    GLint fog_mode = -1;
    GLint fog_color = -1;
    GLint fog_table = -1;
};

namespace
{

constexpr GLenum kGlVertexShader = 0x8B31;
constexpr GLenum kGlFragmentShader = 0x8B30;
constexpr GLenum kGlCompileStatus = 0x8B81;
constexpr GLenum kGlLinkStatus = 0x8B82;

// Task 505: `SDL_FunctionPointer` rather than `void*`, for the reason the
// backend's copy of this function records -- it is what SDL3 returns, MSVC
// converts silently, GCC refuses, and C++ does not guarantee a function pointer
// fits in an object pointer. The failure test is unaffected.
template <typename Function>
bool ResolveOpenGlFunction(const char* name, Function* function)
{
    SDL_FunctionPointer address = SDL_GL_GetProcAddress(name);
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if (address == nullptr || value <= 3U || value == ~std::uintptr_t{0})
    {
        return false;
    }
    *function = reinterpret_cast<Function>(address);
    return true;
}

bool ResolveFunctions(GlideOpenGlShader::Implementation* implementation)
{
    return ResolveOpenGlFunction("glCreateShader",
                                 &implementation->create_shader) &&
        ResolveOpenGlFunction("glShaderSource",
                              &implementation->shader_source) &&
        ResolveOpenGlFunction("glCompileShader",
                              &implementation->compile_shader) &&
        ResolveOpenGlFunction("glGetShaderiv",
                              &implementation->get_shader_iv) &&
        ResolveOpenGlFunction("glGetShaderInfoLog",
                              &implementation->get_shader_info_log) &&
        ResolveOpenGlFunction("glDeleteShader",
                              &implementation->delete_shader) &&
        ResolveOpenGlFunction("glCreateProgram",
                              &implementation->create_program) &&
        ResolveOpenGlFunction("glAttachShader",
                              &implementation->attach_shader) &&
        ResolveOpenGlFunction("glLinkProgram", &implementation->link_program) &&
        ResolveOpenGlFunction("glGetProgramiv",
                              &implementation->get_program_iv) &&
        ResolveOpenGlFunction("glGetProgramInfoLog",
                              &implementation->get_program_info_log) &&
        ResolveOpenGlFunction("glDeleteProgram",
                              &implementation->delete_program) &&
        ResolveOpenGlFunction("glUseProgram", &implementation->use_program) &&
        ResolveOpenGlFunction("glGetUniformLocation",
                              &implementation->get_uniform_location) &&
        ResolveOpenGlFunction("glUniform1i", &implementation->uniform_1i) &&
        ResolveOpenGlFunction("glUniform1fv", &implementation->uniform_1fv) &&
        ResolveOpenGlFunction("glUniform4f", &implementation->uniform_4f);
}

GLuint CompileShader(GlideOpenGlShader::Implementation* implementation,
                     GLenum type,
                     const char* source,
                     std::string* message)
{
    const GLuint shader = implementation->create_shader(type);
    implementation->shader_source(shader, 1, &source, nullptr);
    implementation->compile_shader(shader);
    GLint compiled = GL_FALSE;
    implementation->get_shader_iv(shader, kGlCompileStatus, &compiled);
    if (compiled == GL_TRUE)
    {
        return shader;
    }
    char log[1024]{};
    implementation->get_shader_info_log(shader, sizeof(log), nullptr, log);
    *message = std::string("failed to compile Glide GLSL shader: ") + log;
    implementation->delete_shader(shader);
    return 0;
}

}  // namespace

GlideOpenGlShader::GlideOpenGlShader() = default;

GlideOpenGlShader::~GlideOpenGlShader()
{
    Shutdown();
}

bool GlideOpenGlShader::Initialize()
{
    Shutdown();
    implementation_ = std::make_unique<Implementation>();
    if (!ResolveFunctions(implementation_.get()))
    {
        message_ = "required OpenGL shader entry points are unavailable";
        implementation_.reset();
        return false;
    }
    constexpr char kVertexSource[] =
        "#version 110\n"
        "varying vec4 repiuIteratedColor;\n"
        "varying vec4 repiuTextureFogCoord;\n"
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  repiuIteratedColor = gl_Color;\n"
        "  repiuTextureFogCoord = gl_MultiTexCoord0;\n"
        "}\n";
    constexpr char kFragmentSource[] =
        "#version 110\n"
        "uniform int repiuAlphaFunction;\n"
        "uniform int repiuAlphaFactor;\n"
        "uniform int repiuAlphaLocal;\n"
        "uniform int repiuAlphaOther;\n"
        "uniform int repiuAlphaInvert;\n"
        "uniform int repiuColorFunction;\n"
        "uniform int repiuColorFactor;\n"
        "uniform int repiuColorLocal;\n"
        "uniform int repiuColorOther;\n"
        "uniform int repiuColorInvert;\n"
        "uniform int repiuTextureEnable;\n"
        "uniform sampler2D repiuTexture;\n"
        "uniform int repiuBlitMode;\n"
        "uniform vec4 repiuConstantColor;\n"
        "uniform int repiuFogMode;\n"
        "uniform vec4 repiuFogColor;\n"
        "uniform float repiuFogTable[64];\n"
        "varying vec4 repiuIteratedColor;\n"
        "varying vec4 repiuTextureFogCoord;\n"
        "float repiuFogFactor(float oow) {\n"
        "  if (oow <= 0.0) return 0.0;\n"
        "  float w = 1.0 / oow;\n"
        "  if (w <= 1.0) return repiuFogTable[0];\n"
        "  if (w >= 52428.8) return repiuFogTable[63];\n"
        "  float group = floor(log2(w));\n"
        "  float normalized = w / exp2(group);\n"
        "  int offset = 0;\n"
        "  float lower = 1.0;\n"
        "  float upper = 8.0 / 7.0;\n"
        "  if (normalized > 8.0 / 7.0) { offset = 1; lower = 8.0 / 7.0; upper = 4.0 / 3.0; }\n"
        "  if (normalized > 4.0 / 3.0) { offset = 2; lower = 4.0 / 3.0; upper = 8.0 / 5.0; }\n"
        "  if (normalized > 8.0 / 5.0) { offset = 3; lower = 8.0 / 5.0; upper = 2.0; }\n"
        "  int index = int(group) * 4 + offset;\n"
        "  float fraction = clamp((normalized - lower) / (upper - lower), 0.0, 1.0);\n"
        "  return mix(repiuFogTable[index], repiuFogTable[index + 1], fraction);\n"
        "}\n"
        "void main() {\n"
        "  vec2 uv = repiuTextureFogCoord.xy / max(repiuTextureFogCoord.w, 0.00000000000000000001);\n"
        "  if (repiuBlitMode != 0) { gl_FragColor = texture2D(repiuTexture, uv); return; }\n"
        "  vec4 tex = repiuTextureEnable != 0 ? texture2D(repiuTexture, uv) : vec4(1.0);\n"
        "  float a_loc = (repiuAlphaLocal == 0) ? repiuIteratedColor.a : ((repiuAlphaLocal == 1) ? repiuConstantColor.a : 0.0);\n"
        "  float a_oth = (repiuAlphaOther == 0) ? repiuIteratedColor.a : ((repiuAlphaOther == 1) ? tex.a : ((repiuAlphaOther == 2) ? repiuConstantColor.a : 0.0));\n"
        "  float a_fac = (repiuAlphaFactor == 0) ? 0.0 : ((repiuAlphaFactor == 1) ? a_loc : ((repiuAlphaFactor == 2) ? a_oth : ((repiuAlphaFactor == 8) ? 1.0 : 0.0)));\n"
        "  float alpha = 1.0;\n"
        "  if (repiuAlphaFunction == 1) alpha = a_loc;\n"
        "  else if (repiuAlphaFunction == 2) alpha = a_oth;\n"
        "  else if (repiuAlphaFunction == 3) alpha = a_oth * a_fac;\n"
        "  else if (repiuAlphaFunction == 4) alpha = a_oth * a_fac + a_loc;\n"
        "  else if (repiuAlphaFunction == 7) alpha = (a_oth - a_loc) * a_fac + a_loc;\n"
        "  if (repiuAlphaInvert != 0) alpha = 1.0 - alpha;\n"
        "  vec3 c_loc = (repiuColorLocal == 0) ? repiuIteratedColor.rgb : ((repiuColorLocal == 1) ? repiuConstantColor.rgb : vec3(0.0));\n"
        "  vec3 c_oth = (repiuColorOther == 0) ? repiuIteratedColor.rgb : ((repiuColorOther == 1) ? tex.rgb : ((repiuColorOther == 2) ? repiuConstantColor.rgb : vec3(0.0)));\n"
        "  vec3 c_fac = (repiuColorFactor == 0) ? vec3(0.0) : ((repiuColorFactor == 1) ? c_loc : ((repiuColorFactor == 2) ? vec3(a_oth) : ((repiuColorFactor == 3) ? vec3(a_loc) : ((repiuColorFactor == 8) ? vec3(1.0) : vec3(0.0)))));\n"
        "  vec3 color = vec3(1.0);\n"
        "  if (repiuColorFunction == 1) color = c_loc;\n"
        "  else if (repiuColorFunction == 2) color = c_oth;\n"
        "  else if (repiuColorFunction == 3) color = c_oth * c_fac;\n"
        "  else if (repiuColorFunction == 4) color = c_oth * c_fac + c_loc;\n"
        "  else if (repiuColorFunction == 7) color = (c_oth - c_loc) * c_fac + c_loc;\n"
        "  if (repiuColorInvert != 0) color = vec3(1.0) - color;\n"
        "  if (repiuFogMode == 2) color = mix(color, repiuFogColor.rgb, clamp(repiuFogFactor(repiuTextureFogCoord.z), 0.0, 1.0));\n"
        "  gl_FragColor = vec4(color, alpha);\n"
        "}\n";
    const GLuint vertex = CompileShader(implementation_.get(),
                                        kGlVertexShader,
                                        kVertexSource,
                                        &message_);
    if (vertex == 0)
    {
        implementation_.reset();
        return false;
    }
    const GLuint fragment = CompileShader(implementation_.get(),
                                          kGlFragmentShader,
                                          kFragmentSource,
                                          &message_);
    if (fragment == 0)
    {
        implementation_->delete_shader(vertex);
        implementation_.reset();
        return false;
    }
    implementation_->program = implementation_->create_program();
    implementation_->attach_shader(implementation_->program, vertex);
    implementation_->attach_shader(implementation_->program, fragment);
    implementation_->link_program(implementation_->program);
    implementation_->delete_shader(vertex);
    implementation_->delete_shader(fragment);
    GLint linked = GL_FALSE;
    implementation_->get_program_iv(implementation_->program,
                                    kGlLinkStatus,
                                    &linked);
    if (linked != GL_TRUE)
    {
        char log[1024]{};
        implementation_->get_program_info_log(implementation_->program,
                                              sizeof(log), nullptr, log);
        message_ = std::string("failed to link Glide GLSL program: ") + log;
        Shutdown();
        return false;
    }
    implementation_->alpha_function =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuAlphaFunction");
    implementation_->alpha_factor =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuAlphaFactor");
    implementation_->alpha_local =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuAlphaLocal");
    implementation_->alpha_other =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuAlphaOther");
    implementation_->alpha_invert =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuAlphaInvert");
    implementation_->color_function =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorFunction");
    implementation_->color_factor =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorFactor");
    implementation_->color_local =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorLocal");
    implementation_->color_other =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorOther");
    implementation_->color_invert =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorInvert");
    implementation_->texture_enable =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuTextureEnable");
    implementation_->texture_sampler =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuTexture");
    implementation_->blit_mode =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuBlitMode");
    implementation_->constant_color =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuConstantColor");
    implementation_->fog_mode =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuFogMode");
    implementation_->fog_color =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuFogColor");
    implementation_->fog_table =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuFogTable");
    if (implementation_->alpha_function < 0 ||
        implementation_->alpha_factor < 0 ||
        implementation_->alpha_local < 0 ||
        implementation_->alpha_other < 0 ||
        implementation_->alpha_invert < 0 ||
        implementation_->color_function < 0 ||
        implementation_->color_factor < 0 ||
        implementation_->color_local < 0 ||
        implementation_->color_other < 0 ||
        implementation_->color_invert < 0 ||
        implementation_->texture_enable < 0 ||
        implementation_->texture_sampler < 0 ||
        implementation_->blit_mode < 0 ||
        implementation_->constant_color < 0 ||
        implementation_->fog_mode < 0 ||
        implementation_->fog_color < 0 ||
        implementation_->fog_table < 0)
    {
        message_ = "Glide GLSL alpha-combine uniforms are unavailable";
        Shutdown();
        return false;
    }
    implementation_->use_program(implementation_->program);
    // Seed the combine functions to LOCAL (1) so a draw that precedes the game's
    // grColorCombine/grAlphaCombine setup (or a retained unsupported equation)
    // emits the iterated vertex color rather than a black fragment. The game's
    // observed init combine (1,0,0,2,0) later confirms this same value.
    implementation_->uniform_1i(implementation_->color_function, 1);
    implementation_->uniform_1i(implementation_->color_factor, 0);
    implementation_->uniform_1i(implementation_->color_local, 0);
    implementation_->uniform_1i(implementation_->color_other, 2);
    implementation_->uniform_1i(implementation_->color_invert, 0);
    implementation_->uniform_1i(implementation_->alpha_function, 1);
    implementation_->uniform_1i(implementation_->alpha_factor, 0);
    implementation_->uniform_1i(implementation_->alpha_local, 0);
    implementation_->uniform_1i(implementation_->alpha_other, 2);
    implementation_->uniform_1i(implementation_->alpha_invert, 0);
    // Texture sampling starts disabled and binds to texture unit 0 (R3).
    implementation_->uniform_1i(implementation_->texture_enable, 0);
    implementation_->uniform_1i(implementation_->texture_sampler, 0);
    implementation_->uniform_1i(implementation_->blit_mode, 0);
    implementation_->uniform_1i(implementation_->fog_mode, 0);
    implementation_->uniform_4f(implementation_->constant_color,
                                1.0F, 1.0F, 1.0F, 1.0F);
    implementation_->uniform_4f(implementation_->fog_color,
                                0.0F, 0.0F, 0.0F, 1.0F);
    GLfloat empty_fog_table[hle::kGlideFogTableEntryCount]{};
    implementation_->uniform_1fv(implementation_->fog_table,
                                 hle::kGlideFogTableEntryCount,
                                 empty_fog_table);
    message_ = "Glide GLSL combine program initialized";
    return true;
}

void GlideOpenGlShader::SetTextureEnabled(bool enabled)
{
    if (!implementation_ || implementation_->program == 0 ||
        implementation_->texture_enable < 0)
    {
        return;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->texture_enable,
                                enabled ? 1 : 0);
}

void GlideOpenGlShader::SetBlitMode(bool enabled)
{
    if (!implementation_ || implementation_->program == 0 ||
        implementation_->blit_mode < 0)
    {
        return;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->blit_mode,
                                enabled ? 1 : 0);
}

bool GlideOpenGlShader::SetFogMode(std::uint32_t mode)
{
    if (!implementation_ || implementation_->program == 0 ||
        implementation_->fog_mode < 0 || (mode != 0U && mode != 2U))
    {
        return false;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->fog_mode,
                                static_cast<GLint>(mode));
    return !GlideGlErrorCheckPolicyEnabled() || glGetError() == GL_NO_ERROR;
}

bool GlideOpenGlShader::SetFogColor(std::uint32_t argb)
{
    if (!implementation_ || implementation_->program == 0 ||
        implementation_->fog_color < 0)
    {
        return false;
    }
    const float a = static_cast<float>((argb >> 24) & 0xFF) / 255.0F;
    const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0F;
    const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0F;
    const float b = static_cast<float>(argb & 0xFF) / 255.0F;
    implementation_->use_program(implementation_->program);
    implementation_->uniform_4f(implementation_->fog_color, r, g, b, a);
    return !GlideGlErrorCheckPolicyEnabled() || glGetError() == GL_NO_ERROR;
}

bool GlideOpenGlShader::SetFogTable(const hle::GlideFogTable& table)
{
    if (!implementation_ || implementation_->program == 0 ||
        implementation_->fog_table < 0)
    {
        return false;
    }
    GLfloat normalized[hle::kGlideFogTableEntryCount]{};
    for (std::size_t index = 0U; index < table.size(); ++index)
    {
        normalized[index] = static_cast<GLfloat>(table[index]) / 255.0F;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1fv(
        implementation_->fog_table,
        static_cast<GLsizei>(hle::kGlideFogTableEntryCount), normalized);
    return !GlideGlErrorCheckPolicyEnabled() || glGetError() == GL_NO_ERROR;
}

bool GlideOpenGlShader::SetAlphaCombine(
    const hle::GlideAlphaCombineState& state)
{
    if (!implementation_ || implementation_->program == 0 || !state.valid)
    {
        message_ = "unsupported Glide alpha-combine equation";
        return false;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->alpha_function,
                                static_cast<GLint>(state.function));
    implementation_->uniform_1i(implementation_->alpha_factor,
                                static_cast<GLint>(state.factor));
    implementation_->uniform_1i(implementation_->alpha_local,
                                static_cast<GLint>(state.local));
    implementation_->uniform_1i(implementation_->alpha_other,
                                static_cast<GLint>(state.other));
    implementation_->uniform_1i(implementation_->alpha_invert,
                                state.invert ? 1 : 0);
    if (GlideGlErrorCheckPolicyEnabled() && glGetError() != GL_NO_ERROR)
    {
        message_ = "failed to apply Glide alpha-combine uniforms";
        return false;
    }
    message_ = "observed Glide alpha-combine equation applied with GLSL";
    return true;
}

bool GlideOpenGlShader::SetColorCombine(
    const hle::GlideColorCombineState& state)
{
    if (!implementation_ || implementation_->program == 0 || !state.valid)
    {
        message_ = "unsupported Glide color-combine equation";
        return false;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->color_function,
                                static_cast<GLint>(state.function));
    implementation_->uniform_1i(implementation_->color_factor,
                                static_cast<GLint>(state.factor));
    implementation_->uniform_1i(implementation_->color_local,
                                static_cast<GLint>(state.local));
    implementation_->uniform_1i(implementation_->color_other,
                                static_cast<GLint>(state.other));
    implementation_->uniform_1i(implementation_->color_invert,
                                state.invert ? 1 : 0);
    if (GlideGlErrorCheckPolicyEnabled() && glGetError() != GL_NO_ERROR)
    {
        message_ = "failed to apply Glide color-combine uniforms";
        return false;
    }
    message_ = "observed Glide color-combine equation applied with GLSL";
    return true;
}

bool GlideOpenGlShader::SetConstantColor(std::uint32_t argb)
{
    if (!implementation_ || implementation_->program == 0 ||
        implementation_->constant_color < 0)
    {
        return false;
    }
    implementation_->use_program(implementation_->program);
    const float a = static_cast<float>((argb >> 24) & 0xFF) / 255.0F;
    const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0F;
    const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0F;
    const float b = static_cast<float>(argb & 0xFF) / 255.0F;
    implementation_->uniform_4f(implementation_->constant_color, r, g, b, a);
    return true;
}

void GlideOpenGlShader::Shutdown()
{
    if (implementation_ && implementation_->program != 0 &&
        implementation_->delete_program != nullptr)
    {
        implementation_->use_program(0);
        implementation_->delete_program(implementation_->program);
    }
    implementation_.reset();
}

}  // namespace repiu::engine
