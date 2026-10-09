#ifndef REPIU_TOOLS_AOT_PROBE_MESA_FX_TEXTURE_SOURCE_PROBE_H_
#define REPIU_TOOLS_AOT_PROBE_MESA_FX_TEXTURE_SOURCE_PROBE_H_

namespace repiu::tools
{

// Issue #37. Finding the game's 8-bit texture originals through fake Mesa
// structures: the link checks, the upscale, the verification and every
// fallback, for 4444 and 565.
bool RunMesaFxTextureSourceProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_AOT_PROBE_MESA_FX_TEXTURE_SOURCE_PROBE_H_
