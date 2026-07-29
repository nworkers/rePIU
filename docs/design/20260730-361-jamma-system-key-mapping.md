# JAMMA 시스템 키 매핑 수정 설계
# JAMMA System Key Mapping Correction Design

* 작성일 / Date: 2026-07-30 (Task 361)
* 대상 / Target: `src/platform/win32/io/port_io_emulator.cpp`
* 상태 / Status: 구현 및 검증 완료 / Implemented and verified

## 1. 배경
## 1. Background

현재 Win32 JAMMA HLE는 시스템 포트 `0x02A9`에서 F1을 active-low
`0x80`에 연결하고 이 비트를 `TEST/CLEAR`로 함께 취급한다. 실행 진단 결과 키
상태는 게스트까지 정상 전달되지만, PIU 입력 정의에서 TEST는 `0x02`, CLEAR는
`0x80`이므로 F1이 시스템 메뉴 진입 신호가 아니라 CLEAR 신호를 발생시킨다.

The current Win32 JAMMA HLE maps F1 to active-low `0x80` on system port
`0x02A9` and treats the bit as combined `TEST/CLEAR`. Runtime diagnostics prove
that the key state reaches the guest, but the PIU input definition assigns
`0x02` to TEST and `0x80` to CLEAR. F1 therefore emits CLEAR instead of the
system-menu TEST signal.

## 2. 설계
## 2. Design

시스템 키를 다음과 같이 분리한다. 모든 신호는 기존과 동일하게 released
`0xFF`, pressed 시 해당 비트를 0으로 내리는 active-low 방식이다.

Separate the system keys as follows. All signals retain the existing active-low
contract: `0xFF` while released and the corresponding bit cleared while pressed.

| 호스트 키 / Host key | 기능 / Function | `0x02A9` mask | 누름 값 / Pressed value |
|---|---|---:|---:|
| F1 | TEST | `0x02` | `0xFD` |
| F2 | SERVICE | `0x40` | `0xBF` |
| F3 | CLEAR | `0x80` | `0x7F` |
| F5 | COIN1 | `0x04` | `0xFB` |

`kJammaBitsSystem` 진단 이름표도 같은 마스크와 이름으로 갱신하여 로그가 HLE
내부 정책만 반복하지 않고 실제로 발생한 신호를 구분하도록 한다. 포트 주소,
다중 너비 IN 합성, EIP 전진 및 실시간 폴링 정책은 변경하지 않는다.

Update the `kJammaBitsSystem` diagnostic names to the same masks and labels so
logs distinguish the actual emitted signals. Port addresses, multi-width IN
composition, EIP advancement, and live polling behavior remain unchanged.

## 3. 검증
## 3. Verification

1. Win32 x86 Debug 빌드를 수행한다.
2. `pumpit1`을 실행하고 F1, F2, F3, F5를 합성 입력한다.
3. press/release 로그에서 각각 `0xFD`, `0xBF`, `0x7F`, `0xFB`가 나타나는지
   확인한다.
4. F1 입력 뒤 게스트가 시스템 메뉴 진입 경로를 시작하는지 확인한다.

1. Build the Win32 x86 Debug target.
2. Run `pumpit1` and inject F1, F2, F3, and F5.
3. Confirm press/release logs report `0xFD`, `0xBF`, `0x7F`, and `0xFB`
   respectively.
4. Confirm that F1 starts the guest system-menu transition.
