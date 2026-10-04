#include "repiu/hle/glide_fog.h"
#include "repiu/hle/glide_lfb.h"
#include "repiu/hle/glide_vertex.h"
#if defined(_WIN32)
#include "repiu/engine/glide_letterbox.h"
#include "repiu/engine/glide_opengl_backend.h"
#include "repiu/engine/glide_post_process.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool Check(bool condition, const char* message)
{
    if (condition)
    {
        return true;
    }
    std::cerr << "glide_render_probe failure: " << message << '\n';
    return false;
}

float TableWorldDistance(std::uint32_t index)
{
    return std::ldexp(1.0F, 3 + static_cast<int>(index >> 2U)) /
        static_cast<float>(8U - (index & 3U));
}

#if defined(_WIN32)
// Task 768: the post-processing pass in a real context. A white frame goes
// through the scanline shader at full strength and must come back with dark
// and bright rows; the crt shader must draw and leave the curved-off corner
// black; a missing shader must leave no pass; and the Glide state the pass
// touches must be restored.
bool RunOpenGlPostShaderProbe()
{
    constexpr std::uint32_t kWidth = 64U;
    constexpr std::uint32_t kHeight = 48U;
    std::vector<std::uint8_t> white(kWidth * kHeight * 4U, 255U);
    repiu::engine::GlideOpenGlBackend backend;
    backend.BindHostThread();
    if (!Check(backend.OpenWindowed(kWidth, kHeight, 2U, 1U, 1U),
               "OpenGL backend did not open"))
    {
        return false;
    }
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_Window* const window = SDL_GL_GetCurrentWindow();
    if (!Check(window != nullptr &&
                   SDL_GetWindowSizeInPixels(window, &drawable_width,
                                             &drawable_height) &&
                   drawable_width > 0 && drawable_height > 0,
               "drawable size unavailable"))
    {
        return false;
    }
    const auto width = static_cast<std::uint32_t>(drawable_width);
    const auto height = static_cast<std::uint32_t>(drawable_height);
    const auto read_red = [width, height]() {
        std::vector<std::uint8_t> pixels(width * height * 4U);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
        std::vector<std::uint8_t> red(width * height);
        for (std::size_t index = 0; index < red.size(); ++index)
        {
            red[index] = pixels[index * 4U];
        }
        return red;
    };

    repiu::engine::GlidePostProcess post;
    std::string message;
    if (!Check(post.Initialize(&message), "post-process initialize failed") ||
        !Check(!post.Select("no_such_shader.glsl") && !post.active() &&
                   !post.last_error().empty() &&
                   post.active_id() == "none",
               "a missing shader was not refused") ||
        !Check(post.Select("scanline") && post.active() &&
                   post.parameters().size() == 2U,
               "scanline shader did not compile"))
    {
        if (!post.last_error().empty())
        {
            std::cerr << post.last_error() << '\n';
        }
        return false;
    }
    for (repiu::engine::PostShaderParameter& parameter : post.parameters())
    {
        // Full-strength gaps and no brightness boost: gap rows go black.
        parameter.value = 1.0F;
    }

    if (!Check(backend.PresentLfbSurface(white.data(), kWidth, kHeight, false,
                                         false),
               "white surface presentation failed"))
    {
        return false;
    }
    glEnable(GL_BLEND);
    GLint viewport_before[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport_before);
    GLint program_before = 0;
    glGetIntegerv(0x8B8D, &program_before);
    post.Apply(0U, 0U, width, height, kWidth, kHeight);
    GLint viewport_after[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport_after);
    GLint program_after = 0;
    glGetIntegerv(0x8B8D, &program_after);
    const bool state_restored = glIsEnabled(GL_BLEND) == GL_TRUE &&
        std::memcmp(viewport_before, viewport_after,
                    sizeof(viewport_before)) == 0 &&
        program_before == program_after && glGetError() == GL_NO_ERROR;
    glDisable(GL_BLEND);

    const std::vector<std::uint8_t> scanline = read_red();
    std::size_t bright_rows = 0;
    std::size_t dark_rows = 0;
    for (std::uint32_t y = 0; y < height; ++y)
    {
        const std::uint8_t value = scanline[y * width + width / 2U];
        bright_rows += value > 200U ? 1U : 0U;
        dark_rows += value < 60U ? 1U : 0U;
    }
    const bool scanlines_ok =
        bright_rows >= height / 3U && dark_rows >= height / 3U;

    bool crt_ok = post.Select("crt") && post.active() &&
        post.parameters().size() == 6U &&
        backend.PresentLfbSurface(white.data(), kWidth, kHeight, false,
                                  false);
    if (crt_ok)
    {
        post.Apply(0U, 0U, width, height, kWidth, kHeight);
        const std::vector<std::uint8_t> crt = read_red();
        std::uint32_t lit = 0;
        for (std::uint32_t y = height / 4U; y < height * 3U / 4U; ++y)
        {
            lit = (std::max)(lit, static_cast<std::uint32_t>(
                                      crt[y * width + width / 2U]));
        }
        crt_ok = crt[0] == 0U && lit > 128U;
    }
    const bool none_ok = post.Select("none") && !post.active();

    // Task 769: a window twice as wide as the picture's ratio pillarboxes it.
    // The viewport must be the centred content rect, and the readback the
    // guest sees must cover the picture only: the whole drawable is cleared
    // black first, so a readback that included the bars would not be white.
    bool letterbox_ok = false;
    SDL_SetWindowSize(window, static_cast<int>(kWidth * 4U),
                      static_cast<int>(kHeight * 2U));
    SDL_SyncWindow(window);
    backend.PumpEvents();
    int wide_width = 0;
    int wide_height = 0;
    if (SDL_GetWindowSizeInPixels(window, &wide_width, &wide_height) &&
        wide_width > 0 && wide_height > 0)
    {
        const repiu::engine::GlideLetterboxRect expected =
            repiu::engine::ComputeGlideLetterboxRect(
                kWidth, kHeight, static_cast<std::uint32_t>(wide_width),
                static_cast<std::uint32_t>(wide_height));
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        std::vector<std::uint8_t> picture;
        letterbox_ok = expected.x > 0U &&
            viewport[0] == static_cast<GLint>(expected.x) &&
            viewport[1] == static_cast<GLint>(expected.y) &&
            viewport[2] == static_cast<GLint>(expected.width) &&
            viewport[3] == static_cast<GLint>(expected.height) &&
            backend.PresentLfbSurface(white.data(), kWidth, kHeight, false,
                                      false) &&
            backend.ReadbackFramebuffer(kWidth, kHeight, &picture);
        for (std::size_t index = 0; letterbox_ok && index < picture.size();
             index += 4U)
        {
            letterbox_ok = picture[index] == 255U;
        }
        std::cout << "post_shader_gl_letterbox_viewport=" << viewport[0] << ","
                  << viewport[1] << "," << viewport[2] << "," << viewport[3]
                  << " drawable=" << wide_width << "x" << wide_height << '\n';
    }

    // Task 769: Alt+Enter goes fullscreen and a double click comes back,
    // through the backend's own event pump. The display mode must not change,
    // and fullscreen must still letterbox the picture.
    bool fullscreen_ok = false;
    SDL_DisplayMode mode_before{};
    const SDL_DisplayMode* const current_mode =
        SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    if (current_mode != nullptr)
    {
        mode_before = *current_mode;
        SDL_Event key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.windowID = SDL_GetWindowID(window);
        key.key.key = SDLK_RETURN;
        key.key.mod = SDL_KMOD_LALT;
        key.key.down = true;
        SDL_PushEvent(&key);
        backend.PumpEvents();
        SDL_SyncWindow(window);
        backend.PumpEvents();
        int full_width = 0;
        int full_height = 0;
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        const bool entered =
            (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U &&
            SDL_GetWindowSizeInPixels(window, &full_width, &full_height);
        const repiu::engine::GlideLetterboxRect expected =
            repiu::engine::ComputeGlideLetterboxRect(
                kWidth, kHeight, static_cast<std::uint32_t>(full_width),
                static_cast<std::uint32_t>(full_height));
        const SDL_DisplayMode* const full_mode =
            SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
        const bool mode_kept = full_mode != nullptr &&
            full_mode->w == mode_before.w && full_mode->h == mode_before.h;

        SDL_Event click{};
        click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        click.button.windowID = SDL_GetWindowID(window);
        click.button.button = SDL_BUTTON_LEFT;
        click.button.clicks = 2;
        click.button.down = true;
        SDL_PushEvent(&click);
        backend.PumpEvents();
        SDL_SyncWindow(window);
        backend.PumpEvents();
        const bool left =
            (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0U;
        fullscreen_ok = entered && mode_kept && left &&
            viewport[0] == static_cast<GLint>(expected.x) &&
            viewport[1] == static_cast<GLint>(expected.y) &&
            viewport[2] == static_cast<GLint>(expected.width) &&
            viewport[3] == static_cast<GLint>(expected.height);
        std::cout << "glide_fullscreen_entered/mode-kept/left="
                  << entered << "/" << mode_kept << "/" << left
                  << " fullscreen=" << full_width << "x" << full_height
                  << " viewport=" << viewport[0] << "," << viewport[1] << ","
                  << viewport[2] << "," << viewport[3] << '\n';
    }
    std::cout << "glide_fullscreen_toggle=" << (fullscreen_ok ? "true" : "false")
              << '\n';

    post.Shutdown();
    backend.Close();

    std::cout << "post_shader_gl_state_restored="
              << (state_restored ? "true" : "false")
              << "\npost_shader_gl_scanline_rows=" << bright_rows << "/"
              << dark_rows << "/" << height
              << "\npost_shader_gl_scanline=" << (scanlines_ok ? "true" : "false")
              << "\npost_shader_gl_crt=" << (crt_ok ? "true" : "false")
              << "\npost_shader_gl_none=" << (none_ok ? "true" : "false")
              << "\nglide_letterbox_gl=" << (letterbox_ok ? "true" : "false")
              << '\n';
    return Check(state_restored, "post-process left GL state changed") &&
        Check(scanlines_ok, "scanline rows did not alternate") &&
        Check(crt_ok, "crt shader did not draw as expected") &&
        Check(none_ok, "none did not clear the pass") &&
        Check(letterbox_ok, "letterbox viewport or readback was wrong") &&
        Check(fullscreen_ok, "fullscreen toggle did not round-trip");
}
#endif

}  // namespace

int main(int argc, char** argv)
{
    float producer_fields[repiu::hle::kGlideProducerVertexDwordCount] = {};
    producer_fields[0] = 10.0F;
    producer_fields[1] = 20.0F;
    producer_fields[3] = 255.0F;
    producer_fields[4] = 127.5F;
    producer_fields[5] = 63.75F;
    producer_fields[7] = 31.875F;
    producer_fields[8] = 0.5F;
    producer_fields[9] = 128.0F;
    producer_fields[10] = 64.0F;
    std::array<std::uint32_t,
               repiu::hle::kGlideProducerVertexDwordCount> producer{};
    std::memcpy(producer.data(), producer_fields, sizeof(producer_fields));
    repiu::hle::GlideDrawVertex vertex;
    if (!Check(repiu::hle::DecodeGlideProducerVertex(
                   producer.data(), producer.size(), &vertex),
               "producer vertex decode failed") ||
        !Check(vertex.x == 10.0F && vertex.y == 20.0F,
               "producer position decode failed") ||
        !Check(std::abs(vertex.r - 1.0F) < 0.0001F &&
                   std::abs(vertex.g - 0.5F) < 0.0001F &&
                   std::abs(vertex.b - 0.25F) < 0.0001F &&
                   std::abs(vertex.a - 0.125F) < 0.0001F,
               "producer color decode failed") ||
        !Check(vertex.s == 128.0F && vertex.t == 64.0F &&
                   vertex.fog_oow == 0.5F && vertex.texture_oow == 0.5F,
               "producer texture and oow decode failed") ||
        !Check(!repiu::hle::DecodeGlideProducerVertex(
                    producer.data(), producer.size() - 1U, &vertex),
               "short producer vertex was accepted"))
    {
        return 1;
    }

    const std::uint8_t rgb565_red[] = {0x00U, 0xF8U};
    const std::uint8_t rgb565_blue[] = {0x1FU, 0x00U};
    std::vector<std::uint8_t> rgba8;
    if (!Check(repiu::hle::DecodeGlideLfb565ToRgba8(
                   rgb565_red, sizeof(rgb565_red), 1U, 1U,
                   repiu::hle::kGlideColorFormatArgb, &rgba8),
               "RGB565 red decode failed") ||
        !Check(rgba8[0] == 255U && rgba8[1] == 0U && rgba8[2] == 0U,
               "RGB565 red selected the wrong channel") ||
        !Check(repiu::hle::DecodeGlideLfb565ToRgba8(
                   rgb565_red, sizeof(rgb565_red), 1U, 1U,
                   repiu::hle::kGlideColorFormatAbgr, &rgba8),
               "BGR565 blue decode failed") ||
        !Check(rgba8[0] == 0U && rgba8[1] == 0U && rgba8[2] == 255U,
               "BGR565 high channel was not blue") ||
        !Check(repiu::hle::DecodeGlideLfb565ToRgba8(
                   rgb565_blue, sizeof(rgb565_blue), 1U, 1U,
                   repiu::hle::kGlideColorFormatAbgr, &rgba8),
               "BGR565 red decode failed") ||
        !Check(rgba8[0] == 255U && rgba8[1] == 0U && rgba8[2] == 0U,
               "BGR565 low channel was not red"))
    {
        return 1;
    }

    const std::uint8_t cyan[] = {0U, 255U, 255U, 255U};
    std::uint8_t encoded[2] = {};
    if (!Check(repiu::hle::EncodeRgba8ToGlideLfb565(
                   cyan, sizeof(cyan), 1U, 1U,
                   repiu::hle::kGlideColorFormatAbgr, encoded,
                   sizeof(encoded)),
               "BGR565 cyan encode failed") ||
        !Check(encoded[0] == 0xE0U && encoded[1] == 0xFFU,
               "BGR565 cyan packing is incorrect") ||
        !Check(repiu::hle::DecodeGlideLfb565ToRgba8(
                   encoded, sizeof(encoded), 1U, 1U,
                   repiu::hle::kGlideColorFormatAbgr, &rgba8),
               "BGR565 cyan round trip failed") ||
        !Check(rgba8[0] == 0U && rgba8[1] == 255U && rgba8[2] == 255U,
               "BGR565 cyan round trip changed channels"))
    {
        return 1;
    }

    repiu::hle::GlideFogTable table{};
    for (std::uint32_t index = 0U; index < table.size(); ++index)
    {
        table[index] = static_cast<std::uint8_t>(index * 4U);
        std::uint32_t lower_index = 0U;
        float fraction = 1.0F;
        const float oow = 1.0F / TableWorldDistance(index);
        if (!Check(repiu::hle::CalculateGlideFogTableSample(
                       oow, &lower_index, &fraction),
                   "table knot was rejected") ||
            !Check(lower_index == index ||
                       (index > 0U && lower_index == index - 1U &&
                        std::abs(fraction - 1.0F) < 0.0001F),
                   "table knot selected the wrong interval"))
        {
            return 1;
        }
    }

    const float lower_distance = TableWorldDistance(20U);
    const float upper_distance = TableWorldDistance(21U);
    const float midpoint = (lower_distance + upper_distance) * 0.5F;
    std::uint32_t lower_index = 0U;
    float fraction = 0.0F;
    if (!Check(repiu::hle::CalculateGlideFogTableSample(
                   1.0F / midpoint, &lower_index, &fraction),
               "midpoint was rejected") ||
        !Check(lower_index == 20U, "midpoint lower index is incorrect") ||
        !Check(std::abs(fraction - 0.5F) < 0.0001F,
               "midpoint interpolation is incorrect") ||
        !Check(std::abs(repiu::hle::EvaluateGlideFogTable(
                           table, 1.0F / midpoint) -
                       (static_cast<float>(table[20]) +
                        static_cast<float>(table[21])) /
                           (2.0F * 255.0F)) <
                   0.0001F,
               "table factor interpolation is incorrect"))
    {
        return 1;
    }

    if (!Check(repiu::hle::EvaluateGlideFogTable(table, 2.0F) ==
                   static_cast<float>(table[0]) / 255.0F,
               "near distance did not clamp to entry 0") ||
        !Check(repiu::hle::EvaluateGlideFogTable(table, 0.000001F) ==
                   static_cast<float>(table[63]) / 255.0F,
               "far distance did not clamp to entry 63") ||
        !Check(repiu::hle::EvaluateGlideFogTable(table, 0.0F) == 0.0F,
               "invalid oow did not use the safe no-fog value"))
    {
        return 1;
    }

#if defined(_WIN32)
    using repiu::engine::GlideOpenGlCullFace;
    const auto point_size = [](const std::uint32_t drawable_width,
                               const std::uint32_t drawable_height) {
        return repiu::engine::CalculateGlidePointSize(
            640U, 480U, drawable_width, drawable_height);
    };
    if (!Check(point_size(640U, 480U) == 1.0F,
               "1x point scale is incorrect") ||
        !Check(point_size(1280U, 960U) == 2.0F,
               "2x point scale is incorrect") ||
        !Check(point_size(1920U, 1440U) == 3.0F,
               "3x point scale is incorrect") ||
        !Check(point_size(1280U, 720U) == 1.5F,
               "non-uniform point scale did not preserve square points") ||
        !Check(point_size(320U, 240U) == 1.0F,
               "downscaled point size fell below one pixel") ||
        !Check(repiu::engine::CalculateGlidePointSize(
                   0U, 480U, 1280U, 960U) == 1.0F,
               "invalid logical size did not use one pixel"))
    {
        return 1;
    }

    GlideOpenGlCullFace cull_face = GlideOpenGlCullFace::kDisabled;
    if (!Check(repiu::engine::TranslateGlideOpenGlCullMode(
                   0U, false, &cull_face) &&
                   cull_face == GlideOpenGlCullFace::kDisabled,
               "cull disable translation failed") ||
        !Check(repiu::engine::TranslateGlideOpenGlCullMode(
                   1U, true, &cull_face) &&
                   cull_face == GlideOpenGlCullFace::kBack,
               "lower-left negative cull translation failed") ||
        !Check(repiu::engine::TranslateGlideOpenGlCullMode(
                   2U, true, &cull_face) &&
                   cull_face == GlideOpenGlCullFace::kFront,
               "lower-left positive cull translation failed") ||
        !Check(repiu::engine::TranslateGlideOpenGlCullMode(
                   1U, false, &cull_face) &&
                   cull_face == GlideOpenGlCullFace::kFront,
               "upper-left negative cull translation failed") ||
        !Check(repiu::engine::TranslateGlideOpenGlCullMode(
                   2U, false, &cull_face) &&
                   cull_face == GlideOpenGlCullFace::kBack,
               "upper-left positive cull translation failed") ||
        !Check(!repiu::engine::TranslateGlideOpenGlCullMode(
                    3U, false, &cull_face),
               "invalid cull mode was accepted"))
    {
        return 1;
    }

    if (argc == 2 && std::strcmp(argv[1], "--opengl-post-shader") == 0)
    {
        if (!RunOpenGlPostShaderProbe())
        {
            return 1;
        }
    }
    if (argc == 2 && std::strcmp(argv[1], "--opengl-lfb") == 0)
    {
        constexpr std::uint32_t kWidth = 64U;
        constexpr std::uint32_t kHeight = 48U;
        std::vector<std::uint8_t> source(kWidth * kHeight * 4U, 0U);
        for (std::size_t index = 0U; index < source.size(); index += 4U)
        {
            source[index] = 240U;
            source[index + 1U] = 32U;
            source[index + 2U] = 16U;
            source[index + 3U] = 255U;
        }
        repiu::engine::GlideOpenGlBackend backend;
        backend.BindHostThread();
        if (!Check(backend.OpenWindowed(kWidth, kHeight, 2U, 1U, 1U),
                   "OpenGL backend did not open") ||
            !Check(backend.PresentLfbSurface(source.data(), kWidth, kHeight,
                                             false, false),
                   "LFB surface presentation failed"))
        {
            return 1;
        }
        std::vector<std::uint8_t> result;
        if (!Check(backend.ReadbackFramebuffer(kWidth, kHeight, &result),
                   "LFB framebuffer readback failed"))
        {
            return 1;
        }
        std::size_t red_pixels = 0U;
        for (std::size_t index = 0U; index + 3U < result.size(); index += 4U)
        {
            if (result[index] > 200U && result[index + 1U] < 64U &&
                result[index + 2U] < 64U)
            {
                ++red_pixels;
            }
        }
        if (!Check(red_pixels > kWidth * kHeight * 9U / 10U,
                   "LFB blit did not reach the back buffer"))
        {
            return 1;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif
    std::cout << "glide_render_probe=pass\n";
    return 0;
}
