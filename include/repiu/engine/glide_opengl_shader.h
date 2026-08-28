#ifndef REPIU_PLATFORM_WIN32_GLIDE_OPENGL_SHADER_H_
#define REPIU_PLATFORM_WIN32_GLIDE_OPENGL_SHADER_H_

#include "repiu/hle/glide_hle.h"

#include <memory>
#include <string>

namespace repiu::engine
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
    bool SetFogMode(std::uint32_t mode);
    bool SetFogColor(std::uint32_t argb);
    bool SetFogTable(const hle::GlideFogTable& table);
    // R3: route the fragment output to the bound texture sample (SCALE_OTHER
    // color combine) when enabled, or the iterated vertex color otherwise.
    void SetTextureEnabled(bool enabled);
    // Bypass guest combine/fog equations while copying the CPU LFB texture.
    void SetBlitMode(bool enabled);
    void Shutdown();

    const std::string& message() const { return message_; }

private:
    std::unique_ptr<Implementation> implementation_;
    std::string message_;
};

}  // namespace repiu::engine

#endif  // REPIU_PLATFORM_WIN32_GLIDE_OPENGL_SHADER_H_
