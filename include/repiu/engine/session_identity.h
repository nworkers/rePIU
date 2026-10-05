#ifndef REPIU_ENGINE_SESSION_IDENTITY_H_
#define REPIU_ENGINE_SESSION_IDENTITY_H_

#include <string>
#include <string_view>

namespace repiu::engine
{

// #15. Which target profile this run is, for what the in-game OSD shows. The
// loader writes it once, right after it settles the profile and before the
// execution thread or the Glide window exist, so readers never race the
// writer. The execution entry already takes many arguments; this keeps a
// display-only value out of them.
void SetSessionTargetProfile(std::string_view profile_id);

// The value written above, or "unknown" when nothing was written.
[[nodiscard]] std::string SessionTargetProfile();

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_SESSION_IDENTITY_H_
