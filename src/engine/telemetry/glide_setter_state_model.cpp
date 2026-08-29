#include "repiu/engine/glide_setter_state_model.h"

#include <algorithm>

namespace repiu::engine
{
namespace
{

using go = repiu::hle::GlideGateId;

}  // namespace

bool GlideSetterStateKeysEqual(const GlideSetterStateKey& left,
                               const GlideSetterStateKey& right)
{
    if (left.word_count != right.word_count ||
        left.texture_generation != right.texture_generation)
    {
        return false;
    }
    for (std::uint32_t index = 0; index < left.word_count; ++index)
    {
        if (left.words[index] != right.words[index])
        {
            return false;
        }
    }
    return true;
}

GlideSetterStateKey BuildGlideSetterStateKey(
    const std::uint32_t* argument_words,
    std::uint32_t argument_word_count,
    std::uint32_t texture_generation)
{
    GlideSetterStateKey key;
    key.texture_generation = texture_generation;
    if (argument_words == nullptr)
    {
        return key;
    }
    key.word_count = std::min<std::uint32_t>(
        argument_word_count,
        static_cast<std::uint32_t>(kGlideSetterStateKeyWords));
    for (std::uint32_t index = 0; index < key.word_count; ++index)
    {
        key.words[index] = argument_words[index];
    }
    return key;
}

bool IsGlideSetterStateGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrColorMask:
        case go::kGrDepthMask:
        case go::kGrDepthBufferMode:
        case go::kGrDepthBufferFunction:
        case go::kGrDepthBiasLevel:
        case go::kGrRenderBuffer:
        case go::kGrAlphaBlendFunction:
        case go::kGrAlphaTestFunction:
        case go::kGrAlphaTestReferenceValue:
        case go::kGrAlphaCombine:
        case go::kGrColorCombine:
        case go::kGrConstantColorValue:
        case go::kGrConstantColorValue4:
        case go::kGrFogMode:
        case go::kGrFogColorValue:
        case go::kGrClipWindow:
        case go::kGrCullMode:
        case go::kGrDitherMode:
        case go::kGrChromaKeyMode:
        case go::kGrChromaKeyValue:
        case go::kGrTexCombine:
        case go::kGrTexCombineFunction:
        case go::kGrTexClampMode:
        case go::kGrTexFilterMode:
        case go::kGrTexMipMapMode:
        case go::kGrTexLodBiasValue:
        case go::kGrTexSource:
        case go::kGrLfbWriteColorFormat:
        case go::kGrLfbWriteColorSwizzle:
        case go::kGrLfbConstantAlpha:
        case go::kGrLfbConstantDepth:
            return true;
        default:
            return false;
    }
}

bool IsGlideSetterStateInvalidatingGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrGlideInit:
        case go::kGrGlideShutdown:
        case go::kGrGlideSetState:
        case go::kGrSstWinOpen:
        case go::kGrSstWinClose:
        // The draw buffer switch is a state-restore boundary in the same sense:
        // an applied mask may not be assumed to survive it.
        case go::kGrRenderBuffer:
            return true;
        default:
            return false;
    }
}

bool IsGlideSetterStateTextureGenerationGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrTexDownloadMipMap:
        case go::kGrTexDownloadMipMapLevel:
        case go::kGrTexDownloadMipMapLevelPartial:
        case go::kGrTexDownloadTable:
        case go::kGrTexDownloadTablePartial:
        case go::kGrTexNccTable:
            return true;
        default:
            return false;
    }
}

bool IsGlideSetterStateTextureDependentGate(repiu::hle::GlideGateId gate_id)
{
    switch (gate_id)
    {
        case go::kGrTexSource:
        case go::kGrTexCombine:
        case go::kGrTexCombineFunction:
        case go::kGrTexClampMode:
        case go::kGrTexFilterMode:
        case go::kGrTexMipMapMode:
        case go::kGrTexLodBiasValue:
            return true;
        default:
            return false;
    }
}

}  // namespace repiu::engine
