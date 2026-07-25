#ifndef REPIU_PLATFORM_WIN32_GLIDE_OPENGL_SHADER_H_
#define REPIU_PLATFORM_WIN32_GLIDE_OPENGL_SHADER_H_

#include "repiu/hle/glide_hle.h"

#include <memory>
#include <string>

namespace repiu::platform::win32
{

class GlideOpenGlShader
{
public:
    struct Implementation;

    GlideOpenGlShader();
    ~GlideOpenGlShader();

    GlideOpenGlShader(const GlideOpenGlShader&) = delete;
    GlideOpenGlShader& operator=(const GlideOpenGlShader&) = delete;

    bool Initialize();
    bool SetAlphaCombine(const hle::GlideAlphaCombineState& state);
    bool SetColorCombine(const hle::GlideColorCombineState& state);
    bool SetConstantColor(std::uint32_t argb);
    // R3: route the fragment output to the bound texture sample (SCALE_OTHER
    // color combine) when enabled, or the iterated vertex color otherwise.
    void SetTextureEnabled(bool enabled);
    void Shutdown();

    const std::string& message() const { return message_; }

private:
    std::unique_ptr<Implementation> implementation_;
    std::string message_;
};

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_GLIDE_OPENGL_SHADER_H_
