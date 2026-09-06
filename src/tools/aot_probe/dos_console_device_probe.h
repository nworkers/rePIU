#ifndef REPIU_TOOLS_AOT_PROBE_DOS_CONSOLE_DEVICE_PROBE_H_
#define REPIU_TOOLS_AOT_PROBE_DOS_CONSOLE_DEVICE_PROBE_H_

namespace repiu::tools
{

// Task 608: checks that DOS CON opens as a character-device handle without
// creating a host regular file.
bool RunDosConsoleDeviceProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_AOT_PROBE_DOS_CONSOLE_DEVICE_PROBE_H_
