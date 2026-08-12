#ifndef REPIU_TOOLS_AOT_PROBE_DOS_FILE_CREATE_PROBE_H_
#define REPIU_TOOLS_AOT_PROBE_DOS_FILE_CREATE_PROBE_H_

namespace repiu::tools
{

// Task 477: checks INT 21h AH=3Ch / AH=40h at the file-system layer -- creation
// and handle numbering, truncation, a missing parent, write offsets and cached
// size, a create/write/close/reopen round trip, the read-only attribute that
// must not reach the host file, and handle exhaustion.
bool RunDosFileCreateProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_AOT_PROBE_DOS_FILE_CREATE_PROBE_H_
