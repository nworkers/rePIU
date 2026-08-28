#include "repiu/hle/glide_fog.h"
#include "repiu/hle/glide_lfb.h"
#include "repiu/hle/glide_vertex.h"
#if defined(_WIN32)
#include "repiu/engine/glide_opengl_backend.h"
#endif

#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iostream>
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
