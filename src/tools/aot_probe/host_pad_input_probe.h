#ifndef REPIU_TOOLS_AOT_PROBE_HOST_PAD_INPUT_PROBE_H_
#define REPIU_TOOLS_AOT_PROBE_HOST_PAD_INPUT_PROBE_H_

namespace repiu::tools
{

// Issue #34. Gamepad and joystick names, the defaults, the mapping from pad
// state to JAMMA inputs, and device numbering -- all without a device.
bool RunHostPadInputProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_AOT_PROBE_HOST_PAD_INPUT_PROBE_H_
