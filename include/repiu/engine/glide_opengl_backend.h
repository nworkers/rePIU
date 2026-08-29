#ifndef REPIU_ENGINE_GLIDE_OPENGL_BACKEND_H_
#define REPIU_ENGINE_GLIDE_OPENGL_BACKEND_H_

#include "repiu/hle/glide_hle.h"
#include "repiu/hle/glide_vertex.h"
#include "repiu/engine/glide_buffer_swap_timing.h"
#include "repiu/engine/glide_async_present.h"
#include "repiu/engine/glide_draw_batch.h"
#include "repiu/engine/glide_gate_timing.h"
#include "repiu/engine/glide_gl_error_policy.h"
#include "repiu/engine/glide_ordinal_timing.h"
#include "repiu/engine/glide_opengl_shader.h"
#include "repiu/engine/glide_setter_phase_timing.h"
#include "repiu/engine/glide_swap_interval_policy.h"
#include "repiu/engine/glide_texture_census.h"
#include "repiu/runtime/execution_backend.h"

#include <atomic>
#include <memory>
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

namespace repiu::engine
{

class JammaInputTimeline;

}  // namespace repiu::engine

namespace repiu::hle
{
class BiosKeyboard;
}

namespace repiu::engine
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

float CalculateGlidePointSize(std::uint32_t logical_width,
                              std::uint32_t logical_height,
                              std::uint32_t drawable_width,
                              std::uint32_t drawable_height);

class GlideOpenGlBackend
{
public:
    // Both defined in the source file: the asynchronous state is an incomplete
    // type here, and a `unique_ptr` to it needs the complete type wherever the
    // constructor or destructor is instantiated.
    GlideOpenGlBackend();
    ~GlideOpenGlBackend();

    GlideOpenGlBackend(const GlideOpenGlBackend&) = delete;
    GlideOpenGlBackend& operator=(const GlideOpenGlBackend&) = delete;

    // SDL video and OpenGL operations are owned by the executor main thread.
    // Guest-thread calls block until PumpHostCommands executes them there.
    void BindHostThread();
    void PumpHostCommands();
    void SetExecutionBackend(runtime::ExecutionBackend backend);
    void SetJammaInputTimeline(JammaInputTimeline* timeline);
    void SetBiosKeyboard(hle::BiosKeyboard* keyboard);
    std::uint64_t EventClockNanoseconds() const;

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
    // Task 440: the same three operations posted to the host instead of waited
    // on. With vsync enabled the present blocks for 10.8 ms -- 32.8% of
    // guest-run -- while Glide's own protocol says a swap is asynchronous and
    // the game polls `grBufferNumPending` to throttle. These return true when
    // the command was accepted for execution, not when it has executed; a
    // backend failure afterwards is counted rather than reported here.
    bool PostBufferSwap(std::uint32_t swap_interval);
    bool PostBufferClear(std::uint32_t color, std::uint32_t alpha,
                         std::uint32_t depth);
    bool PostDrawPrimitiveBatch(const hle::GlideDrawVertex* vertices,
                                std::size_t vertex_count,
                                GlideBatchPrimitive primitive);
    // Outstanding swaps, which is what `grBufferNumPending` must answer. Zero
    // whenever the asynchronous path is off, matching today's behaviour.
    // Appends to the asynchronous FIFO and returns without waiting.
    // `swap_command` marks the one command kind the outstanding bound applies
    // to. Public because the ordering it provides is a property worth asserting
    // directly rather than only through the gates that use it.
    bool PostToHostThread(std::function<void()> command, bool swap_command);
    std::uint32_t glide_pending_swap_count() const;
    GlideAsyncPresentSnapshot glide_async_present() const;
    // Task 509: how fast this run actually drew, in a form the shutdown path
    // can read.
    //
    // The frame rate has always been computed -- once a second, into the window
    // title -- but a run that left only a log kept no trace of it, and on Linux
    // there is no window-title reader to scrape. These two are what the
    // `[repiu-shutdown]` line reports instead.
    //
    // `presented_frame_span_milliseconds` runs from the **first** presented
    // frame, not from process start. `pumpit1` spends roughly its first
    // forty-five seconds decoding start-up assets with nothing on screen, so a
    // rate taken over the whole budget understates the real one several times
    // over.
    //
    // **Host thread only.** Both counters are written in `RecordPresentedFrame`,
    // which runs on the thread that performs the swap -- the same thread that
    // runs the teardown block that reads them. That is why no lock is involved,
    // and why a caller on any other thread would need one.
    std::uint64_t presented_frame_total() const;
    std::uint64_t presented_frame_span_milliseconds() const;
    // Task 420: the remaining Glide draw entry points. A point is one vertex
    // with `GL_POINTS`; a polygon is a convex fan, which is what Glide's
    // `grDrawPolygon` contract guarantees.
    bool DrawPoint(const hle::GlideDrawVertex& a);
    bool DrawPolygon(const hle::GlideDrawVertex* vertices, std::size_t count);
    bool DrawLine(const hle::GlideDrawVertex& a,
                  const hle::GlideDrawVertex& b);
    bool DrawTriangle(const hle::GlideDrawVertex& a,
                      const hle::GlideDrawVertex& b,
                      const hle::GlideDrawVertex& c);
    // Task 438: one rendezvous for many primitives of the same kind. Valid only
    // for independent primitives -- `GL_TRIANGLES`, `GL_LINES`, `GL_POINTS` --
    // because those concatenate inside a single `glBegin`/`glEnd` while a
    // `GL_TRIANGLE_FAN` does not. The caller guarantees that no render state
    // changed between the queued primitives, which is what makes drawing them
    // together identical to drawing them one at a time.
    bool DrawPrimitiveBatch(const hle::GlideDrawVertex* vertices,
                            std::size_t vertex_count,
                            GlideBatchPrimitive primitive);
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
    // Glide stores palette indices separately from the palette. Register a new
    // palette generation; stale P_8/AP_88 sources are refreshed on first use.
    bool RefreshPalettizedTextures(const std::uint8_t* palette_rgba8);
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

    // Task 431: true while the guest thread is inside the host rendezvous and
    // therefore running no guest code at all. The AOT timer safe points are
    // INT3 bytes in the code cache, so none of them is reachable in this
    // window and any tick that comes due is coalesced away. Read from the poll
    // thread to attribute those losses.
    // See docs/design/20260806-431-tick-injection-opportunity.md.
    bool guest_in_glide_gate() const
    {
        return guest_in_glide_gate_.load(std::memory_order_relaxed);
    }

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
    GlideGateTimingSnapshot glide_gate_timing() const
    {
        return SnapshotGlideGateTiming(glide_gate_timing_);
    }

    GlideBufferSwapTimingSnapshot glide_buffer_swap_timing() const
    {
        return SnapshotGlideBufferSwapTiming(glide_buffer_swap_timing_);
    }

    // Task 419: how often a spin resolved the rendezvous before the condition
    // variable had to. Read from the exit summary like the timing above.
    GlideRendezvousSpinSnapshot rendezvous_spin_counts() const
    {
        return GlideRendezvousSpinSnapshot{
            rendezvous_spin_guest_hit_, rendezvous_spin_guest_miss_,
            rendezvous_spin_host_hit_, rendezvous_spin_host_miss_,
            rendezvous_spin_microseconds_};
    }

    // Task 364: the OpenGL interval of the two leading state setters, split
    // into error drain, state application, and trailing error check. Off
    // unless REPIU_GLIDE_SETTER_PHASE is set.
    GlideSetterPhaseSnapshot glide_setter_phase_timing() const
    {
        return SnapshotGlideSetterPhaseTiming(glide_setter_phase_timing_);
    }

    // Task 369: whether the per-call setter error check ran, plus the result of
    // the once-per-frame check that replaced it. Reported unconditionally: a
    // silent policy that suppresses error reporting has to say so in the
    // summary, or a later run cannot tell a clean frame from an unchecked one.
    GlideGlErrorPolicySnapshot glide_gl_error_policy() const
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
    GlideTextureCensusSnapshot glide_texture_census() const
    {
        return SnapshotGlideTextureCensus(glide_texture_census_);
    }

    // Task 371: what the swap interval override asked for and what the driver
    // actually reported back afterwards.
    GlideSwapIntervalPolicySnapshot glide_swap_interval_policy() const
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

    void BeginGlideOrdinalTiming(GlideOrdinalTimingProfile* profile,
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
        std::uint32_t format = 0U;
        std::vector<std::uint8_t> source;
        std::uint64_t palette_generation = 0U;
    };

    bool IsHostThread() const;
    void InvokeOnHostThread(std::function<void()> command);
    bool BufferSwapOnHostThread(std::uint32_t swap_interval,
                                bool guest_gate_command);
    bool DrawPrimitive(const hle::GlideDrawVertex* const* vertices,
                       std::size_t vertex_count,
                       std::uint32_t primitive,
                       const char* success_message);
    // The state and per-vertex halves of a draw, split so the single-primitive
    // and batched paths cannot drift apart in what they emit.
    bool PrepareDrawState(std::uint32_t primitive,
                          bool* sample_texture,
                          float* inverse_width,
                          float* inverse_height);
    bool RefreshCurrentPalettizedTexture();
    void EmitDrawVertex(const hle::GlideDrawVertex& vertex,
                        bool sample_texture,
                        float inverse_width,
                        float inverse_height);
    bool ApplyWindowScale(std::uint32_t scale);
    void ApplyDrawableViewport();
    std::string BuildWindowTitle(double frames_per_second) const;
    void ResetFrameRateMeasurement();
    void RecordPresentedFrame();

    std::thread::id host_thread_id_;
    JammaInputTimeline* jamma_input_timeline_ = nullptr;
    hle::BiosKeyboard* bios_keyboard_ = nullptr;
    // Mutable so the async snapshot accessors can stay const: they only read
    // counters this lock protects.
    mutable std::mutex host_command_mutex_;
    std::condition_variable host_command_cv_;
    std::function<void()> host_command_;
    std::exception_ptr host_command_exception_;
    bool host_command_pending_ = false;
    bool host_command_complete_ = false;

    // Task 419. Lock-free mirrors of the two flags above, published at the same
    // points under the same mutex, so a spinning thread can read them without
    // taking the lock. They are **hints only**: every spin that observes one
    // still acquires the mutex and re-tests the original predicate, which is
    // what keeps the condition-variable protocol free of lost wakeups.
    // See docs/design/20260805-419-glide-rendezvous-spin-wait.md.
    std::atomic<bool> host_command_pending_hint_{false};
    std::atomic<bool> host_command_complete_hint_{false};

    // Task 431. Set only on the guest-thread path of InvokeOnHostThread. A
    // ratio is what the measurement needs, not an exact boundary, so relaxed
    // ordering is enough: one misattributed tick out of thousands cannot move
    // the verdict.
    std::atomic<bool> guest_in_glide_gate_{false};

    // Task 440: the asynchronous FIFO. Guarded by `host_command_mutex_` so it
    // shares one lock and one condition variable with the synchronous slot,
    // which is what makes "drain the queue, then run the sync command" a single
    // ordered decision rather than two racing ones.
    // One pointer, not the queue and counters themselves: `ThreadContext` holds
    // this backend and is a stack local, and the inline members overflowed that
    // stack at window creation.
    std::unique_ptr<GlideAsyncPresentState> async_present_state_;
    // Read by the guest thread every frame through `grBufferNumPending`, which
    // runs on the guest's own small stack. Kept as a direct atomic so that read
    // touches no lazily-created state, no function-local static and no
    // allocation -- all of which are CRT work this stack must not carry.
    std::atomic<std::uint32_t> pending_swap_count_{0};
    // Runs posted commands only, never the synchronous slot, so a command may
    // call it without re-entering itself.
    void DrainAsyncCommands();
    GlideAsyncPresentState& async_present();
    const GlideAsyncPresentState& async_present() const;
    // Atomic, and deliberately never guarded by `host_command_mutex_`. The
    // timeout path terminates the guest thread, which takes that mutex on every
    // Glide gate; acquiring a mutex a killed thread still owns faults. Teardown
    // therefore touches this flag and nothing else.
    std::atomic<bool> host_stopped_pumping_{false};
    // Current queue depth as an atomic, so the snapshot never needs the lock
    // either.
    std::atomic<std::uint32_t> queued_command_count_{0};

    // Spin budget in microseconds, resolved once. Zero restores the pure
    // condition-variable wait.
    std::uint32_t RendezvousSpinMicroseconds();
    // Spins until `hint` reads `expected` or the budget runs out. Returns true
    // when the hint was observed, which is a hint, never a decision.
    bool SpinForRendezvousHint(const std::atomic<bool>& hint, bool expected,
                               bool guest_side);
    std::uint32_t rendezvous_spin_microseconds_ = 0;
    bool rendezvous_spin_resolved_ = false;
    std::uint64_t rendezvous_spin_guest_hit_ = 0;
    std::uint64_t rendezvous_spin_guest_miss_ = 0;
    std::uint64_t rendezvous_spin_host_hit_ = 0;
    std::uint64_t rendezvous_spin_host_miss_ = 0;

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
    float point_size_ = 1.0F;
    runtime::ExecutionBackend execution_backend_ =
        runtime::ExecutionBackend::kLegacy;
    std::chrono::steady_clock::time_point frame_rate_period_start_;
    std::uint64_t frame_rate_frame_count_ = 0;
    // Task 509. Deliberately separate from the two above: those measure one
    // second at a time and reset, which is what a window title wants and what a
    // total must not do.
    std::chrono::steady_clock::time_point presented_frame_first_;
    std::uint64_t presented_frame_total_ = 0;
    // Read once, where the backend initialises, rather than per frame.
    bool log_frame_rate_ = false;

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
    TextureEntry* current_texture_ = nullptr;
    // Task 332 diagnostics. A draw census has to separate "this quad was drawn
    // with the right texture", "with no texture at all", and "the game sourced
    // an address that was never downloaded", which the bound pointer alone
    // cannot express.
    std::uint32_t current_texture_address_ = 0;
    std::uint32_t missing_texture_source_count_ = 0;
    std::uint32_t last_missing_texture_address_ = 0;
    bool texture_combine_enabled_ = false;
    std::array<std::uint8_t, 1024> palette_rgba8_ = {};
    bool palette_valid_ = false;
    std::uint64_t palette_generation_ = 0U;
    // Dedicated texture reused by every LFB blit so unlock does not churn GL
    // texture names once per frame.
    std::uint32_t lfb_texture_ = 0;
    // Task 333. Guarded by `host_command_mutex_` except for the guest's own
    // enter timestamp, which is thread-local to the call.
    GlideGateTimingProfile glide_gate_timing_;
    // Task 354: grBufferSwap host work split. Only guest-gate commands are
    // recorded; internal LFB presentation through BufferSwap remains separate.
    GlideBufferSwapTimingProfile glide_buffer_swap_timing_;
    // Task 364: host thread only, so it needs no lock — every writer runs
    // inside a host-thread setter body.
    GlideSetterPhaseProfile glide_setter_phase_timing_;
    // Task 369: host thread only, like the phase profile above — the frame
    // check runs inside `BufferSwapOnHostThread`.
    GlideGlErrorPolicyProfile glide_gl_error_policy_;
    // Task 371: written once during window creation, read at teardown.
    GlideSwapIntervalPolicySnapshot glide_swap_interval_policy_;
    // Task 375: host thread only, written inside StoreTexture.
    GlideTextureCensus glide_texture_census_;
    GlideOrdinalTimingProfile* active_ordinal_timing_ = nullptr;
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

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GLIDE_OPENGL_BACKEND_H_
