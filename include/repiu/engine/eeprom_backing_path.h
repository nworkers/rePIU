#ifndef REPIU_PLATFORM_WIN32_EEPROM_BACKING_PATH_H_
#define REPIU_PLATFORM_WIN32_EEPROM_BACKING_PATH_H_

#include <string>

namespace repiu::engine
{

// Points the 93C46 EEPROM at its backing file for this run.
//
// Declared here rather than in port_io_emulator.h so the host entry point can
// call it: that header pulls in ThreadContext and the whole execution-layer
// include path with it, none of which the entry point needs to name a file.
//
// Must be called before the guest executes. The device is created lazily on
// the first EEPROM port access, and once it has loaded an image the path
// cannot move -- its destructor would save this run's contents over a
// different file. A late call is ignored and reported rather than silently
// doing nothing.
void SetEepromBackingPath(const std::string& path);

}  // namespace repiu::engine

#endif  // REPIU_PLATFORM_WIN32_EEPROM_BACKING_PATH_H_
