# 2P NumLock OFF 숫자패드 입력 설계
# P2 NumLock-Off Numeric-Keypad Input Design

* 작성일 / Date: 2026-07-30 (Task 362)
* 대상 / Target: `src/platform/win32/io/port_io_emulator.cpp`
* 상태 / Status: 구현 및 검증 완료 / Implemented and verified

## 1. 배경
## 1. Background

2P 방향 입력은 이미 NumLock OFF 상태에서 숫자패드 7, 9, 1, 3이 발생시키는
Win32 탐색 가상키 `VK_HOME`, `VK_PRIOR`, `VK_END`, `VK_NEXT`를 사용한다.
그러나 중앙 입력만 NumLock ON 상태의 `VK_NUMPAD5`를 사용해 NumLock 상태가
방향 입력과 일치하지 않는다.

P2 directional inputs already use the Win32 navigation virtual keys generated
by numeric-keypad 7, 9, 1, and 3 while NumLock is off: `VK_HOME`, `VK_PRIOR`,
`VK_END`, and `VK_NEXT`. The center input alone uses the NumLock-on
`VK_NUMPAD5`, making its NumLock state inconsistent with the directions.

## 2. 설계
## 2. Design

2P 중앙 입력을 `VK_CLEAR`로 변경한다. Win32에서 NumLock OFF 상태의 숫자패드
5는 `VK_CLEAR`를 발생시킨다. 여기서 `VK_CLEAR`는 키보드 가상키 이름이며,
F3에 연결된 시스템 포트의 CLEAR 기능과는 별개다.

Change the P2 center input to `VK_CLEAR`. On Win32, numeric-keypad 5 emits
`VK_CLEAR` while NumLock is off. `VK_CLEAR` is the keyboard virtual-key name
and is unrelated to the system-port CLEAR function mapped to F3.

| 숫자패드 / Keypad | 위치 / Position | Win32 가상키 / Virtual key | `0x02AA` mask |
|---:|---|---|---:|
| 7 | P2 Top-Left | `VK_HOME` | `0x01` |
| 9 | P2 Top-Right | `VK_PRIOR` | `0x02` |
| 5 | P2 Center | `VK_CLEAR` | `0x04` |
| 1 | P2 Bottom-Left | `VK_END` | `0x08` |
| 3 | P2 Bottom-Right | `VK_NEXT` | `0x10` |

## 3. 검증
## 3. Verification

1. Win32 x86 Debug 빌드를 수행한다.
2. NumLock을 끈 상태에서 숫자패드 5의 물리 스캔 코드를 합성 입력한다.
3. `P2-Center PRESSED port=0x02AA value=0xFB`와 release `0xFF`를 확인한다.
4. 검증 후 호스트의 원래 NumLock 상태를 복원한다.

1. Build the Win32 x86 Debug target.
2. Inject the physical numeric-keypad 5 scan code while NumLock is off.
3. Confirm `P2-Center PRESSED port=0x02AA value=0xFB` and release `0xFF`.
4. Restore the host's original NumLock state after verification.
