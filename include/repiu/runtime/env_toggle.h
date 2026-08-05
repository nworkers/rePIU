#ifndef REPIU_RUNTIME_ENV_TOGGLE_H_
#define REPIU_RUNTIME_ENV_TOGGLE_H_

namespace repiu::runtime
{

// Task 424: 환경 변수 하나로 기능을 켜고 끄는 두 가지 관례를 한곳에 모읍니다.
// 두 함수 모두 참 값은 `1`, `on`, `true` 세 가지뿐이고 대소문자 변환을 하지
// 않으며, 알 수 없는 값은 fail-closed로 OFF입니다. 차이는 미지정과 빈 값의
// 해석뿐입니다.
//
// Task 424 collects the two conventions for gating a feature on a single
// environment variable. Both accept only `1`, `on`, and `true` as true, neither
// folds case, and both treat any unrecognized value as a fail-closed OFF. They
// differ only in how they read an unset or empty value.

// A/B로 승격이 끝난 기능용입니다. 미지정과 빈 값은 ON이므로, 명시적인
// `0|off|false`만 진단을 위한 opt-out이 됩니다.
//
// For a feature already promoted by an A/B measurement: unset and empty mean
// ON, so only an explicit `0|off|false` opts out for diagnosis.
bool ResolvePromotedToggle(const char* value);

// 아직 기본값이 아닌 기능용입니다. 미지정과 빈 값은 OFF입니다.
//
// For a feature that is not yet a default: unset and empty mean OFF.
bool ResolveOptInToggle(const char* value);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_ENV_TOGGLE_H_
