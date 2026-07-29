# 2P NumLock OFF 숫자패드 입력 작업 지시
# P2 NumLock-Off Numeric-Keypad Input Work Order

* 작업 번호 / Task: 362
* 작성일 / Date: 2026-07-30
* 설계 / Design: `docs/design/20260730-362-p2-numlock-off-keypad.md`

## 1. 목표
## 1. Objective

2P 입력 전체를 NumLock OFF 상태의 숫자패드 7, 9, 5, 1, 3으로 통일한다.

Make all P2 inputs use numeric-keypad 7, 9, 5, 1, and 3 while NumLock is off.

## 2. 작업 범위
## 2. Scope

1. P2 Center 입력을 `VK_NUMPAD5`에서 `VK_CLEAR`로 변경한다.
2. PIU I/O 분석 문서에 2P 숫자패드 배치를 기록한다.
3. Win32 x86 Debug 빌드를 수행한다.
4. NumLock OFF 상태의 숫자패드 5 스캔 코드로 실제 입력 경로를 검증한다.
5. 결과를 작업 로그에 기록한다.

1. Change P2 Center from `VK_NUMPAD5` to `VK_CLEAR`.
2. Record the P2 keypad layout in the PIU I/O analysis.
3. Build Win32 x86 Debug.
4. Verify the live input path with the numeric-keypad 5 scan code while
   NumLock is off.
5. Record the results in the work log.

## 3. 완료 조건
## 3. Completion Criteria

* NumLock OFF 숫자패드 5가 `0x02AA`의 `0x04` 비트를 active-low로 구동한다.
* 2P의 나머지 네 방향 매핑은 유지된다.
* F3 시스템 CLEAR 입력과 혼동되지 않는다.
* 빌드와 실제 입력 로그 검증이 성공한다.

* NumLock-off numeric-keypad 5 drives bit `0x04` of `0x02AA` active-low.
* The other four P2 directional mappings remain unchanged.
* The mapping remains distinct from the F3 system CLEAR input.
* Build and live input-log verification succeed.
