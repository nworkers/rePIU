#include "repiu/hle/glide_hle.h"

#include <algorithm>
#include <array>

namespace repiu::hle
{
namespace
{

constexpr std::array<GlideSignature, 99> kObservedSignatures = {{
    {"_GRGLIDEINIT@0", 0U, GlideReturnKind::kVoid},
    {"_GRBUFFERCLEAR@12", 12U, GlideReturnKind::kVoid},
    {"_GRBUFFERSWAP@4", 4U, GlideReturnKind::kVoid},
    {"_GRBUFFERNUMPENDING@0", 0U, GlideReturnKind::kUInt32},
    {"_GRHINTS@8", 8U, GlideReturnKind::kVoid},
    {"_GRSSTQUERYHARDWARE@4", 4U, GlideReturnKind::kFxBool},
    {"_GRSSTSELECT@4", 4U, GlideReturnKind::kVoid},
    {"_GRSSTWINCLOSE@0", 0U, GlideReturnKind::kVoid},
    {"_GRSSTWINOPEN@28", 28U, GlideReturnKind::kFxBool},
    {"_GRSSTSCREENWIDTH@0", 0U, GlideReturnKind::kUInt32},
    {"_GRSSTSCREENHEIGHT@0", 0U, GlideReturnKind::kUInt32},
    {"_GRTEXMINADDRESS@4", 4U, GlideReturnKind::kUInt32},
    {"_GRTEXMAXADDRESS@4", 4U, GlideReturnKind::kUInt32},
    {"_GRTEXTEXTUREMEMREQUIRED@8", 8U, GlideReturnKind::kUInt32},
    {"_GRTEXDOWNLOADMIPMAPLEVEL@32", 32U, GlideReturnKind::kVoid},
    {"_GRTEXCOMBINE@28", 28U, GlideReturnKind::kVoid},
    {"_GRTEXCLAMPMODE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXFILTERMODE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXMIPMAPMODE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXSOURCE@16", 16U, GlideReturnKind::kVoid},
    {"_GRCOLORMASK@8", 8U, GlideReturnKind::kVoid},
    {"_GRRENDERBUFFER@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHMASK@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHBIASLEVEL@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHBUFFERMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRLFBWRITECOLORFORMAT@4", 4U, GlideReturnKind::kVoid},
    {"_GRALPHACOMBINE@20", 20U, GlideReturnKind::kVoid},
    {"_GRCOLORCOMBINE@20", 20U, GlideReturnKind::kVoid},
    {"_GRALPHABLENDFUNCTION@16", 16U, GlideReturnKind::kVoid},
    {"_GRALPHATESTFUNCTION@4", 4U, GlideReturnKind::kVoid},
    {"_GRALPHATESTREFERENCEVALUE@4", 4U, GlideReturnKind::kVoid},
    {"_GRDEPTHBUFFERFUNCTION@4", 4U, GlideReturnKind::kVoid},
    {"_GRFOGMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRCLIPWINDOW@16", 16U, GlideReturnKind::kVoid},
    {"_GRCULLMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRGLIDEGETSTATE@4", 4U, GlideReturnKind::kVoid},
    {"_GRGLIDESETSTATE@4", 4U, GlideReturnKind::kVoid},
    {"_GRDITHERMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRDRAWLINE@8", 8U, GlideReturnKind::kVoid},
    {"_GRDRAWPOINT@4", 4U, GlideReturnKind::kVoid},
    {"_GRDRAWTRIANGLE@12", 12U, GlideReturnKind::kVoid},
    {"_GRDRAWPLANARPOLYGON@12", 12U, GlideReturnKind::kVoid},
    {"_GRDRAWPLANARPOLYGONVERTEXLIST@8", 8U, GlideReturnKind::kVoid},
    {"_GRDRAWPOLYGON@12", 12U, GlideReturnKind::kVoid},
    {"_GRGLIDESHUTDOWN@0", 0U, GlideReturnKind::kVoid},
    {"_GRLFBLOCK@24", 24U, GlideReturnKind::kFxBool},
    {"_GRLFBUNLOCK@8", 8U, GlideReturnKind::kVoid},
    {"_GRLFBWRITEREGION@32", 32U, GlideReturnKind::kFxBool},
    {"_GRLFBREADREGION@28", 28U, GlideReturnKind::kFxBool},
    {"_GRLFBCONSTANTALPHA@4", 4U, GlideReturnKind::kVoid},
    {"_GRLFBCONSTANTDEPTH@4", 4U, GlideReturnKind::kVoid},
    {"_GRLFBWRITECOLORSWIZZLE@8", 8U, GlideReturnKind::kVoid},
    {"_GRCHROMAKEYMODE@4", 4U, GlideReturnKind::kVoid},
    {"_GRCHROMAKEYVALUE@4", 4U, GlideReturnKind::kVoid},
    {"_GRCONSTANTCOLORVALUE@4", 4U, GlideReturnKind::kVoid},
    {"_GRCONSTANTCOLORVALUE4@16", 16U, GlideReturnKind::kVoid},
    {"_GRAADRAWPOINT@4", 4U, GlideReturnKind::kVoid},
    {"_GRAADRAWLINE@8", 8U, GlideReturnKind::kVoid},
    {"_GRAADRAWTRIANGLE@24", 24U, GlideReturnKind::kVoid},
    {"_GRAADRAWPOLYGON@12", 12U, GlideReturnKind::kVoid},
    {"_GRAADRAWPOLYGONVERTEXLIST@8", 8U, GlideReturnKind::kVoid},
    {"_GRDRAWPOLYGONVERTEXLIST@8", 8U, GlideReturnKind::kVoid},
    {"_GRTEXDOWNLOADMIPMAP@16", 16U, GlideReturnKind::kVoid},
    {"_GRTEXDOWNLOADMIPMAPLEVELPARTIAL@40", 40U, GlideReturnKind::kVoid},
    {"_GRTEXDOWNLOADTABLE@12", 12U, GlideReturnKind::kVoid},
    {"_GRTEXDOWNLOADTABLEPARTIAL@20", 20U, GlideReturnKind::kVoid},
    {"_GRTEXNCCTABLE@8", 8U, GlideReturnKind::kVoid},
    {"_GRTEXCALCMEMREQUIRED@16", 16U, GlideReturnKind::kUInt32},
    {"_GRTEXCOMBINEFUNCTION@8", 8U, GlideReturnKind::kVoid},
    {"_GRTEXDETAILCONTROL@16", 16U, GlideReturnKind::kVoid},
    {"_GRTEXLODBIASVALUE@8", 8U, GlideReturnKind::kVoid},
    {"_GRTEXMULTIBASE@8", 8U, GlideReturnKind::kVoid},
    {"_GRTEXMULTIBASEADDRESS@20", 20U, GlideReturnKind::kVoid},
    {"_GRSSTIDLE@0", 0U, GlideReturnKind::kVoid},
    {"_GRSSTISBUSY@0", 0U, GlideReturnKind::kFxBool},
    {"_GRSSTSTATUS@0", 0U, GlideReturnKind::kUInt32},
    {"_GRSSTVIDEOLINE@0", 0U, GlideReturnKind::kUInt32},
    {"_GRSSTVRETRACEON@0", 0U, GlideReturnKind::kFxBool},
    {"_GRSSTCONTROL@4", 4U, GlideReturnKind::kFxBool},
    {"_GRSSTORIGIN@4", 4U, GlideReturnKind::kVoid},
    {"_GRSSTCONFIGPIPELINE@12", 12U, GlideReturnKind::kVoid},
    {"_GRSSTVIDMODE@8", 8U, GlideReturnKind::kVoid},
    {"_GRSSTQUERYBOARDS@4", 4U, GlideReturnKind::kFxBool},
    {"_GRSSTPERFSTATS@4", 4U, GlideReturnKind::kVoid},
    {"_GRSSTRESETPERFSTATS@0", 0U, GlideReturnKind::kVoid},
    {"_GRFOGCOLORVALUE@4", 4U, GlideReturnKind::kVoid},
    {"_GRFOGTABLE@4", 4U, GlideReturnKind::kVoid},
    {"_GRGAMMACORRECTIONVALUE@4", 4U, GlideReturnKind::kVoid},
    {"_GRALPHACONTROLSITRGBLIGHTING@4", 4U, GlideReturnKind::kVoid},
    {"_GRALPHATESTREFERENCEVALUE@4", 4U, GlideReturnKind::kVoid},
    {"_GRDISABLEALLEFFECTS@0", 0U, GlideReturnKind::kVoid},
    {"_GRGLIDEGETVERSION@4", 4U, GlideReturnKind::kVoid},
    {"_GRGLIDESHAMELESSPLUG@4", 4U, GlideReturnKind::kVoid},
    {"_GRERRORSETCALLBACK@4", 4U, GlideReturnKind::kVoid},
    {"_GRSPLASH@20", 20U, GlideReturnKind::kVoid},
    {"_GRCHECKFORROOM@4", 4U, GlideReturnKind::kVoid},
    {"_GRRESETTRISTATS@0", 0U, GlideReturnKind::kVoid},
    {"_GRTRISTATS@8", 8U, GlideReturnKind::kVoid},
    {"_GUFOGGENERATEEXP@8", 8U, GlideReturnKind::kVoid},
}};

}  // namespace

bool BuildGlideStateImage(const GlideLogicalState& state,
                          GlideStateImage* image)
{
    if (image == nullptr)
    {
        return false;
    }
    image->fill(0U);
    std::size_t cursor = 0;
    const auto write_u32 = [&image, &cursor](std::uint32_t value) {
        if (cursor + 4U > image->size())
        {
            return false;
        }
        (*image)[cursor] = static_cast<std::uint8_t>(value);
        (*image)[cursor + 1U] = static_cast<std::uint8_t>(value >> 8U);
        (*image)[cursor + 2U] = static_cast<std::uint8_t>(value >> 16U);
        (*image)[cursor + 3U] = static_cast<std::uint8_t>(value >> 24U);
        cursor += 4U;
        return true;
    };
    constexpr std::uint32_t kStateMagic = 0x53504952U;
    // Version 3 appends grConstantColorValue (Task 257).
    constexpr std::uint32_t kStateVersion = 3U;
    return write_u32(kStateMagic) && write_u32(kStateVersion) &&
        write_u32(state.initialized ? 1U : 0U) &&
        write_u32(state.selected_board) &&
        write_u32(state.window_open ? 1U : 0U) &&
        write_u32(state.width) && write_u32(state.height) &&
        write_u32(state.color_format) && write_u32(state.origin) &&
        write_u32(state.color_buffer_count) &&
        write_u32(state.auxiliary_buffer_count) &&
        write_u32(state.texture_memory_bytes) &&
        write_u32(state.lfb_write_color_format) &&
        write_u32(state.alpha_test_function) &&
        write_u32(state.depth_buffer_function) &&
        write_u32(state.fog_mode) && write_u32(state.clip_min_x) &&
        write_u32(state.clip_min_y) && write_u32(state.clip_max_x) &&
        write_u32(state.clip_max_y) && write_u32(state.cull_mode) &&
        write_u32(state.dither_mode) && write_u32(state.constant_color);
}

bool ParseGlideStateImage(const GlideStateImage& image,
                          GlideLogicalState* state)
{
    if (state == nullptr)
    {
        return false;
    }
    std::size_t cursor = 0;
    const auto read_u32 = [&image, &cursor](std::uint32_t* value) {
        if (value == nullptr || cursor + 4U > image.size())
        {
            return false;
        }
        *value = static_cast<std::uint32_t>(image[cursor]) |
            (static_cast<std::uint32_t>(image[cursor + 1U]) << 8U) |
            (static_cast<std::uint32_t>(image[cursor + 2U]) << 16U) |
            (static_cast<std::uint32_t>(image[cursor + 3U]) << 24U);
        cursor += 4U;
        return true;
    };
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t initialized = 0;
    std::uint32_t window_open = 0;
    GlideLogicalState restored = *state;
    constexpr std::uint32_t kStateMagic = 0x53504952U;
    // Version 3 appends grConstantColorValue (Task 257).
    constexpr std::uint32_t kStateVersion = 3U;
    if (!read_u32(&magic) || !read_u32(&version) ||
        magic != kStateMagic || version != kStateVersion ||
        !read_u32(&initialized) || !read_u32(&restored.selected_board) ||
        !read_u32(&window_open) || !read_u32(&restored.width) ||
        !read_u32(&restored.height) ||
        !read_u32(&restored.color_format) ||
        !read_u32(&restored.origin) ||
        !read_u32(&restored.color_buffer_count) ||
        !read_u32(&restored.auxiliary_buffer_count) ||
        !read_u32(&restored.texture_memory_bytes) ||
        !read_u32(&restored.lfb_write_color_format) ||
        !read_u32(&restored.alpha_test_function) ||
        !read_u32(&restored.depth_buffer_function) ||
        !read_u32(&restored.fog_mode) ||
        !read_u32(&restored.clip_min_x) ||
        !read_u32(&restored.clip_min_y) ||
        !read_u32(&restored.clip_max_x) ||
        !read_u32(&restored.clip_max_y) ||
        !read_u32(&restored.cull_mode) ||
        !read_u32(&restored.dither_mode) ||
        !read_u32(&restored.constant_color))
    {
        return false;
    }
    restored.initialized = initialized != 0U;
    restored.window_open = window_open != 0U;
    *state = restored;
    return true;
}

GlideGateId ResolveGlideGateId(const std::string& name)
{
    if (name == "_GRGLIDEINIT@0") return GlideGateId::kGrGlideInit;
    if (name == "_GRBUFFERCLEAR@12") return GlideGateId::kGrBufferClear;
    if (name == "_GRBUFFERSWAP@4") return GlideGateId::kGrBufferSwap;
    if (name == "_GRBUFFERNUMPENDING@0") return GlideGateId::kGrBufferNumPending;
    if (name == "_GRHINTS@8") return GlideGateId::kGrHints;
    if (name == "_GRSSTQUERYHARDWARE@4") return GlideGateId::kGrSstQueryHardware;
    if (name == "_GRSSTSELECT@4") return GlideGateId::kGrSstSelect;
    if (name == "_GRSSTWINCLOSE@0") return GlideGateId::kGrSstWinClose;
    if (name == "_GRSSTWINOPEN@28") return GlideGateId::kGrSstWinOpen;
    if (name == "_GRSSTSCREENWIDTH@0") return GlideGateId::kGrSstScreenWidth;
    if (name == "_GRSSTSCREENHEIGHT@0") return GlideGateId::kGrSstScreenHeight;
    if (name == "_GRTEXMINADDRESS@4") return GlideGateId::kGrTexMinAddress;
    if (name == "_GRTEXMAXADDRESS@4") return GlideGateId::kGrTexMaxAddress;
    if (name == "_GUFOGGENERATEEXP@8") return GlideGateId::kGuFogGenerateExp;
    if (name == "_GRCOLORMASK@8") return GlideGateId::kGrColorMask;
    if (name == "_GRRENDERBUFFER@4") return GlideGateId::kGrRenderBuffer;
    if (name == "_GRDEPTHMASK@4") return GlideGateId::kGrDepthMask;
    if (name == "_GRDEPTHBIASLEVEL@4") return GlideGateId::kGrDepthBiasLevel;
    if (name == "_GRDEPTHBUFFERMODE@4") return GlideGateId::kGrDepthBufferMode;
    if (name == "_GRLFBWRITECOLORFORMAT@4") return GlideGateId::kGrLfbWriteColorFormat;
    if (name == "_GRALPHACOMBINE@20") return GlideGateId::kGrAlphaCombine;
    if (name == "_GRCOLORCOMBINE@20") return GlideGateId::kGrColorCombine;
    if (name == "_GRALPHABLENDFUNCTION@16") return GlideGateId::kGrAlphaBlendFunction;
    if (name == "_GRALPHATESTFUNCTION@4") return GlideGateId::kGrAlphaTestFunction;
    if (name == "_GRALPHATESTREFERENCEVALUE@4") return GlideGateId::kGrAlphaTestReferenceValue;
    if (name == "_GRDEPTHBUFFERFUNCTION@4") return GlideGateId::kGrDepthBufferFunction;
    if (name == "_GRFOGMODE@4") return GlideGateId::kGrFogMode;
    if (name == "_GRFOGCOLORVALUE@4") return GlideGateId::kGrFogColorValue;
    if (name == "_GRFOGTABLE@4") return GlideGateId::kGrFogTable;
    if (name == "_GRCLIPWINDOW@16") return GlideGateId::kGrClipWindow;
    if (name == "_GRTEXTEXTUREMEMREQUIRED@8") return GlideGateId::kGrTexTextureMemRequired;
    if (name == "_GRGLIDEGETSTATE@4") return GlideGateId::kGrGlideGetState;
    if (name == "_GRGLIDESETSTATE@4") return GlideGateId::kGrGlideSetState;
    if (name == "_GRTEXDOWNLOADMIPMAPLEVEL@32") return GlideGateId::kGrTexDownloadMipMapLevel;
    if (name == "_GRDRAWLINE@8") return GlideGateId::kGrDrawLine;
    if (name == "_GRDRAWPOINT@4") return GlideGateId::kGrDrawPoint;
    if (name == "_GRDRAWTRIANGLE@12") return GlideGateId::kGrDrawTriangle;
    if (name == "_GRDRAWPLANARPOLYGON@12") return GlideGateId::kGrDrawPlanarPolygon;
    if (name == "_GRDRAWPLANARPOLYGONVERTEXLIST@8") return GlideGateId::kGrDrawPlanarPolygonVertexList;
    if (name == "_GRDRAWPOLYGON@12") return GlideGateId::kGrDrawPolygon;
    if (name == "_GRGLIDESHUTDOWN@0") return GlideGateId::kGrGlideShutdown;
    if (name == "_GRLFBLOCK@24") return GlideGateId::kGrLfbLock;
    if (name == "_GRLFBUNLOCK@8") return GlideGateId::kGrLfbUnlock;
    if (name == "_GRLFBWRITEREGION@32") return GlideGateId::kGrLfbWriteRegion;
    if (name == "_GRLFBREADREGION@28") return GlideGateId::kGrLfbReadRegion;
    if (name == "_GRLFBCONSTANTALPHA@4") return GlideGateId::kGrLfbConstantAlpha;
    if (name == "_GRLFBCONSTANTDEPTH@4") return GlideGateId::kGrLfbConstantDepth;
    if (name == "_GRLFBWRITECOLORSWIZZLE@8") return GlideGateId::kGrLfbWriteColorSwizzle;
    if (name == "_GRCHROMAKEYMODE@4") return GlideGateId::kGrChromaKeyMode;
    if (name == "_GRCHROMAKEYVALUE@4") return GlideGateId::kGrChromaKeyValue;
    if (name == "_GRCONSTANTCOLORVALUE@4") return GlideGateId::kGrConstantColorValue;
    if (name == "_GRCONSTANTCOLORVALUE4@16") return GlideGateId::kGrConstantColorValue4;
    if (name == "_GRAADRAWPOINT@4") return GlideGateId::kGrAADrawPoint;
    if (name == "_GRAADRAWLINE@8") return GlideGateId::kGrAADrawLine;
    if (name == "_GRAADRAWTRIANGLE@24") return GlideGateId::kGrAADrawTriangle;
    if (name == "_GRAADRAWPOLYGON@12") return GlideGateId::kGrAADrawPolygon;
    if (name == "_GRAADRAWPOLYGONVERTEXLIST@8") return GlideGateId::kGrAADrawPolygonVertexList;
    if (name == "_GRDRAWPOLYGONVERTEXLIST@8") return GlideGateId::kGrDrawPolygonVertexList;
    if (name == "_GRTEXDOWNLOADMIPMAP@16") return GlideGateId::kGrTexDownloadMipMap;
    if (name == "_GRTEXDOWNLOADMIPMAPLEVELPARTIAL@40") return GlideGateId::kGrTexDownloadMipMapLevelPartial;
    if (name == "_GRTEXDOWNLOADTABLE@12") return GlideGateId::kGrTexDownloadTable;
    if (name == "_GRTEXDOWNLOADTABLEPARTIAL@20") return GlideGateId::kGrTexDownloadTablePartial;
    if (name == "_GRTEXNCCTABLE@8") return GlideGateId::kGrTexNccTable;
    if (name == "_GRTEXCALCMEMREQUIRED@16") return GlideGateId::kGrTexCalcMemRequired;
    if (name == "_GRTEXCOMBINEFUNCTION@8") return GlideGateId::kGrTexCombineFunction;
    if (name == "_GRTEXDETAILCONTROL@16") return GlideGateId::kGrTexDetailControl;
    if (name == "_GRTEXLODBIASVALUE@8") return GlideGateId::kGrTexLodBiasValue;
    if (name == "_GRTEXMULTIBASE@8") return GlideGateId::kGrTexMultiBase;
    if (name == "_GRTEXMULTIBASEADDRESS@20") return GlideGateId::kGrTexMultiBaseAddress;
    if (name == "_GRSSTIDLE@0") return GlideGateId::kGrSstIdle;
    if (name == "_GRSSTISBUSY@0") return GlideGateId::kGrSstIsBusy;
    if (name == "_GRSSTSTATUS@0") return GlideGateId::kGrSstStatus;
    if (name == "_GRSSTVIDEOLINE@0") return GlideGateId::kGrSstVideoLine;
    if (name == "_GRSSTVRETRACEON@0") return GlideGateId::kGrSstVRetraceOn;
    if (name == "_GRSSTCONTROL@4") return GlideGateId::kGrSstControl;
    if (name == "_GRSSTORIGIN@4") return GlideGateId::kGrSstOrigin;
    if (name == "_GRSSTCONFIGPIPELINE@12") return GlideGateId::kGrSstConfigPipeline;
    if (name == "_GRSSTVIDMODE@8") return GlideGateId::kGrSstVidMode;
    if (name == "_GRSSTQUERYBOARDS@4") return GlideGateId::kGrSstQueryBoards;
    if (name == "_GRSSTPERFSTATS@4") return GlideGateId::kGrSstPerfStats;
    if (name == "_GRSSTRESETPERFSTATS@0") return GlideGateId::kGrSstResetPerfStats;
    if (name == "_GRGAMMACORRECTIONVALUE@4") return GlideGateId::kGrGammaCorrectionValue;
    if (name == "_GRALPHACONTROLSITRGBLIGHTING@4") return GlideGateId::kGrAlphaControlSitRgbLighting;
    if (name == "_GRCULLMODE@4") return GlideGateId::kGrCullMode;
    if (name == "_GRDITHERMODE@4") return GlideGateId::kGrDitherMode;
    if (name == "_GRTEXCLAMPMODE@12") return GlideGateId::kGrTexClampMode;
    if (name == "_GRTEXCOMBINE@28") return GlideGateId::kGrTexCombine;
    if (name == "_GRTEXFILTERMODE@12") return GlideGateId::kGrTexFilterMode;
    if (name == "_GRTEXMIPMAPMODE@12") return GlideGateId::kGrTexMipMapMode;
    if (name == "_GRTEXSOURCE@16") return GlideGateId::kGrTexSource;
    return GlideGateId::kUnknown;
}

bool BuildGlideGatePlan(
    const std::vector<exe::LeResidentName>& resident_names,
    std::uint32_t first_gate_offset,
    std::uint32_t gate_stride,
    std::uint32_t maximum_image_size,
    GlideGatePlan* plan)
{
    if (plan == nullptr || resident_names.empty() || gate_stride < 5U)
    {
        return false;
    }

    *plan = GlideGatePlan{};
    plan->first_gate_offset = first_gate_offset;
    plan->gate_stride = gate_stride;
    std::uint16_t maximum_ordinal = 0;
    for (const exe::LeResidentName& resident : resident_names)
    {
        if (resident.ordinal == 0)
        {
            continue;
        }
        if (!resident.decorated_argument_size_valid)
        {
            plan->message = "Glide export has no validated @N ABI suffix";
            return false;
        }
        maximum_ordinal = std::max(maximum_ordinal, resident.ordinal);
        plan->exports.push_back({resident.name,
                                 resident.ordinal,
                                 ResolveGlideGateId(resident.name),
                                 resident.argument_byte_count,
                                 first_gate_offset +
                                     resident.ordinal * gate_stride});
    }

    const std::uint64_t image_size =
        (static_cast<std::uint64_t>(maximum_ordinal) + 1U) * gate_stride;
    if (plan->exports.empty() || image_size > maximum_image_size)
    {
        plan->message = "Glide export gates do not fit in reserved code tail";
        return false;
    }

    plan->image.assign(static_cast<std::size_t>(image_size), 0x90U);
    for (const GlideExportGate& gate : plan->exports)
    {
        const std::size_t offset =
            static_cast<std::size_t>(gate.ordinal) * gate_stride;
        plan->image[offset] = 0x0FU;
        plan->image[offset + 1U] = 0x0BU;
        plan->image[offset + 2U] =
            static_cast<std::uint8_t>(gate.ordinal);
        plan->image[offset + 3U] =
            static_cast<std::uint8_t>(gate.ordinal >> 8U);
        plan->image[offset + 4U] = 0xC3U;
    }
    plan->valid = true;
    plan->message = "asset-derived Glide export gates are ready";
    return true;
}

const GlideExportGate* FindGlideExportByName(
    const GlideGatePlan& plan,
    const std::string& name)
{
    if (!plan.valid)
    {
        return nullptr;
    }
    const auto found = std::find_if(
        plan.exports.begin(),
        plan.exports.end(),
        [&name](const GlideExportGate& gate) { return gate.name == name; });
    return found != plan.exports.end() ? &*found : nullptr;
}

const GlideExportGate* DecodeGlideGate(const GlideGatePlan& plan,
                                       std::uint32_t gate_offset)
{
    if (!plan.valid || gate_offset < plan.first_gate_offset)
    {
        return nullptr;
    }
    const auto found = std::find_if(
        plan.exports.begin(),
        plan.exports.end(),
        [gate_offset](const GlideExportGate& gate) {
            return gate.gate_offset == gate_offset;
        });
    return found != plan.exports.end() ? &*found : nullptr;
}

const GlideSignature* FindGlideSignature(const std::string& name)
{
    const auto found = std::find_if(
        kObservedSignatures.begin(),
        kObservedSignatures.end(),
        [&name](const GlideSignature& signature) {
            return signature.name != nullptr && name == signature.name;
        });
    return found != kObservedSignatures.end() ? &*found : nullptr;
}

bool DecodeGlideResolution(std::uint32_t resolution,
                           std::uint32_t* width,
                           std::uint32_t* height)
{
    if (width == nullptr || height == nullptr || resolution != 7U)
    {
        return false;
    }
    *width = 640U;
    *height = 480U;
    return true;
}

std::uint32_t ConvertGlideColorToArgb(std::uint32_t value,
                                      std::uint32_t color_format)
{
    // GrColorFormat_t: 0 ARGB, 1 ABGR, 2 RGBA, 3 BGRA. The alpha byte leads in
    // the first two and trails in the last two.
    constexpr std::uint32_t kArgb = 0U;
    constexpr std::uint32_t kAbgr = 1U;
    constexpr std::uint32_t kRgba = 2U;
    constexpr std::uint32_t kBgra = 3U;
    const std::uint32_t byte0 = (value >> 24) & 0xFFU;
    const std::uint32_t byte1 = (value >> 16) & 0xFFU;
    const std::uint32_t byte2 = (value >> 8) & 0xFFU;
    const std::uint32_t byte3 = value & 0xFFU;
    switch (color_format)
    {
        case kAbgr:
            return (byte0 << 24) | (byte3 << 16) | (byte2 << 8) | byte1;
        case kRgba:
            return (byte3 << 24) | (byte0 << 16) | (byte1 << 8) | byte2;
        case kBgra:
            return (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0;
        case kArgb:
        default:
            return value;
    }
}

void CalculateGlideTextureCoordinateExtent(std::uint32_t aspect_ratio,
                                           std::uint32_t* s_extent,
                                           std::uint32_t* t_extent)
{
    // The coordinate space is 256 along the longer axis whatever the LOD, and
    // GrAspectRatio_t shrinks the shorter one by the same power of two it
    // shrinks the texture edge: GR_ASPECT_8x1(0)..GR_ASPECT_1x1(3) are wide and
    // GR_ASPECT_1x2(4).. are tall.
    constexpr std::uint32_t kFullExtent = 256U;
    constexpr std::uint32_t kAspectSquare = 3U;
    constexpr std::uint32_t kMaxAspect = 6U;
    const std::uint32_t aspect =
        aspect_ratio <= kMaxAspect ? aspect_ratio : kAspectSquare;
    if (s_extent != nullptr)
    {
        *s_extent = kFullExtent >>
            (aspect > kAspectSquare ? aspect - kAspectSquare : 0U);
    }
    if (t_extent != nullptr)
    {
        *t_extent = kFullExtent >>
            (aspect < kAspectSquare ? kAspectSquare - aspect : 0U);
    }
}

bool CalculateGlideTextureMaxAddress(std::uint32_t texture_memory_bytes,
                                     std::uint32_t* maximum_address)
{
    constexpr std::uint32_t kTextureStartAlignment = 8U;
    if (maximum_address == nullptr ||
        texture_memory_bytes < kTextureStartAlignment ||
        texture_memory_bytes % kTextureStartAlignment != 0U)
    {
        return false;
    }
    *maximum_address = texture_memory_bytes - kTextureStartAlignment;
    return true;
}

bool CalculateGlideTextureMemoryRequired(std::uint32_t even_odd_mask,
                                       const GlideTextureInfo& info,
                                       std::uint32_t* required_bytes)
{
    constexpr std::uint32_t kEvenMask = 1U;
    constexpr std::uint32_t kOddMask = 2U;
    constexpr std::uint32_t kBothMask = kEvenMask | kOddMask;
    constexpr std::uint32_t kMaxLod = 8U;
    constexpr std::uint32_t kAspectSquare = 3U;
    constexpr std::uint32_t kMaxAspect = 6U;
    constexpr std::uint32_t kMaxFormat = 12U;
    constexpr std::uint32_t kTextureStartAlignment = 8U;
    // largeLod is the biggest mipmap and therefore the numerically smaller
    // GrLOD_t, so the valid ordering is large_lod <= small_lod (the reverse of
    // what this check previously required).
    if (required_bytes == nullptr || (even_odd_mask & ~kBothMask) != 0U ||
        even_odd_mask == 0U || info.large_lod > info.small_lod ||
        info.small_lod > kMaxLod || info.aspect_ratio > kMaxAspect ||
        info.format > kMaxFormat)
    {
        return false;
    }

    const std::uint32_t bytes_per_texel = info.format <= 5U ? 1U : 2U;
    std::uint64_t total = 0;
    // GrLOD_t counts downward in size: GR_LOD_256 is 0 and GR_LOD_1 is 8, so
    // largeLod is numerically <= smallLod and the LOD's larger edge is
    // 256 >> lod. The previous code used the LOD value directly as a log2 size,
    // which reported 8 bytes for a 256x256 texture -- and because the game sizes
    // its own TMU allocations from this answer, it then packed textures 8 bytes
    // apart. Reference: 3Dfx Glide 2.4 Reference Manual, grTexTextureMemRequired.
    constexpr std::int32_t kLargestLodLog2 = 8;
    for (std::uint32_t lod = info.large_lod; lod <= info.small_lod; ++lod)
    {
        const std::uint32_t lod_mask = (lod & 1U) == 0U ? kEvenMask : kOddMask;
        if ((even_odd_mask & lod_mask) == 0U)
        {
            continue;
        }
        const std::int32_t edge_log2 =
            kLargestLodLog2 - static_cast<std::int32_t>(lod);
        const std::int32_t aspect =
            static_cast<std::int32_t>(info.aspect_ratio);
        const std::int32_t width_log2 = edge_log2 -
            std::max(aspect - static_cast<std::int32_t>(kAspectSquare), 0);
        const std::int32_t height_log2 = edge_log2 -
            std::max(static_cast<std::int32_t>(kAspectSquare) - aspect, 0);
        const std::uint32_t width = 1U << std::max(width_log2, 0);
        const std::uint32_t height = 1U << std::max(height_log2, 0);
        total += static_cast<std::uint64_t>(width) * height * bytes_per_texel;
    }
    const std::uint64_t aligned = (total + kTextureStartAlignment - 1U) &
        ~(static_cast<std::uint64_t>(kTextureStartAlignment) - 1U);
    if (aligned > UINT32_MAX)
    {
        return false;
    }
    *required_bytes = static_cast<std::uint32_t>(aligned);
    return true;
}
}  // namespace repiu::hle
