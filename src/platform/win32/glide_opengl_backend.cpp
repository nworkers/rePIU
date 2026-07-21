#include "repiu/platform/win32/glide_opengl_backend.h"
#include "repiu/hle/glide_texture_decode.h"
#include "repiu/hle/glide_lfb.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#endif

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

namespace repiu::platform::win32
{
namespace
{

#if defined(_WIN32)
constexpr wchar_t kWindowClassName[] = L"rePIU.GlideOpenGL";

LRESULT CALLBACK GlideWindowProcedure(HWND window,
                                      UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam)
{
    if (message == WM_CLOSE)
    {
        ShowWindow(window, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool RegisterGlideWindowClass(HINSTANCE instance)
{
    WNDCLASSW existing{};
    if (GetClassInfoW(instance, kWindowClassName, &existing))
    {
        return true;
    }
    WNDCLASSW window_class{};
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = GlideWindowProcedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = kWindowClassName;
    return RegisterClassW(&window_class) != 0;
}
#endif

}  // namespace

GlideOpenGlBackend::~GlideOpenGlBackend()
{
    Close();
}

bool GlideOpenGlBackend::OpenWindowed(
    std::uint32_t logical_width,
    std::uint32_t logical_height,
    std::uint32_t color_buffer_count,
    std::uint32_t auxiliary_buffer_count,
    std::uint32_t origin)
{
    Close();
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

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterGlideWindowClass(instance))
    {
        fprintf(stderr, "[repiu-live-debug] Glide OpenWindowed failed to register class, falling back to dummy mode\n");
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = "Glide dummy fallback activated (no class)";
        return true;
    }

    RECT window_rectangle{0,
                          0,
                          static_cast<LONG>(logical_width),
                          static_cast<LONG>(logical_height)};
    constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&window_rectangle, kWindowStyle, FALSE);
    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        L"rePIU - Glide 2 OpenGL",
        kWindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rectangle.right - window_rectangle.left,
        window_rectangle.bottom - window_rectangle.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr)
    {
        fprintf(stderr, "[repiu-live-debug] Glide OpenWindowed failed to create window, falling back to dummy mode\n");
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = "Glide dummy fallback activated (no window)";
        return true;
    }

    HDC device_context = GetDC(window);
    PIXELFORMATDESCRIPTOR pixel_format{};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags =
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cDepthBits = auxiliary_buffer_count != 0U ? 24 : 0;
    pixel_format.iLayerType = PFD_MAIN_PLANE;
    const int format = ChoosePixelFormat(device_context, &pixel_format);
    if (format == 0 ||
        !SetPixelFormat(device_context, format, &pixel_format))
    {
        ReleaseDC(window, device_context);
        DestroyWindow(window);
        fprintf(stderr, "[repiu-live-debug] Glide OpenWindowed failed to configure pixel format, falling back to dummy mode\n");
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = "Glide dummy fallback activated (no pixel format)";
        return true;
    }

    HGLRC render_context = wglCreateContext(device_context);
    if (render_context == nullptr ||
        !wglMakeCurrent(device_context, render_context))
    {
        if (render_context != nullptr)
        {
            wglDeleteContext(render_context);
        }
        ReleaseDC(window, device_context);
        DestroyWindow(window);
        fprintf(stderr, "[repiu-live-debug] Glide OpenWindowed failed to create/activate context, falling back to dummy mode\n");
        dummy_mode_ = true;
        logical_width_ = logical_width;
        logical_height_ = logical_height;
        message_ = "Glide dummy fallback activated (no GL context)";
        return true;
    }

    window_ = window;
    device_context_ = device_context;
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
    glViewport(0,
               0,
               static_cast<GLsizei>(logical_width),
               static_cast<GLsizei>(logical_height));
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
    SwapBuffers(device_context);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    message_ = "640x480 logical Glide window opened with WGL";
    return true;
#endif
}

void GlideOpenGlBackend::PumpEvents()
{
    if (dummy_mode_)
    {
        return;
    }
#if defined(_WIN32)
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
#endif
}

bool GlideOpenGlBackend::BufferClear(std::uint32_t color, std::uint32_t alpha, std::uint32_t depth)
{
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
#if !defined(_WIN32)
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
    // Optional verification diagnostic (off by default): in a headless session
    // the GL window is not screenshot-able, so sampling the back buffer for
    // non-black pixels proves geometry is rasterized (the black clear is the
    // discriminator). Enable with REPIU_GLIDE_PIXEL_DIAG; sampling is bounded so
    // it never adds steady-state glReadPixels cost. Used to verify Task 254.
    static const bool pixel_diagnostic_enabled =
        std::getenv("REPIU_GLIDE_PIXEL_DIAG") != nullptr;
    static long swap_diag_count = 0;
    const long swap_index = InterlockedIncrement(&swap_diag_count);
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
    auto hdc = static_cast<HDC>(device_context_);
    if (hdc != nullptr)
    {
        SwapBuffers(hdc);
    }
    message_ = "Glide buffer swapped";
    return true;
#endif
}

bool GlideOpenGlBackend::DrawTriangle(const GlideDrawVertex& a,
                                      const GlideDrawVertex& b,
                                      const GlideDrawVertex& c)
{
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
        inv_w = current_texture_->width != 0U
            ? 1.0F / static_cast<float>(current_texture_->width)
            : 1.0F;
        inv_h = current_texture_->height != 0U
            ? 1.0F / static_cast<float>(current_texture_->height)
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
            glTexCoord2f(vertex->s * inv_w, vertex->t * inv_h);
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
                                      std::size_t source_size)
{
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
                                               source_size, nullptr, &rgba8))
    {
        message_ = "unsupported Glide texture format";
        return false;
    }
    static const bool tex_diagnostic_enabled =
        std::getenv("REPIU_GLIDE_TEX_DIAG") != nullptr;
    if (tex_diagnostic_enabled)
    {
        static long store_diag_count = 0;
        const long store_index = InterlockedIncrement(&store_diag_count);
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
    glBindTexture(GL_TEXTURE_2D, entry.gl_name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
    const auto found = textures_.find(start_address);
    if (found == textures_.end())
    {
        current_texture_ = nullptr;
        return false;
    }
    current_texture_ = &found->second;
    return true;
}

void GlideOpenGlBackend::SetTextureCombineEnabled(bool enabled)
{
    texture_combine_enabled_ = enabled;
}

bool GlideOpenGlBackend::PresentLfbSurface(const std::uint8_t* rgba8,
                                           std::uint32_t width,
                                           std::uint32_t height,
                                           bool flip_v)
{
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
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    shader_.SetTextureEnabled(true);

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
    glTexCoord2f(0.0F, top_v);    glVertex3f(0.0F, 0.0F, 0.0F);
    glTexCoord2f(1.0F, top_v);    glVertex3f(right, 0.0F, 0.0F);
    glTexCoord2f(1.0F, bottom_v); glVertex3f(right, bottom, 0.0F);
    glTexCoord2f(0.0F, top_v);    glVertex3f(0.0F, 0.0F, 0.0F);
    glTexCoord2f(1.0F, bottom_v); glVertex3f(right, bottom, 0.0F);
    glTexCoord2f(0.0F, bottom_v); glVertex3f(0.0F, bottom, 0.0F);
    glEnd();

    shader_.SetTextureEnabled(texture_combine_enabled_);
    if (depth_was_enabled == GL_TRUE)
    {
        glEnable(GL_DEPTH_TEST);
    }
    if (blend_was_enabled == GL_TRUE)
    {
        glEnable(GL_BLEND);
    }
    if (cull_was_enabled == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    // Leave the geometry texture binding as the game left it.
    if (current_texture_ != nullptr && current_texture_->gl_name != 0U)
    {
        glBindTexture(GL_TEXTURE_2D, current_texture_->gl_name);
    }
    message_ = "Glide LFB surface presented";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::ReadbackFramebuffer(std::uint32_t width,
                                             std::uint32_t height,
                                             std::vector<std::uint8_t>* rgba8)
{
#if !defined(_WIN32)
    return false;
#else
    if (rgba8 == nullptr || width == 0U || height == 0U || !is_open())
    {
        message_ = "cannot read back Glide framebuffer";
        return false;
    }
    rgba8->assign(static_cast<std::size_t>(width) * height * 4U, 0U);
    if (dummy_mode_)
    {
        message_ = "Glide framebuffer read back (dummy)";
        return true;
    }
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba8->data());
    // glReadPixels returns row 0 as the bottom scanline, while every consumer
    // here (the LFB staging surface, texture uploads) treats row 0 as the top.
    // Flip once at the source so callers never have to think about it.
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> scratch(row_bytes);
    for (std::uint32_t row = 0; row < height / 2U; ++row)
    {
        std::uint8_t* top = rgba8->data() + static_cast<std::size_t>(row) *
            row_bytes;
        std::uint8_t* bottom = rgba8->data() +
            static_cast<std::size_t>(height - 1U - row) * row_bytes;
        std::memcpy(scratch.data(), top, row_bytes);
        std::memcpy(top, bottom, row_bytes);
        std::memcpy(bottom, scratch.data(), row_bytes);
    }
    message_ = "Glide framebuffer read back";
    return glGetError() == GL_NO_ERROR;
#endif
}
bool GlideOpenGlBackend::SetColorMask(bool rgb, bool alpha)
{
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

bool GlideOpenGlBackend::SetColorCombine(
    const hle::GlideColorCombineState& state)
{
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
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideCompareAlways = 7U;
    if (!is_open() || function != kGlideCompareAlways)
    {
        message_ = "unsupported Glide alpha-test function";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide ALWAYS alpha test disabled (dummy)";
        return true;
    }
    glDisable(GL_ALPHA_TEST);
    message_ = "Glide ALWAYS alpha test disabled in OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetDepthBufferFunction(std::uint32_t function)
{
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideCompareAlways = 7U;
    if (!is_open() || function != kGlideCompareAlways)
    {
        message_ = "unsupported Glide depth-buffer function";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide ALWAYS depth comparison applied (dummy)";
        return true;
    }
    glDepthFunc(GL_ALWAYS);
    message_ = "Glide ALWAYS depth comparison applied to OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetFogMode(std::uint32_t mode)
{
#if !defined(_WIN32)
    return false;
#else
    if (!is_open() || mode != 0U)
    {
        message_ = "unsupported Glide fog mode";
        return false;
    }
    if (dummy_mode_)
    {
        message_ = "Glide fog disabled (dummy)";
        return true;
    }
    glDisable(GL_FOG);
    message_ = "Glide fog disabled in OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetClipWindow(std::uint32_t min_x,
                                       std::uint32_t min_y,
                                       std::uint32_t max_x,
                                       std::uint32_t max_y)
{
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
    glViewport(0, 0, static_cast<GLsizei>(logical_width_),
               static_cast<GLsizei>(logical_height_));
    glScissor(0, 0, static_cast<GLsizei>(logical_width_),
              static_cast<GLsizei>(logical_height_));
    glEnable(GL_SCISSOR_TEST);
    message_ = "full Glide clip window applied to OpenGL";
    return glGetError() == GL_NO_ERROR;
#endif
}

bool GlideOpenGlBackend::SetCullMode(std::uint32_t mode)
{
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
    if (dummy_mode_)
    {
        dummy_mode_ = false;
        logical_width_ = 0;
        logical_height_ = 0;
        return;
    }
    try
    {
        HGLRC render_context = static_cast<HGLRC>(render_context_);
        HDC device_context = static_cast<HDC>(device_context_);
        HWND window = static_cast<HWND>(window_);
        if (render_context != nullptr)
        {
            wglMakeCurrent(device_context, render_context);
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
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(render_context);
        }
        if (window != nullptr && device_context != nullptr)
        {
            ReleaseDC(window, device_context);
        }
        if (window != nullptr)
        {
            DestroyWindow(window);
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
    device_context_ = nullptr;
    window_ = nullptr;
    logical_width_ = 0;
    logical_height_ = 0;
    origin_lower_left_ = false;
    textures_.clear();
    current_texture_ = nullptr;
    texture_combine_enabled_ = false;
    lfb_texture_ = 0;
}

}  // namespace repiu::platform::win32
