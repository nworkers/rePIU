#include "repiu/engine/glide_post_process.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace repiu::engine
{

struct GlidePostProcess::Implementation
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
    using BindAttribLocation = void(APIENTRY*)(GLuint, GLuint, const char*);
    using LinkProgram = void(APIENTRY*)(GLuint);
    using GetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteProgram = void(APIENTRY*)(GLuint);
    using UseProgram = void(APIENTRY*)(GLuint);
    using GetUniformLocation = GLint(APIENTRY*)(GLuint, const char*);
    using Uniform1i = void(APIENTRY*)(GLint, GLint);
    using Uniform1f = void(APIENTRY*)(GLint, GLfloat);
    using Uniform2f = void(APIENTRY*)(GLint, GLfloat, GLfloat);
    using UniformMatrix4fv = void(APIENTRY*)(GLint, GLsizei, GLboolean,
                                             const GLfloat*);
    using VertexAttrib4f = void(APIENTRY*)(GLuint, GLfloat, GLfloat, GLfloat,
                                           GLfloat);
    using ActiveTexture = void(APIENTRY*)(GLenum);

    CreateShader create_shader = nullptr;
    ShaderSource shader_source = nullptr;
    CompileShader compile_shader = nullptr;
    GetShaderiv get_shader_iv = nullptr;
    GetShaderInfoLog get_shader_info_log = nullptr;
    DeleteShader delete_shader = nullptr;
    CreateProgram create_program = nullptr;
    AttachShader attach_shader = nullptr;
    BindAttribLocation bind_attrib_location = nullptr;
    LinkProgram link_program = nullptr;
    GetProgramiv get_program_iv = nullptr;
    GetProgramInfoLog get_program_info_log = nullptr;
    DeleteProgram delete_program = nullptr;
    UseProgram use_program = nullptr;
    GetUniformLocation get_uniform_location = nullptr;
    Uniform1i uniform_1i = nullptr;
    Uniform1f uniform_1f = nullptr;
    Uniform2f uniform_2f = nullptr;
    UniformMatrix4fv uniform_matrix_4fv = nullptr;
    VertexAttrib4f vertex_attrib_4f = nullptr;
    ActiveTexture active_texture = nullptr;

    bool initialized = false;
    std::filesystem::path directory;
    std::string directory_text;
    std::vector<PostShaderEntry> catalog;
    std::string active_id = kPostShaderNoneId;
    std::string last_error;
    std::vector<PostShaderParameter> parameters;
    std::vector<GLint> parameter_locations;

    GLuint program = 0;
    GLint mvp_matrix = -1;
    GLint texture = -1;
    GLint input_size = -1;
    GLint texture_size = -1;
    GLint output_size = -1;
    GLint frame_count = -1;
    GLint frame_direction = -1;

    GLuint scene_texture = 0;
    std::uint32_t scene_width = 0;
    std::uint32_t scene_height = 0;
    std::uint32_t frames = 0;
};

namespace
{

constexpr GLenum kGlVertexShader = 0x8B31;
constexpr GLenum kGlFragmentShader = 0x8B30;
constexpr GLenum kGlCompileStatus = 0x8B81;
constexpr GLenum kGlLinkStatus = 0x8B82;
constexpr GLenum kGlInfoLogLength = 0x8B84;
constexpr GLenum kGlCurrentProgram = 0x8B8D;
constexpr GLenum kGlTexture0 = 0x84C0;
constexpr GLenum kGlClampToEdge = 0x812F;
constexpr GLenum kGlRgba8 = 0x8058;

// The attribute slots bound before linking. Slot 0 aliases the fixed-function
// vertex, so setting it inside glBegin/glEnd emits the vertex.
constexpr GLuint kVertexCoordSlot = 0;
constexpr GLuint kTexCoordSlot = 1;
constexpr GLuint kColorSlot = 2;

// Orthographic projection of the unit square onto clip space, column-major.
constexpr GLfloat kUnitSquareProjection[16] = {
    2.0F, 0.0F, 0.0F, 0.0F,
    0.0F, 2.0F, 0.0F, 0.0F,
    0.0F, 0.0F, -1.0F, 0.0F,
    -1.0F, -1.0F, 0.0F, 1.0F,
};

// SDL_FunctionPointer rather than void*, for the reason the shader module's
// copy records: C++ does not promise a function pointer fits an object one.
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

bool ResolveFunctions(GlidePostProcess::Implementation* gl)
{
    return ResolveOpenGlFunction("glCreateShader", &gl->create_shader) &&
        ResolveOpenGlFunction("glShaderSource", &gl->shader_source) &&
        ResolveOpenGlFunction("glCompileShader", &gl->compile_shader) &&
        ResolveOpenGlFunction("glGetShaderiv", &gl->get_shader_iv) &&
        ResolveOpenGlFunction("glGetShaderInfoLog",
                              &gl->get_shader_info_log) &&
        ResolveOpenGlFunction("glDeleteShader", &gl->delete_shader) &&
        ResolveOpenGlFunction("glCreateProgram", &gl->create_program) &&
        ResolveOpenGlFunction("glAttachShader", &gl->attach_shader) &&
        ResolveOpenGlFunction("glBindAttribLocation",
                              &gl->bind_attrib_location) &&
        ResolveOpenGlFunction("glLinkProgram", &gl->link_program) &&
        ResolveOpenGlFunction("glGetProgramiv", &gl->get_program_iv) &&
        ResolveOpenGlFunction("glGetProgramInfoLog",
                              &gl->get_program_info_log) &&
        ResolveOpenGlFunction("glDeleteProgram", &gl->delete_program) &&
        ResolveOpenGlFunction("glUseProgram", &gl->use_program) &&
        ResolveOpenGlFunction("glGetUniformLocation",
                              &gl->get_uniform_location) &&
        ResolveOpenGlFunction("glUniform1i", &gl->uniform_1i) &&
        ResolveOpenGlFunction("glUniform1f", &gl->uniform_1f) &&
        ResolveOpenGlFunction("glUniform2f", &gl->uniform_2f) &&
        ResolveOpenGlFunction("glUniformMatrix4fv",
                              &gl->uniform_matrix_4fv) &&
        ResolveOpenGlFunction("glVertexAttrib4f", &gl->vertex_attrib_4f) &&
        ResolveOpenGlFunction("glActiveTexture", &gl->active_texture);
}

std::string ShaderLog(GlidePostProcess::Implementation* gl, GLuint shader)
{
    GLint length = 0;
    gl->get_shader_iv(shader, kGlInfoLogLength, &length);
    std::string log(length > 0 ? static_cast<std::size_t>(length) : 0U, '\0');
    if (length > 0)
    {
        gl->get_shader_info_log(shader, length, nullptr, log.data());
    }
    while (!log.empty() && (log.back() == '\0' || log.back() == '\n'))
    {
        log.pop_back();
    }
    return log;
}

std::string ProgramLog(GlidePostProcess::Implementation* gl, GLuint program)
{
    GLint length = 0;
    gl->get_program_iv(program, kGlInfoLogLength, &length);
    std::string log(length > 0 ? static_cast<std::size_t>(length) : 0U, '\0');
    if (length > 0)
    {
        gl->get_program_info_log(program, length, nullptr, log.data());
    }
    while (!log.empty() && (log.back() == '\0' || log.back() == '\n'))
    {
        log.pop_back();
    }
    return log;
}

GLuint CompileStage(GlidePostProcess::Implementation* gl, GLenum type,
                    const std::string& source, std::string* error)
{
    const GLuint shader = gl->create_shader(type);
    const char* text = source.c_str();
    gl->shader_source(shader, 1, &text, nullptr);
    gl->compile_shader(shader);
    GLint compiled = GL_FALSE;
    gl->get_shader_iv(shader, kGlCompileStatus, &compiled);
    if (compiled != GL_TRUE)
    {
        *error = std::string(type == kGlVertexShader ? "vertex" : "fragment") +
            " stage failed to compile: " + ShaderLog(gl, shader);
        gl->delete_shader(shader);
        return 0;
    }
    return shader;
}

void ReleaseProgram(GlidePostProcess::Implementation* gl)
{
    if (gl->program != 0U)
    {
        gl->delete_program(gl->program);
        gl->program = 0;
    }
    gl->parameters.clear();
    gl->parameter_locations.clear();
    gl->active_id = kPostShaderNoneId;
}

std::filesystem::path ExecutableDirectory()
{
    const char* base = SDL_GetBasePath();
    return base != nullptr ? std::filesystem::path(base)
                           : std::filesystem::path();
}

}  // namespace

GlidePostProcess::GlidePostProcess()
    : implementation_(std::make_unique<Implementation>())
{
}

GlidePostProcess::~GlidePostProcess() = default;

bool GlidePostProcess::Initialize(std::string* message)
{
    Implementation* gl = implementation_.get();
    if (gl->initialized)
    {
        return true;
    }
    if (!ResolveFunctions(gl))
    {
        if (message != nullptr)
        {
            *message = "OpenGL shader entry points are unavailable";
        }
        return false;
    }
    gl->directory = ResolvePostShaderDirectory(ExecutableDirectory());
    gl->directory_text = gl->directory.string();
    gl->catalog = ListPostShaders(gl->directory);
    gl->initialized = true;
    return true;
}

void GlidePostProcess::Shutdown()
{
    Implementation* gl = implementation_.get();
    if (!gl->initialized)
    {
        return;
    }
    ReleaseProgram(gl);
    if (gl->scene_texture != 0U)
    {
        GLuint name = gl->scene_texture;
        glDeleteTextures(1, &name);
        gl->scene_texture = 0;
    }
    gl->scene_width = 0;
    gl->scene_height = 0;
    gl->initialized = false;
}

bool GlidePostProcess::initialized() const
{
    return implementation_->initialized;
}

const std::vector<PostShaderEntry>& GlidePostProcess::catalog() const
{
    return implementation_->catalog;
}

const std::string& GlidePostProcess::shader_directory() const
{
    return implementation_->directory_text;
}

const std::string& GlidePostProcess::active_id() const
{
    return implementation_->active_id;
}

bool GlidePostProcess::active() const
{
    return implementation_->program != 0U;
}

const std::string& GlidePostProcess::last_error() const
{
    return implementation_->last_error;
}

std::vector<PostShaderParameter>& GlidePostProcess::parameters()
{
    return implementation_->parameters;
}

bool GlidePostProcess::Select(std::string_view id)
{
    Implementation* gl = implementation_.get();
    gl->last_error.clear();
    if (!gl->initialized)
    {
        gl->last_error = "post-processing is not initialized";
        return false;
    }
    ReleaseProgram(gl);
    if (id.empty() || id == kPostShaderNoneId)
    {
        std::fprintf(stderr, "[repiu-post] shader: none\n");
        return true;
    }

    const PostShaderEntry* entry = FindPostShader(gl->catalog, id);
    std::string text;
    PostShaderProgramSource source;
    if (entry == nullptr)
    {
        gl->last_error = "no shader named '" + std::string(id) + "' in " +
            gl->directory_text + " or built in";
    }
    else if (LoadPostShaderText(*entry, &text, &gl->last_error) &&
             BuildPostShaderProgramSource(text, &source, &gl->last_error))
    {
        const GLuint vertex =
            CompileStage(gl, kGlVertexShader, source.vertex, &gl->last_error);
        const GLuint fragment = vertex == 0U
            ? 0U
            : CompileStage(gl, kGlFragmentShader, source.fragment,
                           &gl->last_error);
        if (vertex != 0U && fragment != 0U)
        {
            const GLuint program = gl->create_program();
            gl->attach_shader(program, vertex);
            gl->attach_shader(program, fragment);
            gl->bind_attrib_location(program, kVertexCoordSlot, "VertexCoord");
            gl->bind_attrib_location(program, kTexCoordSlot, "TexCoord");
            gl->bind_attrib_location(program, kColorSlot, "COLOR");
            gl->link_program(program);
            GLint linked = GL_FALSE;
            gl->get_program_iv(program, kGlLinkStatus, &linked);
            if (linked == GL_TRUE)
            {
                gl->program = program;
            }
            else
            {
                gl->last_error = "link failed: " + ProgramLog(gl, program);
                gl->delete_program(program);
            }
        }
        if (vertex != 0U)
        {
            gl->delete_shader(vertex);
        }
        if (fragment != 0U)
        {
            gl->delete_shader(fragment);
        }
    }

    if (gl->program == 0U)
    {
        std::fprintf(stderr, "[repiu-post] shader '%.*s' not applied: %s\n",
                     static_cast<int>(id.size()), id.data(),
                     gl->last_error.c_str());
        return false;
    }

    gl->active_id = std::string(id);
    gl->mvp_matrix = gl->get_uniform_location(gl->program, "MVPMatrix");
    gl->texture = gl->get_uniform_location(gl->program, "Texture");
    gl->input_size = gl->get_uniform_location(gl->program, "InputSize");
    gl->texture_size = gl->get_uniform_location(gl->program, "TextureSize");
    gl->output_size = gl->get_uniform_location(gl->program, "OutputSize");
    gl->frame_count = gl->get_uniform_location(gl->program, "FrameCount");
    gl->frame_direction =
        gl->get_uniform_location(gl->program, "FrameDirection");
    gl->parameters = std::move(source.parameters);
    for (const PostShaderParameter& parameter : gl->parameters)
    {
        gl->parameter_locations.push_back(
            gl->get_uniform_location(gl->program, parameter.name.c_str()));
    }
    gl->frames = 0;
    std::fprintf(stderr, "[repiu-post] shader: %s (%zu parameters)\n",
                 gl->active_id.c_str(), gl->parameters.size());
    return true;
}

bool GlidePostProcess::Reload()
{
    Implementation* gl = implementation_.get();
    if (!gl->initialized)
    {
        return false;
    }
    const std::string id = gl->active_id;
    gl->catalog = ListPostShaders(gl->directory);
    return Select(id);
}

void GlidePostProcess::Apply(std::uint32_t x, std::uint32_t y,
                             std::uint32_t drawable_width,
                             std::uint32_t drawable_height,
                             std::uint32_t logical_width,
                             std::uint32_t logical_height)
{
    Implementation* gl = implementation_.get();
    if (gl->program == 0U || drawable_width == 0U || drawable_height == 0U)
    {
        return;
    }
    if (logical_width == 0U || logical_height == 0U)
    {
        logical_width = drawable_width;
        logical_height = drawable_height;
    }
    const auto width = static_cast<GLsizei>(drawable_width);
    const auto height = static_cast<GLsizei>(drawable_height);

    // Glide state outlives the swap, so everything the pass changes is put
    // back: the attribute stack covers enables, masks, viewport, draw and read
    // buffers, and every unit's texture binding; the program is separate.
    GLint previous_program = 0;
    glGetIntegerv(kGlCurrentProgram, &previous_program);
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    gl->active_texture(kGlTexture0);
    if (gl->scene_texture == 0U)
    {
        GLuint name = 0;
        glGenTextures(1, &name);
        gl->scene_texture = name;
    }
    glBindTexture(GL_TEXTURE_2D, gl->scene_texture);
    if (gl->scene_width != drawable_width ||
        gl->scene_height != drawable_height)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, kGlRgba8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
        gl->scene_width = drawable_width;
        gl->scene_height = drawable_height;
    }
    glReadBuffer(GL_BACK);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLint>(x),
                        static_cast<GLint>(y), width, height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FOG);
    glDrawBuffer(GL_BACK);
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y), width, height);
    // Alpha is left as the game wrote it, so no translucency reaches a
    // compositor through the window's alpha channel.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    gl->use_program(gl->program);
    if (gl->mvp_matrix >= 0)
    {
        gl->uniform_matrix_4fv(gl->mvp_matrix, 1, GL_FALSE,
                               kUnitSquareProjection);
    }
    if (gl->texture >= 0)
    {
        gl->uniform_1i(gl->texture, 0);
    }
    if (gl->input_size >= 0)
    {
        gl->uniform_2f(gl->input_size, static_cast<GLfloat>(logical_width),
                       static_cast<GLfloat>(logical_height));
    }
    if (gl->texture_size >= 0)
    {
        gl->uniform_2f(gl->texture_size, static_cast<GLfloat>(logical_width),
                       static_cast<GLfloat>(logical_height));
    }
    if (gl->output_size >= 0)
    {
        gl->uniform_2f(gl->output_size, static_cast<GLfloat>(drawable_width),
                       static_cast<GLfloat>(drawable_height));
    }
    if (gl->frame_count >= 0)
    {
        gl->uniform_1i(gl->frame_count, static_cast<GLint>(gl->frames));
    }
    if (gl->frame_direction >= 0)
    {
        gl->uniform_1i(gl->frame_direction, 1);
    }
    for (std::size_t index = 0; index < gl->parameters.size(); ++index)
    {
        if (gl->parameter_locations[index] >= 0)
        {
            gl->uniform_1f(gl->parameter_locations[index],
                           gl->parameters[index].value);
        }
    }

    // A unit-square strip; texture row 0 is the bottom scanline, which is
    // where glCopyTexSubImage2D put the frame's bottom row.
    constexpr GLfloat kCorners[4][2] = {
        {0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}};
    glBegin(GL_TRIANGLE_STRIP);
    for (const auto& corner : kCorners)
    {
        glTexCoord4f(corner[0], corner[1], 0.0F, 1.0F);
        gl->vertex_attrib_4f(kTexCoordSlot, corner[0], corner[1], 0.0F, 1.0F);
        gl->vertex_attrib_4f(kColorSlot, 1.0F, 1.0F, 1.0F, 1.0F);
        gl->vertex_attrib_4f(kVertexCoordSlot, corner[0], corner[1], 0.0F,
                             1.0F);
    }
    glEnd();

    gl->use_program(static_cast<GLuint>(previous_program));
    glPopAttrib();
    ++gl->frames;
}

}  // namespace repiu::engine
