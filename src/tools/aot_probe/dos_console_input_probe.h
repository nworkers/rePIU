#ifndef REPIU_TOOLS_AOT_PROBE_DOS_CONSOLE_INPUT_PROBE_H_
#define REPIU_TOOLS_AOT_PROBE_DOS_CONSOLE_INPUT_PROBE_H_

namespace repiu::tools
{

// Task 764: checks INT 21h AH=07h / AH=08h against the BIOS keyboard buffer --
// an ordinary key in AL with EIP advanced, an extended key as a zero followed
// by its scan code, an empty buffer leaving EIP on the `int 21h` and counting
// the wait, and the same service reached through the traced dispatcher.
bool RunDosConsoleInputProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_AOT_PROBE_DOS_CONSOLE_INPUT_PROBE_H_
