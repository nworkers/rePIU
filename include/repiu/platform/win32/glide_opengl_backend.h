#ifndef REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_
#define REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_

#include "repiu/hle/glide_hle.h"
#include "repiu/platform/win32/glide_opengl_shader.h"

#include <cstdint>
#include <string>

namespace repiu::platform::win32
{

class GlideOpenGlBackend
{
public:
    GlideOpenGlBackend() = default;
    ~GlideOpenGlBackend();

    GlideOpenGlBackend(const GlideOpenGlBackend&) = delete;
    GlideOpenGlBackend& operator=(const GlideOpenGlBackend&) = delete;

    bool OpenWindowed(std::uint32_t logical_width,
                      std::uint32_t logical_height,
                      std::uint32_t color_buffer_count,
                      std::uint32_t auxiliary_buffer_count);
    void PumpEvents();
    bool BufferClear(std::uint32_t color, std::uint32_t alpha, std::uint32_t depth);
    bool BufferSwap(std::uint32_t swap_interval);
    bool SetColorMask(bool rgb, bool alpha);
    bool SetRenderBuffer(std::uint32_t buffer);
    bool SetDepthMask(bool enabled);
    bool SetDepthBufferMode(std::uint32_t mode);
    bool SetAlphaCombine(const hle::GlideAlphaCombineState& state);
    bool SetColorCombine(const hle::GlideColorCombineState& state);
    bool SetAlphaBlend(const hle::GlideAlphaBlendState& state);
    bool SetAlphaTestFunction(std::uint32_t function);
    bool SetDepthBufferFunction(std::uint32_t function);
    bool SetFogMode(std::uint32_t mode);
    bool SetClipWindow(std::uint32_t min_x,
                       std::uint32_t min_y,
                       std::uint32_t max_x,
                       std::uint32_t max_y);
    bool SetCullMode(std::uint32_t mode);
    bool SetDitherMode(std::uint32_t mode);
    void Close();

    bool is_open() const { return window_ != nullptr || dummy_mode_; }
    bool is_dummy() const { return dummy_mode_; }
    const std::string& message() const { return message_; }

private:
    void* window_ = nullptr;
    void* device_context_ = nullptr;
    void* render_context_ = nullptr;
    std::uint32_t logical_width_ = 0;
    std::uint32_t logical_height_ = 0;
    GlideOpenGlShader shader_;
    std::string message_;
    bool dummy_mode_ = false;
};

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_
