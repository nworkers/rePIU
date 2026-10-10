#ifndef REPIU_ENGINE_SESSION_DISPLAY_PREFERENCES_H_
#define REPIU_ENGINE_SESSION_DISPLAY_PREFERENCES_H_

#include <functional>

namespace repiu::engine
{

// Issue #45. Where the Glide backend reports a display choice the operator
// made in the game (the OSD's fullscreen and keep-aspect checkboxes, Alt+Enter,
// a double click), so the loader can store it in cfg/repiu.ini. The engine
// knows no launcher types; it only calls this.
//
// The loader registers the sink once, before the execution thread or the Glide
// window exist, and keeps it for the rest of the process, so the backend's host
// thread never races the writer. The sink runs on that host thread. With no
// sink -- probes and tools -- a report does nothing.
using SessionDisplayPreferenceSink =
    std::function<void(bool fullscreen, bool keep_aspect)>;

void SetSessionDisplayPreferenceSink(SessionDisplayPreferenceSink sink);

// Calls the registered sink, if any, with the current values of both options.
void ReportSessionDisplayPreferences(bool fullscreen, bool keep_aspect);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_SESSION_DISPLAY_PREFERENCES_H_
