#include "repiu/platform/win32/glide_opengl_shader.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#endif

#include <sstream>

namespace repiu::platform::win32
{

struct GlideOpenGlShader::Implementation
{
#if defined(_WIN32)
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
    GLuint program = 0;
    GLint alpha_function = -1;
    GLint alpha_invert = -1;
    GLint color_function = -1;
    GLint color_invert = -1;
#endif
};

namespace
{

#if defined(_WIN32)
constexpr GLenum kGlVertexShader = 0x8B31;
constexpr GLenum kGlFragmentShader = 0x8B30;
constexpr GLenum kGlCompileStatus = 0x8B81;
constexpr GLenum kGlLinkStatus = 0x8B82;

template <typename Function>
bool ResolveOpenGlFunction(const char* name, Function* function)
{
    PROC address = wglGetProcAddress(name);
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
        ResolveOpenGlFunction("glUniform1i", &implementation->uniform_1i);
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
#endif

}  // namespace

GlideOpenGlShader::GlideOpenGlShader() = default;

GlideOpenGlShader::~GlideOpenGlShader()
{
    Shutdown();
}

bool GlideOpenGlShader::Initialize()
{
    Shutdown();
#if !defined(_WIN32)
    message_ = "Win32 GLSL shader backend is unavailable";
    return false;
#else
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
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  repiuIteratedColor = gl_Color;\n"
        "}\n";
    constexpr char kFragmentSource[] =
        "#version 110\n"
        "uniform int repiuAlphaFunction;\n"
        "uniform int repiuAlphaInvert;\n"
        "uniform int repiuColorFunction;\n"
        "uniform int repiuColorInvert;\n"
        "varying vec4 repiuIteratedColor;\n"
        "void main() {\n"
        "  float alpha = repiuAlphaFunction == 1"
        " ? repiuIteratedColor.a : 0.0;\n"
        "  if (repiuAlphaInvert != 0) alpha = 1.0 - alpha;\n"
        "  vec3 color = repiuColorFunction == 1"
        " ? repiuIteratedColor.rgb : vec3(0.0);\n"
        "  if (repiuColorInvert != 0) color = vec3(1.0) - color;\n"
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
    implementation_->alpha_invert =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuAlphaInvert");
    implementation_->color_function =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorFunction");
    implementation_->color_invert =
        implementation_->get_uniform_location(implementation_->program,
                                              "repiuColorInvert");
    if (implementation_->alpha_function < 0 ||
        implementation_->alpha_invert < 0 ||
        implementation_->color_function < 0 ||
        implementation_->color_invert < 0)
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
    implementation_->uniform_1i(implementation_->color_invert, 0);
    implementation_->uniform_1i(implementation_->alpha_function, 1);
    implementation_->uniform_1i(implementation_->alpha_invert, 0);
    message_ = "Glide GLSL combine program initialized";
    return true;
#endif
}

bool GlideOpenGlShader::SetAlphaCombine(
    const hle::GlideAlphaCombineState& state)
{
#if !defined(_WIN32)
    return false;
#else
    if (!implementation_ || implementation_->program == 0 || !state.valid ||
        state.function != 1U || state.factor != 0U || state.local != 0U ||
        state.other != 2U || state.invert)
    {
        message_ = "unsupported Glide alpha-combine equation";
        return false;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->alpha_function,
                                static_cast<GLint>(state.function));
    implementation_->uniform_1i(implementation_->alpha_invert,
                                state.invert ? 1 : 0);
    if (glGetError() != GL_NO_ERROR)
    {
        message_ = "failed to apply Glide alpha-combine uniforms";
        return false;
    }
    message_ = "observed Glide alpha-combine equation applied with GLSL";
    return true;
#endif
}

bool GlideOpenGlShader::SetColorCombine(
    const hle::GlideColorCombineState& state)
{
#if !defined(_WIN32)
    return false;
#else
    if (!implementation_ || implementation_->program == 0 || !state.valid ||
        state.function != 1U || state.factor != 0U || state.local != 0U ||
        state.other != 2U || state.invert)
    {
        message_ = "unsupported Glide color-combine equation";
        return false;
    }
    implementation_->use_program(implementation_->program);
    implementation_->uniform_1i(implementation_->color_function,
                                static_cast<GLint>(state.function));
    implementation_->uniform_1i(implementation_->color_invert,
                                state.invert ? 1 : 0);
    if (glGetError() != GL_NO_ERROR)
    {
        message_ = "failed to apply Glide color-combine uniforms";
        return false;
    }
    message_ = "observed Glide color-combine equation applied with GLSL";
    return true;
#endif
}

void GlideOpenGlShader::Shutdown()
{
#if defined(_WIN32)
    if (implementation_ && implementation_->program != 0 &&
        implementation_->delete_program != nullptr)
    {
        implementation_->use_program(0);
        implementation_->delete_program(implementation_->program);
    }
#endif
    implementation_.reset();
}

}  // namespace repiu::platform::win32
