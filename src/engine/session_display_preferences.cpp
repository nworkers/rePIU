#include "repiu/engine/session_display_preferences.h"

#include <utility>

namespace repiu::engine
{
namespace
{

SessionDisplayPreferenceSink& SinkStorage()
{
    static SessionDisplayPreferenceSink sink;
    return sink;
}

}  // namespace

void SetSessionDisplayPreferenceSink(SessionDisplayPreferenceSink sink)
{
    SinkStorage() = std::move(sink);
}

void ReportSessionDisplayPreferences(bool fullscreen, bool keep_aspect)
{
    const SessionDisplayPreferenceSink& sink = SinkStorage();
    if (sink)
    {
        sink(fullscreen, keep_aspect);
    }
}

}  // namespace repiu::engine
