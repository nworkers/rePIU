#include "repiu/platform/win32/glide_opengl_backend.h"
#include "repiu/hle/glide_texture_decode.h"
#include "repiu/hle/glide_lfb.h"
#include "repiu/platform/win32/execution_time_profile.h"

#if defined(_WIN32)
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>

#if !defined(REPIU_VERSION)
#define REPIU_VERSION "unknown"
#endif

namespace repiu::platform::win32
{

GlideOpenGlBackend::~GlideOpenGlBackend()
{
    Close();
}

void GlideOpenGlBackend::BindHostThread()
{
    host_thread_id_ = std::this_thread::get_id();
}

void GlideOpenGlBackend::SetExecutionBackend(
    runtime::ExecutionBackend backend)
{
    execution_backend_ = backend;
}

bool GlideOpenGlBackend::IsHostThread() const
{
    return host_thread_id_ == std::thread::id{} ||
        host_thread_id_ == std::this_thread::get_id();
}

// Task 333: resolved once rather than per command, since the rendezvous is on
// the hot path and `getenv` is not.
bool GlideOpenGlBackend::GlideGateTimingEnabled()
{
    if (!glide_gate_timing_resolved_)
    {
        glide_gate_timing_enabled_ = ExecutionTimeProfileEnabled();
        glide_gate_timing_resolved_ = true;
    }
    return glide_gate_timing_enabled_;
}

void GlideOpenGlBackend::InvokeOnHostThread(std::function<void()> command)
{
    const bool timing =
        GlideGateTimingEnabled() || active_ordinal_timing_ != nullptr;
    if (IsHostThread())
    {
        const std::uint64_t start =
            timing ? ReadGlideGateTimingCycles() : 0U;
        command();
        if (timing)
        {
            const std::uint64_t finish = ReadGlideGateTimingCycles();
            const std::uint64_t cycles =
                GlideGateTimingDelta(&glide_gate_timing_, start, finish);
            RecordGlideGateDirectCommand(
                &glide_gate_timing_, cycles);
            RecordGlideOrdinalDirectWork(
                active_ordinal_timing_, active_ordinal_, cycles);
        }
        return;
    }

    const std::uint64_t enter = timing ? ReadGlideGateTimingCycles() : 0U;
    std::exception_ptr command_exception;
    std::unique_lock<std::mutex> lock(host_command_mutex_);
    host_command_cv_.wait(lock, [this]() { return !host_command_pending_; });
    host_command_ = std::move(command);
    host_command_exception_ = nullptr;
    host_command_complete_ = false;
    host_command_pending_ = true;
    if (timing)
    {
        // Published under the lock the host takes before reading it, so the
        // timestamp is visible to the host without an atomic.
        RecordGlideGatePublish(&glide_gate_timing_, enter,
                               ReadGlideGateTimingCycles());
    }
    host_command_cv_.notify_all();
    host_command_cv_.wait(lock, [this]() { return host_command_complete_; });
    if (timing)
    {
        const std::uint64_t resume = ReadGlideGateTimingCycles();
        RecordGlideGateResume(&glide_gate_timing_, enter, resume);
        RecordGlideOrdinalRendezvous(
            active_ordinal_timing_, active_ordinal_, enter,
            glide_gate_timing_.publish_cycles,
            glide_gate_timing_.host_start_cycles,
            glide_gate_timing_.host_finish_cycles, resume);
    }
    command_exception = host_command_exception_;
    lock.unlock();
    if (command_exception != nullptr)
    {
        std::rethrow_exception(command_exception);
    }
}

void GlideOpenGlBackend::PumpHostCommands()
{
    if (!IsHostThread())
    {
        return;
    }

    const bool timing = GlideGateTimingEnabled();
    std::function<void()> command;
    {
        std::lock_guard<std::mutex> lock(host_command_mutex_);
        if (!host_command_pending_)
        {
            return;
        }
        command = host_command_;
    }

    const std::uint64_t start = timing ? ReadGlideGateTimingCycles() : 0U;
    std::exception_ptr command_exception;
    try
    {
        command();
    }
    catch (...)
    {
        command_exception = std::current_exception();
    }

    {
        std::lock_guard<std::mutex> lock(host_command_mutex_);
        if (timing)
        {
            RecordGlideGateHostCommand(&glide_gate_timing_, start,
                                       ReadGlideGateTimingCycles());
        }
        host_command_ = {};
        host_command_exception_ = command_exception;
        host_command_pending_ = false;
        host_command_complete_ = true;
    }
    host_command_cv_.notify_all();
}

// Task 333: the host thread used to sleep unconditionally between polls, so a
// guest command published just after a pump waited out the whole sleep before
// anyone looked at it. Waiting on the same condition variable keeps the poll
// cadence (the timeout is the old sleep) while making publication wake the host
// immediately.
bool GlideOpenGlBackend::WaitAndPumpHostCommands(
    std::uint32_t timeout_milliseconds)
{
    if (!IsHostThread())
    {
        return false;
    }
    {
        std::unique_lock<std::mutex> lock(host_command_mutex_);
        if (!host_command_pending_)
        {
            host_command_cv_.wait_for(
                lock, std::chrono::milliseconds(timeout_milliseconds),
                [this]() { return host_command_pending_; });
        }
        if (!host_command_pending_)
        {
            return false;
        }
    }
    PumpHostCommands();
    return true;
}

bool GlideOpenGlBackend::ApplyWindowScale(std::uint32_t scale)
{
#if !defined(_WIN32)
    (void)scale;
    return false;
#else
    if (window_ == nullptr || scale < 1U || scale > 4U ||
        logical_width_ > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()) / scale ||
        logical_height_ > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()) / scale)
    {
        return false;
    }

    SDL_Window* window = static_cast<SDL_Window*>(window_);
    const int width = static_cast<int>(logical_width_ * scale);
    const int height = static_cast<int>(logical_height_ * scale);
    if (!SDL_SetWindowSize(window, width, height))
    {
        message_ = std::string("SDL3 window scale failed: ") +
            SDL_GetError();
        return false;
    }
    window_scale_ = scale;
    ApplyDrawableViewport();
    std::ostringstream stream;
    stream << "SDL3 window scale " << scale << "x (" << width << "x"
           << height << ")";
    message_ = stream.str();
    fprintf(stderr, "[repiu-live-debug] %s\n", message_.c_str());
    return true;
#endif
}

void GlideOpenGlBackend::ApplyDrawableViewport()
{
#if defined(_WIN32)
    if (window_ == nullptr || render_context_ == nullptr || dummy_mode_)
    {
        return;
    }
    int drawable_width = 0;
    int drawable_height = 0;
    if (!SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window_),
                                   &drawable_width, &drawable_height) ||
        drawable_width <= 0 || drawable_height <= 0)
    {
        return;
    }
    glViewport(0, 0, static_cast<GLsizei>(drawable_width),
               static_cast<GLsizei>(drawable_height));
    glScissor(0, 0, static_cast<GLsizei>(drawable_width),
              static_cast<GLsizei>(drawable_height));
#endif
}

std::string GlideOpenGlBackend::BuildWindowTitle(
    double frames_per_second) const
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "rePIU v" REPIU_VERSION " - Build " __DATE__
           << " - Glide 2 OpenGL ["
           << runtime::ExecutionBackendName(execution_backend_)
           << "] - FPS : " << std::fixed << std::setprecision(1)
           << frames_per_second;
    return stream.str();
}

void GlideOpenGlBackend::ResetFrameRateMeasurement()
{
    frame_rate_period_start_ = {};
    frame_rate_frame_count_ = 0;
}

void GlideOpenGlBackend::RecordPresentedFrame()
{
#if defined(_WIN32)
    const auto now = std::chrono::steady_clock::now();
    if (frame_rate_frame_count_ == 0)
    {
        frame_rate_period_start_ = now;
        frame_rate_frame_count_ = 1;
        return;
    }

    ++frame_rate_frame_count_;
    const double elapsed_seconds =
        std::chrono::duration<double>(now - frame_rate_period_start_).count();
    if (elapsed_seconds < 1.0)
    {
        return;
    }

    const double frames_per_second =
        static_cast<double>(frame_rate_frame_count_ - 1) / elapsed_seconds;
    const std::string window_title = BuildWindowTitle(frames_per_second);
    if (!SDL_SetWindowTitle(static_cast<SDL_Window*>(window_),
                            window_title.c_str()))
    {
        fprintf(stderr,
                "[repiu-live-debug] SDL3 window title update failed: %s\n",
                SDL_GetError());
    }
    frame_rate_period_start_ = now;
    frame_rate_frame_count_ = 1;
#endif
}

bool GlideOpenGlBackend::OpenWindowed(
    std::uint32_t logical_width,
    std::uint32_t logical_height,
    std::uint32_t color_buffer_count,
    std::uint32_t auxiliary_buffer_count,
    std::uint32_t origin)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, logical_width, logical_height,
                            color_buffer_count, auxiliary_buffer_count, origin,
                            &result]() {
            result = OpenWindowed(logical_width, logical_height,
                color_buffer_count, auxiliary_buffer_count, origin);
        });
        return result;
    }
    Close();
    exit_requested_ = false;
    dummy_mode_ = false;
    origin_lower_left_ = origin == repiu::hle::kGlideOriginLowerLeft;
#if !defined(_WIN32)
    message_ = "Win32 OpenGL backend is unavailable";
    return false;
#else
    if (logical_width == 0 || logical_height == 0 ||
        color_buffer_count < 2U)
    {
        message_ = "invalid Glide windowed framebuffer request";
        return false;
    }

    if (logical_width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()) / window_scale_ ||
        logical_height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()) / window_scale_)
    {
        message_ = "scaled Glide window dimensions are too large";
        return false;
    }
    const int window_width = static_cast<int>(logical_width * window_scale_);
    const int window_height = static_cast<int>(logical_height * window_scale_);

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = std::string("Glide dummy fallback (SDL video): ") +
            SDL_GetError();
        return true;
    }
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,
                        auxiliary_buffer_count != 0U ? 24 : 0);
    const std::string window_title = BuildWindowTitle(0.0);
    SDL_Window* window = SDL_CreateWindow(
        window_title.c_str(),
        window_width,
        window_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = std::string("Glide dummy fallback (SDL window): ") +
            SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return true;
    }
    SDL_GLContext render_context = SDL_GL_CreateContext(window);
    if (render_context == nullptr)
    {
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = std::string("Glide dummy fallback (SDL GL context): ") +
            SDL_GetError();
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return true;
    }
    if (!SDL_GL_MakeCurrent(window, render_context))
    {
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = std::string("Glide dummy fallback (SDL GL current): ") +
            SDL_GetError();
        SDL_GL_DestroyContext(render_context);
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return true;
    }

    window_ = window;
    render_context_ = render_context;
    logical_width_ = logical_width;
    logical_height_ = logical_height;
    if (!shader_.Initialize())
    {
        fprintf(stderr, "[repiu-live-debug] Glide OpenWindowed failed to initialize shader, falling back to dummy mode\n");
        Close();
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = "Glide dummy fallback activated (no shader)";
        return true;
    }
    ApplyDrawableViewport();
    // Glide vertices arrive in screen pixel coordinates. Honor the origin
    // grSstWinOpen actually asked for: GR_ORIGIN_UPPER_LEFT (0) needs a
    // y-flipped projection, while GR_ORIGIN_LOWER_LEFT (1) matches OpenGL's
    // default orientation. Hardcoding the flipped form renders the whole scene
    // upside down for a lower-left guest. Culling is disabled by the game
    // (grCullMode 0), so the resulting winding difference is harmless.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (origin_lower_left_)
    {
        glOrtho(0.0,
                static_cast<double>(logical_width),
                0.0,
                static_cast<double>(logical_height),
                -1.0,
                1.0);
    }
    else
    {
        glOrtho(0.0,
                static_cast<double>(logical_width),
                static_cast<double>(logical_height),
                0.0,
                -1.0,
                1.0);
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDL_GL_SwapWindow(window);
    ResetFrameRateMeasurement();
    std::ostringstream stream;
    stream << logical_width << "x" << logical_height
           << " logical Glide window opened at " << window_scale_
           << "x (" << window_width << "x" << window_height << ")";
    message_ = stream.str();
    return true;
#endif
}

void GlideOpenGlBackend::PumpEvents()
{
    if (!IsHostThread())
    {
        InvokeOnHostThread([this]() { PumpEvents(); });
        return;
    }
    if (dummy_mode_ || window_ == nullptr)
    {
        return;
    }
#if defined(_WIN32)
    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            exit_requested_ = true;
            message_ = "SDL3 host exit requested";
            break;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                 (event.key.mod & SDL_KMOD_ALT) != 0)
        {
            std::uint32_t scale = 0;
            switch (event.key.key)
            {
                case SDLK_1:
                    scale = 1U;
                    break;
                case SDLK_2:
                    scale = 2U;
                    break;
                case SDLK_3:
                    scale = 3U;
                    break;
                case SDLK_4:
                    scale = 4U;
                    break;
                default:
                    break;
            }
            if (scale != 0U)
            {
                ApplyWindowScale(scale);
            }
        }
        else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                 event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            ApplyDrawableViewport();
        }
    }
#endif
}

bool GlideOpenGlBackend::BufferClear(std::uint32_t color, std::uint32_t alpha, std::uint32_t depth)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, color, alpha, depth, &result]() {
            result = BufferClear(color, alpha, depth);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot clear Glide buffer without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        return true;
    }
    const float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(color & 0xFF) / 255.0f;
    const float a = static_cast<float>(alpha & 0xFF) / 255.0f;
    const float d = static_cast<float>(depth) / 65535.0f;
    glClearColor(r, g, b, a);
    glClearDepth(d);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    message_ = "Glide buffer cleared";
    return true;
#endif
}

bool GlideOpenGlBackend::BufferSwap(std::uint32_t swap_interval)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, swap_interval, &result]() {
            result = BufferSwapOnHostThread(swap_interval, true);
        });
        return result;
    }
    return BufferSwapOnHostThread(swap_interval, false);
}

bool GlideOpenGlBackend::BufferSwapOnHostThread(
    std::uint32_t swap_interval,
    bool guest_gate_command)
{
#if !defined(_WIN32)
    (void)swap_interval;
    (void)guest_gate_command;
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot swap Glide buffer without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        return true;
    }
    Win32GlideBufferSwapTimingProfile* swap_timing =
        guest_gate_command && GlideBufferSwapTimingProfileEnabled()
            ? &glide_buffer_swap_timing_
            : nullptr;
    const std::uint64_t entry_cycles =
        swap_timing != nullptr ? ReadGlideBufferSwapTimingCycles() : 0U;
    if (swap_timing != nullptr &&
        swap_timing->sdl_interval_query_count == 0U)
    {
        int interval = 0;
        const bool queried = SDL_GL_GetSwapInterval(&interval);
        RecordGlideBufferSwapSdlInterval(
            swap_timing, queried, static_cast<std::int32_t>(interval));
    }
    // Optional verification diagnostic (off by default): in a headless session
    // the GL window is not screenshot-able, so sampling the back buffer for
    // non-black pixels proves geometry is rasterized (the black clear is the
    // discriminator). Enable with REPIU_GLIDE_PIXEL_DIAG; sampling is bounded so
    // it never adds steady-state glReadPixels cost. Used to verify Task 254.
    static const bool pixel_diagnostic_enabled =
        std::getenv("REPIU_GLIDE_PIXEL_DIAG") != nullptr;
    static std::atomic<long> swap_diag_count{0};
    const long swap_index = swap_diag_count.fetch_add(1) + 1;
    if (pixel_diagnostic_enabled &&
        (swap_index <= 60 || swap_index % 200 == 0) && swap_index <= 4000 &&
        logical_width_ > 0 && logical_height_ > 0)
    {
        const int width = static_cast<int>(logical_width_);
        const int height = static_cast<int>(logical_height_);
        std::vector<unsigned char> pixels(
            static_cast<std::size_t>(width) * height * 3U);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE,
                     pixels.data());
        std::size_t non_black = 0;
        std::uint64_t sum_r = 0;
        std::uint64_t sum_g = 0;
        std::uint64_t sum_b = 0;
        for (std::size_t i = 0; i + 2U < pixels.size(); i += 3U)
        {
            if (pixels[i] > 8U || pixels[i + 1U] > 8U || pixels[i + 2U] > 8U)
            {
                ++non_black;
                sum_r += pixels[i];
                sum_g += pixels[i + 1U];
                sum_b += pixels[i + 2U];
            }
        }
        const std::size_t denom = non_black == 0 ? 1U : non_black;
        fprintf(stderr,
                "[repiu-live-debug] Glide swap #%ld non-black pixels=%zu/%d"
                " avg-rgb=%llu,%llu,%llu\n",
                swap_index, non_black, width * height,
                static_cast<unsigned long long>(sum_r / denom),
                static_cast<unsigned long long>(sum_g / denom),
                static_cast<unsigned long long>(sum_b / denom));
    }
    const std::uint64_t present_start_cycles =
        swap_timing != nullptr ? ReadGlideBufferSwapTimingCycles() : 0U;
    const bool swapped =
        SDL_GL_SwapWindow(static_cast<SDL_Window*>(window_));
    const std::uint64_t present_end_cycles =
        swap_timing != nullptr ? ReadGlideBufferSwapTimingCycles() : 0U;
    if (!swapped)
    {
        message_ = std::string("SDL3 Glide buffer swap failed: ") +
            SDL_GetError();
        if (swap_timing != nullptr)
        {
            const std::uint64_t finish_cycles =
                ReadGlideBufferSwapTimingCycles();
            RecordGlideBufferSwapTiming(
                swap_timing, swap_interval, false, entry_cycles,
                present_start_cycles, present_end_cycles, present_end_cycles,
                finish_cycles);
        }
        return false;
    }
    RecordPresentedFrame();
    const std::uint64_t accounting_end_cycles =
        swap_timing != nullptr ? ReadGlideBufferSwapTimingCycles() : 0U;
    message_ = "Glide buffer swapped";
    if (swap_timing != nullptr)
    {
        const std::uint64_t finish_cycles =
            ReadGlideBufferSwapTimingCycles();
        RecordGlideBufferSwapTiming(
            swap_timing, swap_interval, true, entry_cycles,
            present_start_cycles, present_end_cycles, accounting_end_cycles,
            finish_cycles);
    }
    return true;
#endif
}

bool GlideOpenGlBackend::DrawTriangle(const GlideDrawVertex& a,
                                      const GlideDrawVertex& b,
                                      const GlideDrawVertex& c)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, &a, &b, &c, &result]() {
            result = DrawTriangle(a, b, c);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot draw Glide triangle without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        return true;
    }
    // R3: when the color combine selects the texture (SCALE_OTHER) and a texture
    // is currently sourced, bind it and sample; otherwise output iterated color.
    const bool sample_texture =
        texture_combine_enabled_ && current_texture_ != nullptr &&
        current_texture_->gl_name != 0U;
    float inv_w = 1.0F;
    float inv_h = 1.0F;
    if (sample_texture)
    {
        glBindTexture(GL_TEXTURE_2D, current_texture_->gl_name);
        // Task 332: normalize by the Glide coordinate extent, not the pixel
        // size. They are equal for every map whose longer edge is 256, which is
        // why only smaller sprites -- the difficulty dots and the arrows --
        // were wrong.
        inv_w = current_texture_->s_extent != 0U
            ? 1.0F / static_cast<float>(current_texture_->s_extent)
            : 1.0F;
        inv_h = current_texture_->t_extent != 0U
            ? 1.0F / static_cast<float>(current_texture_->t_extent)
            : 1.0F;
    }
    shader_.SetTextureEnabled(sample_texture);
    const GlideDrawVertex* const vertices[3] = {&a, &b, &c};
    glBegin(GL_TRIANGLES);
    for (const GlideDrawVertex* vertex : vertices)
    {
        glColor4f(vertex->r, vertex->g, vertex->b, vertex->a);
        if (sample_texture)
        {
            // Pack normalized sow/tow and the shared texture/fog oow. Because
            // orthographic clip w is constant, the shader receives linearly
            // interpolated numerators and reciprocal-w and performs the Glide
            // perspective divide per fragment.
            glTexCoord4f(vertex->s * inv_w, vertex->t * inv_h,
                         vertex->fog_oow, vertex->texture_oow);
        }
        else
        {
            glTexCoord4f(0.0F, 0.0F, vertex->fog_oow, 1.0F);
        }
        glVertex3f(vertex->x, vertex->y, 0.0F);
    }
    glEnd();
    message_ = "Glide compact triangle drawn";
    return true;
#endif
}

bool GlideOpenGlBackend::StoreTexture(std::uint32_t start_address,
                                      std::uint32_t format,
                                      std::uint32_t large_lod,
                                      std::uint32_t aspect_ratio,
                                      const std::uint8_t* source,
                                      std::size_t source_size,
                                      const std::uint8_t* palette_rgba8)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, start_address, format, large_lod,
                            aspect_ratio, source, source_size, palette_rgba8, &result]() {
            result = StoreTexture(start_address, format, large_lod,
                                  aspect_ratio, source, source_size, palette_rgba8);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!repiu::hle::IsGlideTextureFormatAcceptable(format))
    {
        message_ = "unacceptable Glide texture format";
        return false;
    }
    if (!is_open() || dummy_mode_ || source == nullptr)
    {
        return false;
    }
    repiu::hle::GlideTextureDimensions dimensions;
    if (!repiu::hle::CalculateGlideTextureDimensions(large_lod, aspect_ratio,
                                                     &dimensions))
    {
        message_ = "unsupported Glide texture dimensions";
        return false;
    }
    std::vector<std::uint8_t> rgba8;
    if (!repiu::hle::DecodeGlideTextureToRgba8(format, dimensions.width,
                                               dimensions.height, source,
                                               source_size, palette_rgba8, &rgba8))
    {
        message_ = "unsupported Glide texture format";
        return false;
    }
    static const bool tex_diagnostic_enabled =
        std::getenv("REPIU_GLIDE_TEX_DIAG") != nullptr;
    if (tex_diagnostic_enabled)
    {
        static std::atomic<long> store_diag_count{0};
        const long store_index = store_diag_count.fetch_add(1) + 1;
        if (store_index <= 16)
        {
            fprintf(stderr,
                    "[repiu-live-debug] StoreTexture #%ld addr=0x%08X format=%u"
                    " %ux%u texel0-rgba=%u,%u,%u,%u\n",
                    store_index, start_address, format, dimensions.width,
                    dimensions.height, rgba8[0], rgba8[1], rgba8[2], rgba8[3]);
        }
    }
    TextureEntry& entry = textures_[start_address];
    if (entry.gl_name == 0U)
    {
        GLuint name = 0;
        glGenTextures(1, &name);
        entry.gl_name = name;
    }
    entry.width = dimensions.width;
    entry.height = dimensions.height;
    // Task 332: the coordinate extent follows the aspect ratio alone, so it
    // equals the pixel size only when the longer edge is already 256.
    repiu::hle::CalculateGlideTextureCoordinateExtent(
        aspect_ratio, &entry.s_extent, &entry.t_extent);
    glBindTexture(GL_TEXTURE_2D, entry.gl_name);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(dimensions.width),
                 static_cast<GLsizei>(dimensions.height), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba8.data());
    message_ = "Glide texture stored";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SourceTexture(std::uint32_t start_address)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, start_address, &result]() {
            result = SourceTexture(start_address);
        });
        return result;
    }
    const auto found = textures_.find(start_address);
    if (found == textures_.end())
    {
        // Task 332: record the miss. A miss leaves the draw untextured, which
        // looks like a missing sprite rather than a wrong one, so the address
        // is kept for the census to report.
        current_texture_ = nullptr;
        current_texture_address_ = 0U;
        last_missing_texture_address_ = start_address;
        ++missing_texture_source_count_;
        return false;
    }
    current_texture_ = &found->second;
    current_texture_address_ = start_address;
    glBindTexture(GL_TEXTURE_2D, current_texture_->gl_name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    tmu_min_filter_ == 1 ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    tmu_mag_filter_ == 1 ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    tmu_s_clamp_ == 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    tmu_t_clamp_ == 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    return true;
}

void GlideOpenGlBackend::SetTextureClampMode(std::uint32_t s_clamp,
                                             std::uint32_t t_clamp)
{
    if (!IsHostThread())
    {
        InvokeOnHostThread([this, s_clamp, t_clamp]() {
            SetTextureClampMode(s_clamp, t_clamp);
        });
        return;
    }
#if defined(_WIN32)
    tmu_s_clamp_ = s_clamp;
    tmu_t_clamp_ = t_clamp;
    if (is_open() && !dummy_mode_ && current_texture_ != nullptr &&
        current_texture_->gl_name != 0U)
    {
        glBindTexture(GL_TEXTURE_2D, current_texture_->gl_name);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        tmu_s_clamp_ == 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        tmu_t_clamp_ == 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }
#endif
}

void GlideOpenGlBackend::SetTextureFilterMode(std::uint32_t min_filter,
                                              std::uint32_t mag_filter)
{
    if (!IsHostThread())
    {
        InvokeOnHostThread([this, min_filter, mag_filter]() {
            SetTextureFilterMode(min_filter, mag_filter);
        });
        return;
    }
#if defined(_WIN32)
    tmu_min_filter_ = min_filter;
    tmu_mag_filter_ = mag_filter;
    if (is_open() && !dummy_mode_ && current_texture_ != nullptr &&
        current_texture_->gl_name != 0U)
    {
        glBindTexture(GL_TEXTURE_2D, current_texture_->gl_name);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        tmu_min_filter_ == 1 ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        tmu_mag_filter_ == 1 ? GL_LINEAR : GL_NEAREST);
    }
#endif
}

void GlideOpenGlBackend::SetTextureCombineEnabled(bool enabled)
{
    if (!IsHostThread())
    {
        InvokeOnHostThread([this, enabled]() { SetTextureCombineEnabled(enabled); });
        return;
    }
    texture_combine_enabled_ = enabled;
}

bool GlideOpenGlBackend::PresentLfbSurface(const std::uint8_t* rgba8,
                                           std::uint32_t width,
                                           std::uint32_t height,
                                           bool flip_v,
                                           bool present_to_front)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, rgba8, width, height, flip_v, present_to_front, &result]() {
            result = PresentLfbSurface(rgba8, width, height, flip_v, present_to_front);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot present Glide LFB surface without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide LFB surface presented (dummy)";
        return true;
    }
    if (rgba8 == nullptr || width == 0U || height == 0U)
    {
        message_ = "invalid Glide LFB surface";
        return false;
    }

    GLint previous_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    if (lfb_texture_ == 0U)
    {
        GLuint name = 0;
        glGenTextures(1, &name);
        lfb_texture_ = name;
    }
    glBindTexture(GL_TEXTURE_2D, lfb_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba8);

    // The blit must not inherit the geometry state (depth compare, blending,
    // culling, combine routing) the game set for its triangles, so force the
    // pieces that matter and restore them afterwards.
    const GLboolean depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
    const GLboolean cull_was_enabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean alpha_test_was_enabled = glIsEnabled(GL_ALPHA_TEST);
    const GLboolean scissor_was_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean color_mask[4]{};
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
    GLint draw_buffer = GL_BACK;
    glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDrawBuffer(GL_BACK);
    shader_.SetBlitMode(true);

    // The orthographic projection installed at OpenWindowed already maps screen
    // pixels directly, so the quad spans the logical surface. Two independent
    // orientations decide where staging row 0 lands: the lock's origin
    // (`flip_v`) and the window projection's direction. Under a lower-left
    // projection, vertex y=0 is the bottom of the screen rather than the top,
    // which inverts the mapping again -- XOR them so the two never cancel out
    // silently.
    const bool invert_v = flip_v != origin_lower_left_;
    const float top_v = invert_v ? 1.0F : 0.0F;
    const float bottom_v = invert_v ? 0.0F : 1.0F;
    const float right = static_cast<float>(logical_width_);
    const float bottom = static_cast<float>(logical_height_);
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
    glBegin(GL_TRIANGLES);
    glTexCoord4f(0.0F, top_v, 0.0F, 1.0F);    glVertex3f(0.0F, 0.0F, 0.0F);
    glTexCoord4f(1.0F, top_v, 0.0F, 1.0F);    glVertex3f(right, 0.0F, 0.0F);
    glTexCoord4f(1.0F, bottom_v, 0.0F, 1.0F); glVertex3f(right, bottom, 0.0F);
    glTexCoord4f(0.0F, top_v, 0.0F, 1.0F);    glVertex3f(0.0F, 0.0F, 0.0F);
    glTexCoord4f(1.0F, bottom_v, 0.0F, 1.0F); glVertex3f(right, bottom, 0.0F);
    glTexCoord4f(0.0F, bottom_v, 0.0F, 1.0F); glVertex3f(0.0F, bottom, 0.0F);
    glEnd();

    static std::atomic<long> lfb_diagnostic_count{0};
    const long diagnostic_index = lfb_diagnostic_count.fetch_add(1) + 1;
    if (std::getenv("REPIU_GLIDE_DRAW_DIAG") != nullptr &&
        diagnostic_index <= 4)
    {
        std::size_t input_non_black = 0U;
        std::size_t input_non_zero = 0U;
        std::uint8_t maximum_red = 0U;
        std::uint8_t maximum_green = 0U;
        std::uint8_t maximum_blue = 0U;
        for (std::size_t index = 0U;
             index + 3U < static_cast<std::size_t>(width) * height * 4U;
             index += 4U)
        {
            if (rgba8[index] > 8U || rgba8[index + 1U] > 8U ||
                rgba8[index + 2U] > 8U)
            {
                ++input_non_black;
            }
            if (rgba8[index] != 0U || rgba8[index + 1U] != 0U ||
                rgba8[index + 2U] != 0U)
            {
                ++input_non_zero;
            }
            maximum_red = (std::max)(maximum_red, rgba8[index]);
            maximum_green = (std::max)(maximum_green, rgba8[index + 1U]);
            maximum_blue = (std::max)(maximum_blue, rgba8[index + 2U]);
        }
        GLint viewport[4]{};
        GLint current_program = 0;
        GLint current_texture = 0;
        GLfloat projection[16]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);
        glGetFloatv(GL_PROJECTION_MATRIX, projection);
        const std::size_t sample_width = static_cast<std::size_t>(viewport[2]);
        const std::size_t sample_height = static_cast<std::size_t>(viewport[3]);
        std::vector<std::uint8_t> pixels(sample_width * sample_height * 3U);
        glReadBuffer(GL_BACK);
        glReadPixels(viewport[0], viewport[1], viewport[2], viewport[3], GL_RGB,
                     GL_UNSIGNED_BYTE, pixels.data());
        std::size_t output_non_black = 0U;
        std::size_t output_non_zero = 0U;
        std::uint8_t output_maximum = 0U;
        for (std::size_t index = 0U; index + 2U < pixels.size(); index += 3U)
        {
            if (pixels[index] > 8U || pixels[index + 1U] > 8U ||
                pixels[index + 2U] > 8U)
            {
                ++output_non_black;
            }
            if (pixels[index] != 0U || pixels[index + 1U] != 0U ||
                pixels[index + 2U] != 0U)
            {
                ++output_non_zero;
            }
            output_maximum = (std::max)(output_maximum, pixels[index]);
            output_maximum = (std::max)(output_maximum, pixels[index + 1U]);
            output_maximum = (std::max)(output_maximum, pixels[index + 2U]);
        }
        fprintf(stderr,
                "[repiu-live-debug] LFB draw #%ld input=%zu/%zu max=%u,%u,%u"
                " viewport=%d,%d,%d,%d"
                " program=%d texture=%d projection=%g,%g,%g,%g"
                " output=%zu/%zu nonzero=%zu max=%u\n",
                diagnostic_index, input_non_black, input_non_zero,
                maximum_red, maximum_green, maximum_blue, viewport[0],
                viewport[1], viewport[2], viewport[3], current_program, current_texture,
                projection[0], projection[5], projection[12], projection[13],
                output_non_black, sample_width * sample_height,
                output_non_zero, output_maximum);
    }

    shader_.SetBlitMode(false);
    shader_.SetTextureEnabled(texture_combine_enabled_);
    glDrawBuffer(static_cast<GLenum>(draw_buffer));
    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    if (depth_was_enabled) glEnable(GL_DEPTH_TEST);
    if (blend_was_enabled) glEnable(GL_BLEND);
    if (cull_was_enabled) glEnable(GL_CULL_FACE);
    if (alpha_test_was_enabled) glEnable(GL_ALPHA_TEST);
    if (scissor_was_enabled) glEnable(GL_SCISSOR_TEST);

    if (present_to_front)
    {
        BufferSwap(0);
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    message_ = "Glide LFB surface presented";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::ReadbackFramebuffer(std::uint32_t width,
                                             std::uint32_t height,
                                             std::vector<std::uint8_t>* rgba8)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, width, height, rgba8, &result]() {
            result = ReadbackFramebuffer(width, height, rgba8);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (rgba8 == nullptr || width == 0U || height == 0U || !is_open())
    {
        message_ = "cannot read back Glide framebuffer";
        return false;
    }
    if (dummy_mode_)
    {
        rgba8->assign(static_cast<std::size_t>(width) * height * 4U, 0U);
        message_ = "Glide framebuffer read back (dummy)";
        return true;
    }
    int drawable_width = static_cast<int>(width);
    int drawable_height = static_cast<int>(height);
    if (!SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window_),
                                   &drawable_width, &drawable_height) ||
        drawable_width <= 0 || drawable_height <= 0)
    {
        drawable_width = static_cast<int>(width);
        drawable_height = static_cast<int>(height);
    }
    std::vector<std::uint8_t> drawable_rgba(
        static_cast<std::size_t>(drawable_width) * drawable_height * 4U);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, drawable_width, drawable_height, GL_RGBA,
                 GL_UNSIGNED_BYTE, drawable_rgba.data());
    // glReadPixels returns row 0 as the bottom scanline, while every consumer
    // here treats row 0 as the top. Sample the complete scaled drawable and
    // return the logical Glide dimensions with nearest-neighbor filtering.
    rgba8->assign(static_cast<std::size_t>(width) * height * 4U, 0U);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        const std::size_t source_top_y = static_cast<std::size_t>(y) *
            drawable_height / height;
        const std::size_t source_y = static_cast<std::size_t>(
            drawable_height - 1) - source_top_y;
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const std::size_t source_x = static_cast<std::size_t>(x) *
                drawable_width / width;
            const std::size_t source_offset =
                (source_y * drawable_width + source_x) * 4U;
            const std::size_t destination_offset =
                (static_cast<std::size_t>(y) * width + x) * 4U;
            std::memcpy(rgba8->data() + destination_offset,
                        drawable_rgba.data() + source_offset, 4U);
        }
    }
    message_ = "Glide framebuffer read back";
    return glGetError() == GL_NO_ERROR;
#endif
}
bool GlideOpenGlBackend::SetColorMask(bool rgb, bool alpha)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, rgb, alpha, &result]() {
            result = SetColorMask(rgb, alpha);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot set Glide color mask without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide color mask dummy-applied";
        return true;
    }
    glColorMask(rgb ? GL_TRUE : GL_FALSE,
                rgb ? GL_TRUE : GL_FALSE,
                rgb ? GL_TRUE : GL_FALSE,
                alpha ? GL_TRUE : GL_FALSE);
    message_ = "Glide color mask applied to OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetRenderBuffer(std::uint32_t buffer)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, buffer, &result]() {
            result = SetRenderBuffer(buffer);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideFrontBuffer = 0U;
    constexpr std::uint32_t kGlideBackBuffer = 1U;
    if (!is_open() ||
        (buffer != kGlideFrontBuffer && buffer != kGlideBackBuffer))
    {
        message_ = "unsupported Glide render buffer";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = buffer == kGlideBackBuffer
            ? "Glide back buffer selected (dummy)"
            : "Glide front buffer selected (dummy)";
        return true;
    }
    glDrawBuffer(buffer == kGlideBackBuffer ? GL_BACK : GL_FRONT);
    message_ = buffer == kGlideBackBuffer
        ? "Glide back buffer selected"
        : "Glide front buffer selected";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetDepthMask(bool enabled)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, enabled, &result]() {
            result = SetDepthMask(enabled);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot set Glide depth mask without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = enabled
            ? "Glide depth writes enabled (dummy)"
            : "Glide depth writes disabled (dummy)";
        return true;
    }
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    message_ = enabled
        ? "Glide depth writes enabled"
        : "Glide depth writes disabled";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetDepthBufferMode(std::uint32_t mode)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, mode, &result]() {
            result = SetDepthBufferMode(mode);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideDepthDisabled = 0U;
    constexpr std::uint32_t kGlideZBuffer = 1U;
    if (!is_open() ||
        (mode != kGlideDepthDisabled && mode != kGlideZBuffer))
    {
        message_ = "unsupported Glide depth buffer mode";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = mode == kGlideZBuffer
            ? "Glide Z-buffer mode enabled (dummy)"
            : "Glide depth buffer disabled (dummy)";
        return true;
    }
    if (mode == kGlideZBuffer)
    {
        glEnable(GL_DEPTH_TEST);
        message_ = "Glide Z-buffer mode enabled";
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
        message_ = "Glide depth buffer disabled";
    }
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetAlphaCombine(
    const hle::GlideAlphaCombineState& state)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, &state, &result]() {
            result = SetAlphaCombine(state);
        });
        return result;
    }
    if (!is_open())
    {
        message_ = "cannot set Glide alpha combine without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide alpha combine dummy-applied";
        return true;
    }
    const bool applied = shader_.SetAlphaCombine(state);
    message_ = shader_.message();
    return applied;
}

bool GlideOpenGlBackend::SetConstantColor(std::uint32_t argb)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, argb, &result]() {
            result = SetConstantColor(argb);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot set constant color without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        return true;
    }
    const bool result = shader_.SetConstantColor(argb);
    message_ = shader_.message();
    return result;
#endif
}

bool GlideOpenGlBackend::SetColorCombine(
    const hle::GlideColorCombineState& state)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, &state, &result]() {
            result = SetColorCombine(state);
        });
        return result;
    }
    if (!is_open())
    {
        message_ = "cannot set Glide color combine without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide color combine dummy-applied";
        return true;
    }
    const bool applied = shader_.SetColorCombine(state);
    message_ = shader_.message();
    return applied;
}

#if defined(_WIN32)
namespace
{

// Map a Glide GrAlphaBlendFnc_t factor to its OpenGL blend factor. Glide uses
// the same enum value for the color-scaled variants (2, 6) but interprets them
// relative to the operand, so the source and destination factors differ.
bool MapGlideBlendFactor(std::uint32_t factor, bool is_source, GLenum* gl_factor)
{
    switch (factor)
    {
        case 0U: *gl_factor = GL_ZERO; return true;              // GR_BLEND_ZERO
        case 1U: *gl_factor = GL_SRC_ALPHA; return true;         // SRC_ALPHA
        case 2U: *gl_factor = is_source ? GL_DST_COLOR
                                        : GL_SRC_COLOR; return true;
        case 3U: *gl_factor = GL_DST_ALPHA; return true;         // DST_ALPHA
        case 4U: *gl_factor = GL_ONE; return true;               // GR_BLEND_ONE
        case 5U: *gl_factor = GL_ONE_MINUS_SRC_ALPHA; return true;
        case 6U: *gl_factor = is_source ? GL_ONE_MINUS_DST_COLOR
                                        : GL_ONE_MINUS_SRC_COLOR; return true;
        case 7U: *gl_factor = GL_ONE_MINUS_DST_ALPHA; return true;
        case 15U: *gl_factor = GL_SRC_ALPHA_SATURATE; return true;
        default: return false;
    }
}

}  // namespace
#endif

bool GlideOpenGlBackend::SetAlphaBlend(
    const hle::GlideAlphaBlendState& state)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, &state, &result]() {
            result = SetAlphaBlend(state);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideBlendZero = 0U;
    constexpr std::uint32_t kGlideBlendOne = 4U;
    GLenum gl_source = GL_ONE;
    GLenum gl_destination = GL_ZERO;
    if (!is_open() || !state.valid ||
        !MapGlideBlendFactor(state.rgb_source, true, &gl_source) ||
        !MapGlideBlendFactor(state.rgb_destination, false, &gl_destination))
    {
        message_ = "unsupported Glide alpha-blend function";
        return false;
    }
    const bool opaque = state.rgb_source == kGlideBlendOne &&
        state.rgb_destination == kGlideBlendZero;
    if (dummy_mode_)
    {
        message_ = opaque
            ? "Glide ONE/ZERO alpha blending disabled (dummy)"
            : "Glide alpha blending enabled (dummy)";
        return true;
    }
    while (glGetError() != GL_NO_ERROR)
    {
    }
    if (opaque)
    {
        glDisable(GL_BLEND);
        message_ = "Glide ONE/ZERO alpha blending disabled in OpenGL";
    }
    else
    {
        glEnable(GL_BLEND);
        glBlendFunc(gl_source, gl_destination);
        message_ = "Glide alpha blending enabled in OpenGL";
    }
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetAlphaTestFunction(std::uint32_t function)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, function, &result]() {
            result = SetAlphaTestFunction(function);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open() || function > 7U)
    {
        message_ = "unsupported Glide alpha-test function";
        return false;
    }
    alpha_test_function_ = function;
    if (dummy_mode_)
    {
        message_ = "Glide alpha test applied (dummy)";
        return true;
    }
    if (function == 7U) // GR_CMP_ALWAYS
    {
        glDisable(GL_ALPHA_TEST);
        message_ = "Glide ALWAYS alpha test disabled in OpenGL";
    }
    else
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_NEVER + function, alpha_test_reference_);
        message_ = "Glide alpha test enabled in OpenGL";
    }
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetAlphaTestReferenceValue(std::uint32_t reference_value)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, reference_value, &result]() {
            result = SetAlphaTestReferenceValue(reference_value);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        return false;
    }
    alpha_test_reference_ = static_cast<float>(reference_value) / 255.0f;
    if (dummy_mode_)
    {
        return true;
    }
    if (alpha_test_function_ != 7U)
    {
        glAlphaFunc(GL_NEVER + alpha_test_function_, alpha_test_reference_);
    }
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetDepthBufferFunction(std::uint32_t function)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, function, &result]() {
            result = SetDepthBufferFunction(function);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideCompareAlways = 7U;
    if (!is_open() || function > kGlideCompareAlways)
    {
        message_ = "unsupported Glide depth-buffer function";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide depth comparison applied (dummy)";
        return true;
    }
    glDepthFunc(GL_NEVER + function);
    message_ = "Glide depth comparison applied to OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetFogMode(std::uint32_t mode)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, mode, &result]() {
            result = SetFogMode(mode);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open() || (mode != 0U && mode != 2U))
    {
        message_ = "unsupported Glide fog mode";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = mode == 0U
            ? "Glide fog disabled (dummy)"
            : "Glide table fog enabled (dummy)";
        return true;
    }
    glDisable(GL_FOG);
    const bool applied = shader_.SetFogMode(mode);
    message_ = applied
        ? (mode == 0U ? "Glide fog disabled in GLSL"
                      : "Glide table fog enabled in GLSL")
        : "failed to apply Glide fog mode to GLSL";
    return applied && glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetFogColor(std::uint32_t argb)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, argb, &result]() {
            result = SetFogColor(argb);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot set Glide fog color without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide fog color applied (dummy)";
        return true;
    }
    const bool applied = shader_.SetFogColor(argb);
    message_ = applied ? "Glide fog color applied to GLSL"
                       : "failed to apply Glide fog color to GLSL";
    return applied;
#endif
}

bool GlideOpenGlBackend::SetFogTable(const hle::GlideFogTable& table)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, &table, &result]() {
            result = SetFogTable(table);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open())
    {
        message_ = "cannot set Glide fog table without an OpenGL window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide fog table applied (dummy)";
        return true;
    }
    const bool applied = shader_.SetFogTable(table);
    message_ = applied ? "Glide fog table applied to GLSL"
                       : "failed to apply Glide fog table to GLSL";
    return applied;
#endif
}

bool GlideOpenGlBackend::SetClipWindow(std::uint32_t min_x,
                                       std::uint32_t min_y,
                                       std::uint32_t max_x,
                                       std::uint32_t max_y)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, min_x, min_y, max_x, max_y, &result]() {
            result = SetClipWindow(min_x, min_y, max_x, max_y);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open() || min_x != 0U || min_y != 0U ||
        max_x != logical_width_ || max_y != logical_height_)
    {
        message_ = "unsupported partial Glide clip window";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "full Glide clip window applied (dummy)";
        return true;
    }
    ApplyDrawableViewport();
    glEnable(GL_SCISSOR_TEST);
    message_ = "full Glide clip window applied to OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetCullMode(std::uint32_t mode)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, mode, &result]() {
            result = SetCullMode(mode);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    if (!is_open() || mode != 0U)
    {
        message_ = "unsupported Glide cull mode";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide culling disabled (dummy)";
        return true;
    }
    glDisable(GL_CULL_FACE);
    message_ = "Glide culling disabled in OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetDitherMode(std::uint32_t mode)
{
    if (!IsHostThread())
    {
        bool result = false;
        InvokeOnHostThread([this, mode, &result]() {
            result = SetDitherMode(mode);
        });
        return result;
    }
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kObservedDitherMode = 2U;
    if (!is_open() || mode != kObservedDitherMode)
    {
        message_ = "unsupported Glide dither mode";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "observed Glide dither mode delegated (dummy)";
        return true;
    }
    // TODO(Glide fidelity): replace host dithering with a verified Voodoo
    // ordered-dither GLSL path once mode-2 matrix and PIU color quantization
    // are confirmed. See docs/design/20260712-158-glide-host-dither-policy.md.
    glEnable(GL_DITHER);
    message_ = "observed Glide dither mode delegated to OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

void GlideOpenGlBackend::Close()
{
    if (!IsHostThread())
    {
        InvokeOnHostThread([this]() { Close(); });
        return;
    }
    if (dummy_mode_)
    {
        dummy_mode_ = false;
        logical_width_ = 0;
        logical_height_ = 0;
        window_scale_ = 2U;
        ResetFrameRateMeasurement();
        return;
    }
    try
    {
        SDL_GLContext render_context =
            reinterpret_cast<SDL_GLContext>(render_context_);
        SDL_Window* window = static_cast<SDL_Window*>(window_);
        if (render_context != nullptr)
        {
            SDL_GL_MakeCurrent(window, render_context);
            for (auto& entry : textures_)
            {
                if (entry.second.gl_name != 0U)
                {
                    GLuint name = entry.second.gl_name;
                    glDeleteTextures(1, &name);
                }
            }
            if (lfb_texture_ != 0U)
            {
                GLuint lfb_name = lfb_texture_;
                glDeleteTextures(1, &lfb_name);
            }
            shader_.Shutdown();
            SDL_GL_MakeCurrent(window, nullptr);
            SDL_GL_DestroyContext(render_context);
        }
        if (window != nullptr)
        {
            SDL_DestroyWindow(window);
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "[repiu-live-debug] GlideOpenGlBackend::Close caught standard exception: %s\n", e.what());
    }
    catch (...)
    {
        fprintf(stderr, "[repiu-live-debug] GlideOpenGlBackend::Close caught unknown exception\n");
    }
    render_context_ = nullptr;
    window_ = nullptr;
    logical_width_ = 0;
    logical_height_ = 0;
    window_scale_ = 2U;
    ResetFrameRateMeasurement();
    origin_lower_left_ = false;
    textures_.clear();
    current_texture_ = nullptr;
    texture_combine_enabled_ = false;
    lfb_texture_ = 0;
}

}  // namespace repiu::platform::win32
