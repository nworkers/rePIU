#ifndef REPIU_HLE_GLIDE_HLE_H_
#define REPIU_HLE_GLIDE_HLE_H_

#include "repiu/exe/executable_headers.h"
#include "repiu/hle/glide_fog.h"

#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace repiu::hle
{

enum class GlideReturnKind : std::uint8_t
{
    kVoid,
    kFxBool,
    kUInt32,
};

enum class GlideGateId : std::uint16_t
{
    kUnknown = 0,
    kGrGlideInit,
    kGrBufferClear,
    kGrBufferSwap,
    kGrBufferNumPending,
    kGrHints,
    kGrSstQueryHardware,
    kGrSstSelect,
    kGrSstWinClose,
    kGrSstWinOpen,
    kGrSstScreenWidth,
    kGrSstScreenHeight,
    kGrTexMinAddress,
    kGrTexMaxAddress,
    kGuFogGenerateExp,
    kGrColorMask,
    kGrRenderBuffer,
    kGrDepthMask,
    kGrDepthBiasLevel,
    kGrDepthBufferMode,
    kGrLfbWriteColorFormat,
    kGrAlphaCombine,
    kGrColorCombine,
    kGrAlphaBlendFunction,
    kGrAlphaTestFunction,
    kGrAlphaTestReferenceValue,
    kGrDepthBufferFunction,
    kGrFogMode,
    kGrFogColorValue,
    kGrFogTable,
    kGrClipWindow,
    kGrTexTextureMemRequired,
    kGrGlideGetState,
    kGrGlideSetState,
    kGrTexDownloadMipMapLevel,
    kGrDrawLine,
    kGrDrawPoint,
    kGrDrawTriangle,
    kGrDrawPlanarPolygon,
    kGrDrawPlanarPolygonVertexList,
    kGrDrawPolygon,
    kGrGlideShutdown,
    kGrLfbLock,
    kGrLfbUnlock,
    kGrLfbWriteRegion,
    kGrLfbReadRegion,
    kGrLfbConstantAlpha,
    kGrLfbConstantDepth,
    kGrLfbWriteColorSwizzle,
    kGrChromaKeyMode,
    kGrChromaKeyValue,
    kGrConstantColorValue,
    kGrConstantColorValue4,
    kGrAADrawPoint,
    kGrAADrawLine,
    kGrAADrawTriangle,
    kGrAADrawPolygon,
    kGrAADrawPolygonVertexList,
    kGrDrawPolygonVertexList,
    kGrTexDownloadMipMap,
    kGrTexDownloadMipMapLevelPartial,
    kGrTexDownloadTable,
    kGrTexDownloadTablePartial,
    kGrTexNccTable,
    kGrTexCalcMemRequired,
    kGrTexCombineFunction,
    kGrTexDetailControl,
    kGrTexLodBiasValue,
    kGrTexMultiBase,
    kGrTexMultiBaseAddress,
    kGrSstIdle,
    kGrSstIsBusy,
    kGrSstStatus,
    kGrSstVideoLine,
    kGrSstVRetraceOn,
    kGrSstControl,
    kGrSstOrigin,
    kGrSstConfigPipeline,
    kGrSstVidMode,
    kGrSstQueryBoards,
    kGrSstPerfStats,
    kGrSstResetPerfStats,
    kGrGammaCorrectionValue,
    kGrAlphaControlSitRgbLighting,
    kGrCullMode,
    kGrDitherMode,
    kGrTexClampMode,
    kGrTexCombine,
    kGrTexFilterMode,
    kGrTexMipMapMode,
    kGrTexSource
};

GlideGateId ResolveGlideGateId(const std::string& name);

struct GlideSignature
{
    const char* name = nullptr;
    std::uint32_t argument_byte_count = 0;
    GlideReturnKind return_kind = GlideReturnKind::kVoid;
};

struct GlideTextureInfo
{
    std::uint32_t small_lod = 0;
    std::uint32_t large_lod = 0;
    std::uint32_t aspect_ratio = 0;
    std::uint32_t format = 0;
    std::uint32_t data = 0;
};

struct GlideExportGate
{
    std::string name;
    std::uint16_t ordinal = 0;
    GlideGateId gate_id = GlideGateId::kUnknown;
    std::uint32_t argument_byte_count = 0;
    std::uint32_t gate_offset = 0;
};

struct GlideGatePlan
{
    bool valid = false;
    std::uint32_t first_gate_offset = 0;
    std::uint32_t gate_stride = 0;
    std::vector<GlideExportGate> exports;
    std::vector<std::uint8_t> image;
    std::string message;
};

struct GlideAlphaCombineState
{
    std::uint32_t function = 0;
    std::uint32_t factor = 0;
    std::uint32_t local = 0;
    std::uint32_t other = 0;
    bool invert = false;
    bool valid = false;
};

using GlideColorCombineState = GlideAlphaCombineState;

struct GlideAlphaBlendState
{
    std::uint32_t rgb_source = 0;
    std::uint32_t rgb_destination = 0;
    std::uint32_t alpha_source = 0;
    std::uint32_t alpha_destination = 0;
    bool valid = false;
};

struct GlideLogicalState
{
    bool initialized = false;
    std::uint32_t selected_board = 0;
    bool window_open = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t color_format = 0;
    std::uint32_t origin = 0;
    std::uint32_t color_buffer_count = 0;
    std::uint32_t auxiliary_buffer_count = 0;
    std::uint32_t texture_memory_bytes = 0;
    std::uint32_t lfb_write_color_format = 0;
    GlideAlphaCombineState alpha_combine;
    GlideColorCombineState color_combine;
    GlideAlphaBlendState alpha_blend;
    std::uint32_t alpha_test_function = 0;
    std::uint32_t alpha_test_reference = 0;
    std::uint32_t depth_buffer_function = 0;
    std::uint32_t fog_mode = 0;
    std::uint32_t fog_color = 0;
    std::uint32_t fog_table_pointer = 0;
    GlideFogTable fog_table{};
    bool fog_table_valid = false;
    std::uint32_t clip_min_x = 0;
    std::uint32_t clip_min_y = 0;
    std::uint32_t clip_max_x = 0;
    std::uint32_t clip_max_y = 0;
    std::uint32_t cull_mode = 0;
    std::uint32_t dither_mode = 0;
    // grConstantColorValue: observed during the content phase (0xFFFFFFFF).
    // Retained so a later CONSTANT combine source can read it.
    std::uint32_t constant_color = 0xFFFFFFFFU;

    // grHints state (Task 332). These are driver optimization declarations, not
    // rendering state: the `GrVertex` layout is fixed by the Glide ABI, so a
    // renderer that reads the structure directly needs no behavior change from
    // any of them. They are recorded because a later change to vertex handling
    // must be able to see what the game declared.
    //
    // `stw_hint` is the GR_HINT_STWHINT mask declaring which w and s/t values
    // are unique per TMU; the default of 0 means every TMU shares TMU0's.
    std::uint32_t stw_hint = 0;
    std::uint32_t fifo_check_hint = 0;
    // GR_HINT_FPUPRECISION asks Glide to run its own math in single precision.
    // Recorded only: the host renderer does not execute the guest's floating
    // point, and changing the x87 control word would alter guest results
    // (accuracy over speed).
    std::uint32_t fpu_precision_hint = 0;
    std::uint32_t allow_mipmap_dither_hint = 0;
    bool hints_seen = false;
    
    // Palette downloaded by grTexDownloadTable.
    std::array<std::uint8_t, 1024> palette_rgba8 = {};
    bool palette_valid = false;
};

constexpr std::uint32_t kPiuBansheeVirtualTextureMemoryBytes =
    8U * 1024U * 1024U;
// The 312-byte Glide2 compatibility observation is cross-checked against
// PIU's 336-byte allocation gap. No external state layout or source is used.
// References:
// https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf
// https://www.zeus-software.com/forum/viewtopic.php?start=10&t=2232
constexpr std::size_t kGlide2StateImageBytes = 312U;
using GlideStateImage = std::array<std::uint8_t, kGlide2StateImageBytes>;

bool BuildGlideStateImage(const GlideLogicalState& state,
                          GlideStateImage* image);
bool ParseGlideStateImage(const GlideStateImage& image,
                          GlideLogicalState* state);

bool BuildGlideGatePlan(
    const std::vector<exe::LeResidentName>& resident_names,
    std::uint32_t first_gate_offset,
    std::uint32_t gate_stride,
    std::uint32_t maximum_image_size,
    GlideGatePlan* plan);

const GlideExportGate* FindGlideExportByName(
    const GlideGatePlan& plan,
    const std::string& name);

const GlideExportGate* DecodeGlideGate(
    const GlideGatePlan& plan,
    std::uint32_t gate_offset);

const GlideSignature* FindGlideSignature(const std::string& name);

bool DecodeGlideResolution(std::uint32_t resolution,
                           std::uint32_t* width,
                           std::uint32_t* height);

bool CalculateGlideTextureMaxAddress(std::uint32_t texture_memory_bytes,
                                     std::uint32_t* maximum_address);

bool CalculateGlideTextureMemoryRequired(std::uint32_t even_odd_mask,
                                       const GlideTextureInfo& info,
                                       std::uint32_t* required_bytes);

// A `GrColor_t` is laid out according to the `GrColorFormat_t` chosen at
// grSstWinOpen, so the same 32-bit value means different colors in different
// runs. Converts one into the canonical ARGB the renderer uses. PIU selects
// GR_COLORFORMAT_ABGR (1), under which an unconverted value swaps red and blue
// (Task 332).
std::uint32_t ConvertGlideColorToArgb(std::uint32_t value,
                                      std::uint32_t color_format);

// Glide texture coordinates are not in texel units: they span a fixed 256-wide
// space for the longer axis, with the shorter axis scaled by the aspect ratio,
// independent of how large the texture actually is. A 32x32 map is therefore
// addressed with s and t running 0..256, so normalizing by the texture's pixel
// size shrinks every map smaller than 256 by exactly the ratio between them
// (Task 332). Returns the coordinate extent for each axis.
void CalculateGlideTextureCoordinateExtent(std::uint32_t aspect_ratio,
                                           std::uint32_t* s_extent,
                                           std::uint32_t* t_extent);

namespace glide_ordinal {

constexpr std::uint16_t kGrGlideInit = 1U;
constexpr std::uint16_t kGrBufferClear = 2U;
constexpr std::uint16_t kGrBufferSwap = 3U;
constexpr std::uint16_t kGrBufferNumPending = 4U;
constexpr std::uint16_t kGrSstQueryHardware = 6U;
constexpr std::uint16_t kGrSstSelect = 7U;
constexpr std::uint16_t kGrSstWinClose = 8U;
constexpr std::uint16_t kGrSstWinOpen = 9U;
constexpr std::uint16_t kGrSstScreenWidth = 10U;
constexpr std::uint16_t kGrSstScreenHeight = 11U;
constexpr std::uint16_t kGrTexMinAddress = 12U;
constexpr std::uint16_t kGrTexMaxAddress = 13U;
constexpr std::uint16_t kGuFogGenerateExp = 16U;
constexpr std::uint16_t kGrHints = 31U;
constexpr std::uint16_t kGrColorMask = 32U;
constexpr std::uint16_t kGrRenderBuffer = 33U;
constexpr std::uint16_t kGrDepthMask = 34U;
constexpr std::uint16_t kGrDepthBiasLevel = 35U;
constexpr std::uint16_t kGrDepthBufferMode = 36U;
constexpr std::uint16_t kGrLfbWriteColorFormat = 37U;
constexpr std::uint16_t kGrAlphaCombine = 38U;
constexpr std::uint16_t kGrColorCombine = 39U;
constexpr std::uint16_t kGrAlphaBlendFunction = 40U;
constexpr std::uint16_t kGrAlphaTestFunction = 41U;
constexpr std::uint16_t kGrAlphaTestReferenceValue = 42U;
constexpr std::uint16_t kGrDepthBufferFunction = 43U;
constexpr std::uint16_t kGrClipWindow = 45U;
constexpr std::uint16_t kGrTexTextureMemRequired = 46U;
constexpr std::uint16_t kGrGlideGetState = 47U;
constexpr std::uint16_t kGrGlideSetState = 48U;
constexpr std::uint16_t kGrTexDownloadMipMapLevel = 49U;
constexpr std::uint16_t kGrDrawLine = 50U;
constexpr std::uint16_t kGrDrawPoint = 51U;
constexpr std::uint16_t kGrDrawTriangle = 52U;
constexpr std::uint16_t kGrDrawPlanarPolygon = 53U;
constexpr std::uint16_t kGrDrawPlanarPolygonVertexList = 54U;
constexpr std::uint16_t kGrDrawPolygon = 55U;
constexpr std::uint16_t kGrConstantColorValue = 66U;
constexpr std::uint16_t kGrLfbLock = 70U;
constexpr std::uint16_t kGrLfbUnlock = 71U;
constexpr std::uint16_t kGrLfbWriteRegion = 72U;
constexpr std::uint16_t kGrLfbReadRegion = 73U;
constexpr std::uint16_t kGrLfbConstantAlpha = 74U;
constexpr std::uint16_t kGrLfbConstantDepth = 75U;
constexpr std::uint16_t kGrTexDownloadTable = 76U;
constexpr std::uint16_t kGrLfbWriteColorSwizzle = 77U;
constexpr std::uint16_t kGrCullMode = 99U;
constexpr std::uint16_t kGrDitherMode = 100U;
constexpr std::uint16_t kGrFogMode = 101U;
constexpr std::uint16_t kGrFogColorValue = 102U;
constexpr std::uint16_t kGrFogTable = 103U;
constexpr std::uint16_t kGrTexClampMode = 131U;
constexpr std::uint16_t kGrTexCombine = 132U;
constexpr std::uint16_t kGrTexFilterMode = 134U;
constexpr std::uint16_t kGrTexMipMapMode = 136U;
constexpr std::uint16_t kGrTexSource = 138U;

}  // namespace glide_ordinal

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_HLE_H_
