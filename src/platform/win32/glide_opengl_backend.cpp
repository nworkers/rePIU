#include "repiu/platform/win32/glide_opengl_backend.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#endif

#include <sstream>

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
    std::uint32_t auxiliary_buffer_count)
{
    Close();
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
        message_ = "failed to register Glide OpenGL window class";
        return false;
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
        message_ = "failed to create Glide OpenGL window";
        return false;
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
        message_ = "failed to configure Glide OpenGL pixel format";
        return false;
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
        message_ = "failed to create or activate Glide OpenGL context";
        return false;
    }

    window_ = window;
    device_context_ = device_context;
    render_context_ = render_context;
    logical_width_ = logical_width;
    logical_height_ = logical_height;
    if (!shader_.Initialize())
    {
        message_ = shader_.message();
        Close();
        return false;
    }
    glViewport(0,
               0,
               static_cast<GLsizei>(logical_width),
               static_cast<GLsizei>(logical_height));
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
#if defined(_WIN32)
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
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
    const bool applied = shader_.SetColorCombine(state);
    message_ = shader_.message();
    return applied;
}

bool GlideOpenGlBackend::SetAlphaBlend(
    const hle::GlideAlphaBlendState& state)
{
#if !defined(_WIN32)
    return false;
#else
    constexpr std::uint32_t kGlideBlendZero = 0U;
    constexpr std::uint32_t kGlideBlendOne = 4U;
    if (!is_open() || !state.valid ||
        state.rgb_source != kGlideBlendOne ||
        state.rgb_destination != kGlideBlendZero ||
        state.alpha_source != kGlideBlendOne ||
        state.alpha_destination != kGlideBlendZero)
    {
        message_ = "unsupported Glide alpha-blend function";
        return false;
    }
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glDisable(GL_BLEND);
    message_ = "Glide ONE/ZERO alpha blending disabled in OpenGL";
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
#if defined(_WIN32)
    HGLRC render_context = static_cast<HGLRC>(render_context_);
    HDC device_context = static_cast<HDC>(device_context_);
    HWND window = static_cast<HWND>(window_);
    if (render_context != nullptr)
    {
        wglMakeCurrent(device_context, render_context);
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
#endif
    render_context_ = nullptr;
    device_context_ = nullptr;
    window_ = nullptr;
    logical_width_ = 0;
    logical_height_ = 0;
}

}  // namespace repiu::platform::win32
