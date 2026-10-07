#ifndef REPIU_TOOLS_SEGMENT_RESTORE_PROBE_H_
#define REPIU_TOOLS_SEGMENT_RESTORE_PROBE_H_

namespace repiu::tools
{

// Task i018. Establishes what the physical ES register actually holds after
// a fault-handler resume whose context carries an arbitrary selector in
// SegEs. The engine's HLE segment load writes the guest's selector (0x0024
// and friends) into the resumed context, and the guarded AOT slots compare
// the physical register against that selector -- whether a guard can ever
// succeed therefore depends on what the host OS restores, which has never
// been established. Prints one line per candidate selector and returns true
// when the mechanics held (the control selector came back unchanged and no
// candidate needed an unexpected recovery).
bool RunSegmentRestoreProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_SEGMENT_RESTORE_PROBE_H_
