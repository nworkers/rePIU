#ifndef REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_
#define REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_

#include "repiu/hle/glide_hle.h"
#include "repiu/platform/win32/glide_opengl_shader.h"
#include "repiu/runtime/execution_backend.h"

#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace repiu::platform::win32
{

// Decoded Glide draw vertex in platform-neutral form: screen-space position,
// iterated color in [0,1], and TMU0 texture coordinates in texel space.
struct GlideDrawVertex
{
    float x = 0.0F;
    float y = 0.0F;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
    float s = 0.0F;
    float t = 0.0F;
};

class GlideOpenGlBackend
{
public:
    GlideOpenGlBackend() = default;
    ~GlideOpenGlBackend();

    GlideOpenGlBackend(const GlideOpenGlBackend&) = delete;
    GlideOpenGlBackend& operator=(const GlideOpenGlBackend&) = delete;

    // SDL video and OpenGL operations are owned by the executor main thread.
    // Guest-thread calls block until PumpHostCommands executes them there.
    void BindHostThread();
    void PumpHostCommands();
    void SetExecutionBackend(runtime::ExecutionBackend backend);

    // `origin` is the GrOriginLocation_t passed to grSstWinOpen:
    // GR_ORIGIN_UPPER_LEFT is 0 and GR_ORIGIN_LOWER_LEFT is 1. It selects the
    // orthographic projection, so getting it backwards flips the screen.
    bool OpenWindowed(std::uint32_t logical_width,
                      std::uint32_t logical_height,
                      std::uint32_t color_buffer_count,
                      std::uint32_t auxiliary_buffer_count,
                      std::uint32_t origin);
    void PumpEvents();
    bool BufferClear(std::uint32_t color, std::uint32_t alpha, std::uint32_t depth);
    bool BufferSwap(std::uint32_t swap_interval);
    bool DrawTriangle(const GlideDrawVertex& a,
                      const GlideDrawVertex& b,
                      const GlideDrawVertex& c);
    // Decode a Glide texture download into an OpenGL texture keyed by its TMU
    // start address (R3). format/large_lod/aspect follow the observed
    // GrTexInfo; source is guest texel data of source_size bytes.
    bool StoreTexture(std::uint32_t start_address,
                      std::uint32_t format,
                      std::uint32_t large_lod,
                      std::uint32_t aspect_ratio,
                      const std::uint8_t* source,
                      std::size_t source_size,
                      const std::uint8_t* palette_rgba8 = nullptr);
    // Select the current texture for subsequent draws (grTexSource).
    bool SourceTexture(std::uint32_t start_address);
    // Set TMU texture wrapping (clamp) and filtering.
    void SetTextureClampMode(std::uint32_t s_clamp, std::uint32_t t_clamp);
    void SetTextureFilterMode(std::uint32_t min_filter, std::uint32_t mag_filter);
    // R4 LFB: upload an RGBA8 image and blit it over the whole render target.
    // Used by grLfbUnlock to present what the guest wrote through the staging
    // surface. `flip_v` mirrors vertically for GR_ORIGIN_LOWER_LEFT locks. The
    // blit isolates its own GL state so it does not inherit the geometry state.
    bool PresentLfbSurface(const std::uint8_t* rgba8,
                           std::uint32_t width,
                           std::uint32_t height,
                           bool flip_v,
                           bool present_to_front);
    // R4 LFB: read the current render target back as RGBA8 (read locks).
    bool ReadbackFramebuffer(std::uint32_t width,
                             std::uint32_t height,
                             std::vector<std::uint8_t>* rgba8);
    // Enable/disable texture-driven color output for SCALE_OTHER color combine.
    void SetTextureCombineEnabled(bool enabled);
    bool SetColorMask(bool rgb, bool alpha);
    bool SetRenderBuffer(std::uint32_t buffer);
    bool SetDepthMask(bool enabled);
    bool SetDepthBufferMode(std::uint32_t mode);
    bool SetConstantColor(std::uint32_t argb);
    bool SetAlphaCombine(const hle::GlideAlphaCombineState& state);
    bool SetColorCombine(const hle::GlideColorCombineState& state);
    bool SetAlphaBlend(const hle::GlideAlphaBlendState& state);
    bool SetAlphaTestFunction(std::uint32_t function);
    bool SetAlphaTestReferenceValue(std::uint32_t reference_value);
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
    bool exit_requested() const { return exit_requested_; }
    bool is_texture_combine_enabled() const { return texture_combine_enabled_; }

    // Task 332 draw-census accessors. Read-only; they exist so the boundary can
    // report what a draw actually had bound without reaching into GL state.
    bool has_current_texture() const { return current_texture_ != nullptr; }
    std::uint32_t current_texture_address() const
    {
        return current_texture_ != nullptr ? current_texture_address_ : 0U;
    }
    std::uint32_t current_texture_width() const
    {
        return current_texture_ != nullptr ? current_texture_->width : 0U;
    }
    std::uint32_t current_texture_height() const
    {
        return current_texture_ != nullptr ? current_texture_->height : 0U;
    }
    std::uint32_t missing_texture_source_count() const
    {
        return missing_texture_source_count_;
    }
    std::uint32_t last_missing_texture_address() const
    {
        return last_missing_texture_address_;
    }
    std::uint32_t stored_texture_count() const
    {
        return static_cast<std::uint32_t>(textures_.size());
    }
    const std::string& message() const { return message_; }

private:
    struct TextureEntry
    {
        std::uint32_t gl_name = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        // Task 332: the texture-coordinate extent is not the pixel size. Glide
        // spans 256 along the longer axis whatever the LOD, so a 32x32 map is
        // still addressed 0..256 and normalizing by 32 shrinks it eightfold.
        std::uint32_t s_extent = 256;
        std::uint32_t t_extent = 256;
    };

    bool IsHostThread() const;
    void InvokeOnHostThread(std::function<void()> command);
    bool ApplyWindowScale(std::uint32_t scale);
    void ApplyDrawableViewport();
    std::string BuildWindowTitle(double frames_per_second) const;
    void ResetFrameRateMeasurement();
    void RecordPresentedFrame();

    std::thread::id host_thread_id_;
    std::mutex host_command_mutex_;
    std::condition_variable host_command_cv_;
    std::function<void()> host_command_;
    std::exception_ptr host_command_exception_;
    bool host_command_pending_ = false;
    bool host_command_complete_ = false;

    void* window_ = nullptr;
    void* render_context_ = nullptr;

    // TMU state
    std::uint32_t tmu_s_clamp_ = 0; // GR_TEXTURECLAMP_WRAP
    std::uint32_t tmu_t_clamp_ = 0; // GR_TEXTURECLAMP_WRAP
    std::uint32_t tmu_min_filter_ = 0; // GR_TEXTUREFILTER_POINT_SAMPLED
    std::uint32_t tmu_mag_filter_ = 0; // GR_TEXTUREFILTER_POINT_SAMPLED
    std::uint32_t logical_width_ = 0;
    std::uint32_t logical_height_ = 0;
    std::uint32_t window_scale_ = 2U;
    runtime::ExecutionBackend execution_backend_ =
        runtime::ExecutionBackend::kLegacy;
    std::chrono::steady_clock::time_point frame_rate_period_start_;
    std::uint64_t frame_rate_frame_count_ = 0;

    std::uint32_t alpha_test_function_ = 7U; // GR_CMP_ALWAYS
    float alpha_test_reference_ = 0.0f;
    bool exit_requested_ = false;
    // True when grSstWinOpen asked for GR_ORIGIN_LOWER_LEFT, ie. guest y grows
    // upward and the projection matches OpenGL's default orientation.
    bool origin_lower_left_ = false;
    GlideOpenGlShader shader_;
    std::string message_;
    bool dummy_mode_ = false;
    std::unordered_map<std::uint32_t, TextureEntry> textures_;
    const TextureEntry* current_texture_ = nullptr;
    // Task 332 diagnostics. A draw census has to separate "this quad was drawn
    // with the right texture", "with no texture at all", and "the game sourced
    // an address that was never downloaded", which the bound pointer alone
    // cannot express.
    std::uint32_t current_texture_address_ = 0;
    std::uint32_t missing_texture_source_count_ = 0;
    std::uint32_t last_missing_texture_address_ = 0;
    bool texture_combine_enabled_ = false;
    // Dedicated texture reused by every LFB blit so unlock does not churn GL
    // texture names once per frame.
    std::uint32_t lfb_texture_ = 0;
};

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_
