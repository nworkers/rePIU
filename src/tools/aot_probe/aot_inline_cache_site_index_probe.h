#pragma once

namespace repiu::tools
{

// Task 479: checks that the inline-cache site index answers exactly what the
// linear scan it replaces answered, and that a stale index falls back.
bool RunAotInlineCacheSiteIndexProbe();

}  // namespace repiu::tools
