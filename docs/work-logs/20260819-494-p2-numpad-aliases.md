# Task 494 작업 로그: 2P 숫자패드 별칭

## 결과

Task 493 사용자 로그에서 2P `7/9`가 guest에 도달하지 않은 원인을 NumLock numeric
keycode 별칭 누락으로 확인하고 다섯 2P 위치를 모두 보완했습니다.

- SDL: `KP7/9/5/1/3`과 `Home/PageUp/Clear/End/PageDown`을 각각 같은 key로 정규화
- 초기 snapshot: `VK_NUMPAD7/9/5/1/3` 추가
- live fallback: navigation과 numeric virtual key를 모두 조회
- constexpr SDL 변환과 10개 static assertion으로 alias matrix 고정

## 검증

- Win32 x86 Debug `repiu`, `repiu_aot_probe`: 성공
- `--jamma-input-timeline`: 성공
- pumpit1 전체 AOT probe: `valid=true`, `timer_tick_delivery_all=true`,
  `jamma_input_timeline_probe=true`, `coherence_all=true`
- 후속 사용자 실행에서 UpLeft/UpRight/Center/DownLeft/DownRight press/release가 각각
  `77/77`, `97/97`, `124/124`, `107/107`, `91/91`로 균형을 이뤄 검증 완료

---

# Task 494 Work Log: 2P Numpad Aliases

## Result

The Task 493 user log showed that 2P `7/9` never reached the guest. The cause was missing NumLock
numeric-keycode aliases, and all five 2P positions are now covered.

- SDL normalizes `KP7/9/5/1/3` with `Home/PageUp/Clear/End/PageDown` respectively.
- Initial snapshot capture includes `VK_NUMPAD7/9/5/1/3`.
- The live fallback queries both navigation and numeric virtual keys.
- A constexpr SDL conversion and ten static assertions pin the alias matrix.

## Verification

- Win32 x86 Debug `repiu` and `repiu_aot_probe`: passed.
- `--jamma-input-timeline`: passed.
- Complete pumpit1 AOT probe reported `valid=true`, `timer_tick_delivery_all=true`,
  `jamma_input_timeline_probe=true`, and `coherence_all=true`.
- A subsequent user run completed validation with balanced press/release counts for UpLeft,
  UpRight, Center, DownLeft, and DownRight: `77/77`, `97/97`, `124/124`, `107/107`, and `91/91`.
