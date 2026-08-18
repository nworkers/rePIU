# Task 494: 2P 숫자패드 별칭 완성

## 문제

Task 493 사용자 로그에서 2P Center(`5`)는 정상 기록됐지만 `7`과 `9`는 guest transition이
한 건도 없었습니다. SDL3는 숫자패드 위치와 navigation key를 별도 keycode로 정의합니다.
현재 event mapping은 `HOME/PAGEUP/END/PAGEDOWN`만 처리하고 `KP_7/KP_9/KP_1/KP_3`을
처리하지 않습니다. Win32 live/초기 snapshot도 `VK_NUMPAD*` 별칭이 없습니다.

## 설계

물리적으로 같은 2P 패널 위치에 해당하는 navigation key와 numeric keypad key를 동일한
`JammaInputKey`로 정규화합니다.

| 위치 | navigation | numeric keypad | JAMMA |
|---|---|---|---|
| 좌상 | Home | KP 7 | P2 UpLeft |
| 우상 | PageUp | KP 9 | P2 UpRight |
| 중앙 | Clear | KP 5 | P2 Center |
| 좌하 | End | KP 1 | P2 DownLeft |
| 우하 | PageDown | KP 3 | P2 DownRight |

SDL event history, Win32 초기 pressed mask, replay 밖 live fallback에 같은 별칭 집합을
적용합니다. due-time replay와 원본 JAMMA port bit 배치는 변경하지 않습니다.

## 검증

Win32 Debug 빌드와 기존 timeline/전체 AOT probe를 통과시킵니다. 최종 검증은 NumLock
ON/OFF 각각에서 2P `7/9/5/1/3` press/release가 종료 로그에 나타나는지 확인합니다.

---

# Task 494: Completing 2P Numpad Aliases

## Problem

The Task 493 user log recorded 2P Center (`5`) but no guest transition for `7` or `9`. SDL3 assigns
separate keycodes to keypad positions and navigation keys. The event mapping handled only
`HOME/PAGEUP/END/PAGEDOWN`, while Win32 live and initial snapshots also lacked `VK_NUMPAD*`
aliases.

## Design

Normalize navigation and numeric-keypad aliases for the same physical 2P panel position into one
`JammaInputKey`, as shown in the table above. Apply the same alias set to SDL event history, the
initial Win32 pressed mask, and the live fallback outside replay. Due-time replay and the original
JAMMA port bit layout remain unchanged.

## Verification

Run Win32 Debug builds and the existing timeline and complete AOT probes. Final validation checks
that 2P `7/9/5/1/3` press/release transitions appear with NumLock both on and off.
