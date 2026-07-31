#ifndef REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_
#define REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_

#include "repiu/hle/glide_hle.h"
#include "repiu/hle/glide_vertex.h"
#include "repiu/platform/win32/glide_buffer_swap_timing.h"
#include "repiu/platform/win32/glide_gate_timing.h"
#include "repiu/platform/win32/glide_gl_error_policy.h"
#include "repiu/platform/win32/glide_ordinal_timing.h"
#include "repiu/platform/win32/glide_opengl_shader.h"
#include "repiu/platform/win32/glide_setter_phase_timing.h"
#include "repiu/platform/win32/glide_swap_interval_policy.h"
#include "repiu/platform/win32/glide_texture_census.h"
#include "repiu/runtime/execution_backend.h"

#include <chrono>
#include <cstddef>
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

enum class GlideOpenGlCullFace : std::uint8_t
{
    kDisabled,
    kFront,
    kBack,
};

bool TranslateGlideOpenGlCullMode(std::uint32_t mode,
                                  bool origin_lower_left,
                                  GlideOpenGlCullFace* face);

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
    bool DrawLine(const hle::GlideDrawVertex& a,
                  const hle::GlideDrawVertex& b);
    bool DrawTriangle(const hle::GlideDrawVertex& a,
                      const hle::GlideDrawVertex& b,
                      const hle::GlideDrawVertex& c);
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
    bool SetFogColor(std::uint32_t argb);
    bool SetFogTable(const hle::GlideFogTable& table);
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

    // Task 333: the host-thread rendezvous split into waiting and work. Off
    // unless REPIU_EXECUTION_TIME_PROFILE is set, so the normal path pays one
    // branch. Read from the exit summary after the guest thread has stopped.
    Win32GlideGateTimingSnapshot glide_gate_timing() const
    {
        return SnapshotGlideGateTiming(glide_gate_timing_);
    }

    Win32GlideBufferSwapTimingSnapshot glide_buffer_swap_timing() const
    {
        return SnapshotGlideBufferSwapTiming(glide_buffer_swap_timing_);
    }

    // Task 364: the OpenGL interval of the two leading state setters, split
    // into error drain, state application, and trailing error check. Off
    // unless REPIU_GLIDE_SETTER_PHASE is set.
    Win32GlideSetterPhaseSnapshot glide_setter_phase_timing() const
    {
        return SnapshotGlideSetterPhaseTiming(glide_setter_phase_timing_);
    }

    // Task 369: whether the per-call setter error check ran, plus the result of
    // the once-per-frame check that replaced it. Reported unconditionally: a
    // silent policy that suppresses error reporting has to say so in the
    // summary, or a later run cannot tell a clean frame from an unchecked one.
    Win32GlideGlErrorPolicySnapshot glide_gl_error_policy() const
    {
        // The free accessor rather than the cached member: a run that never
        // reached a setter has not resolved the member yet, and reporting the
        // policy as off in that case would misdescribe the run.
        return SnapshotGlideGlErrorPolicy(glide_gl_error_policy_,
                                          GlideGlErrorCheckPolicyEnabled(),
                                          glide_gl_error_frame_interval_);
    }

    // Task 375: texture upload attributes, including the uploads that failed to
    // decode. Always collected -- one hash per upload on a path that sees a
    // couple of uploads per second is not a hot path.
    Win32GlideTextureCensusSnapshot glide_texture_census() const
    {
        return SnapshotGlideTextureCensus(glide_texture_census_);
    }

    // Task 371: what the swap interval override asked for and what the driver
    // actually reported back afterwards.
    Win32GlideSwapIntervalPolicySnapshot glide_swap_interval_policy() const
    {
        return glide_swap_interval_policy_;
    }

    // Task 370: called from the GL debug trampoline, which is a free function in
    // the source file because it needs the driver's calling convention and GL
    // types this header does not pull in. Records only -- see the recorder's
    // allocation-free contract.
    void RecordGlDebugMessage(std::uint32_t id,
                              bool is_error,
                              const char* message,
                              std::size_t length)
    {
        RecordGlideGlDebugMessage(
            &glide_gl_error_policy_, id, is_error, message, length);
    }

    void BeginGlideOrdinalTiming(Win32GlideOrdinalTimingProfile* profile,
                                 std::uint16_t ordinal)
    {
        active_ordinal_timing_ = profile;
        active_ordinal_ = ordinal;
    }

    void EndGlideOrdinalTiming()
    {
        active_ordinal_timing_ = nullptr;
        active_ordinal_ = 0;
    }

    // Waits up to `timeout_milliseconds` for a guest command and executes it,
    // returning whether one ran. Host thread only. Task 333 replaced the poll
    // loop's unconditional sleep with this, so a posted command no longer waits
    // out the sleep before being seen.
    bool WaitAndPumpHostCommands(std::uint32_t timeout_milliseconds);

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
    bool BufferSwapOnHostThread(std::uint32_t swap_interval,
                                bool guest_gate_command);
    bool DrawPrimitive(const hle::GlideDrawVertex* const* vertices,
                       std::size_t vertex_count,
                       std::uint32_t primitive,
                       const char* success_message);
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
    // Task 333. Guarded by `host_command_mutex_` except for the guest's own
    // enter timestamp, which is thread-local to the call.
    Win32GlideGateTimingProfile glide_gate_timing_;
    // Task 354: grBufferSwap host work split. Only guest-gate commands are
    // recorded; internal LFB presentation through BufferSwap remains separate.
    Win32GlideBufferSwapTimingProfile glide_buffer_swap_timing_;
    // Task 364: host thread only, so it needs no lock — every writer runs
    // inside a host-thread setter body.
    Win32GlideSetterPhaseProfile glide_setter_phase_timing_;
    // Task 369: host thread only, like the phase profile above — the frame
    // check runs inside `BufferSwapOnHostThread`.
    Win32GlideGlErrorPolicyProfile glide_gl_error_policy_;
    // Task 371: written once during window creation, read at teardown.
    Win32GlideSwapIntervalPolicySnapshot glide_swap_interval_policy_;
    // Task 375: host thread only, written inside StoreTexture.
    Win32GlideTextureCensus glide_texture_census_;
    Win32GlideOrdinalTimingProfile* active_ordinal_timing_ = nullptr;
    std::uint16_t active_ordinal_ = 0;
    bool glide_gate_timing_enabled_ = false;
    bool glide_gate_timing_resolved_ = false;
    bool glide_setter_phase_enabled_ = false;
    bool glide_setter_phase_resolved_ = false;
    bool glide_gl_error_check_enabled_ = false;
    bool glide_gl_error_check_resolved_ = false;
    bool GlideGateTimingEnabled();
    // Resolved once, like the gate timing gate above: the setters are a hot
    // path and `getenv` is not.
    bool GlideSetterPhaseEnabled();
    // Task 370: zero once the debug callback is installed, otherwise the sampled
    // fallback period. Task 369 ran this check every frame and that single call
    // was 10.71% of wall.
    std::uint32_t glide_gl_error_frame_interval_ = 0;
    std::uint32_t glide_gl_error_frame_counter_ = 0;
    // Installs GL_KHR_debug asynchronous reporting. Returns false when the
    // driver does not expose it, which selects the sampled fallback.
    bool InstallGlDebugOutput();
    void RunGlErrorFrameCheck();
    // Task 369. Same resolve-once shape, for the same reason.
    bool GlideGlErrorCheckEnabled();
    // Returns true when the setter may report success. With the policy off it
    // never calls `glGetError`, which is the entire point: the call is a
    // pipeline flush, so skipping the branch is not enough — the call itself
    // has to not happen.
    bool CheckGlErrorIfEnabled();
};

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_GLIDE_OPENGL_BACKEND_H_
