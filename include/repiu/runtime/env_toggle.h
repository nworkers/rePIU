#ifndef REPIU_RUNTIME_ENV_TOGGLE_H_
#define REPIU_RUNTIME_ENV_TOGGLE_H_

namespace repiu::runtime
{

// Task 424 collects the two conventions for gating a feature on a single
// environment variable. Both accept only `1`, `on`, and `true` as true, neither
// folds case, and both treat any unrecognized value as a fail-closed OFF. They
// differ only in how they read an unset or empty value.

// For a feature already promoted by an A/B measurement: unset and empty mean
// ON, so only an explicit `0|off|false` opts out for diagnosis.
bool ResolvePromotedToggle(const char* value);

// For a feature that is not yet a default: unset and empty mean OFF.
bool ResolveOptInToggle(const char* value);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_ENV_TOGGLE_H_
