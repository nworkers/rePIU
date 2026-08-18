# Task 492 작업 로그 — 타임스탬프 기반 JAMMA 입력 재생

설계: [20260819-492](../design/20260819-492-timestamped-jamma-input-replay.md) ·
작업 지시: [20260819-492](../work-orders/20260819-492-timestamped-jamma-input-replay.md)

## 한국어

### 결과

timer backlog가 더 이상 전달 순간의 현재 키 상태 하나를 모든 overdue ISR에 복제하지
않습니다. SDL press/release timestamp와 PIT tick의 원래 due timestamp를 연결하고, 원본
INT 8 ISR의 JAMMA 읽기가 해당 시각의 pressed mask를 복원하게 했습니다. ISR 밖의 입력은
기존 `GetAsyncKeyState`/500 us snapshot을 그대로 사용합니다.

### 구현

- `Win32JammaInputTimeline`은 256개 full-state edge history와 기존 backlog cap에 맞춘 64개
  due-time queue를 고정 배열로 관리합니다.
- SDL key down/up은 event의 `SDL_GetTicksNS()` clock timestamp를 보존하고 repeat를
  무시합니다. focus loss는 전체 release로 기록합니다.
- `PitIrqSchedule::Poll`은 기존 count와 함께 epoch, 첫 due ordinal, divisor를 제공합니다.
  delivery accounting이 수락한 tick에만 absolute due timestamp를 만듭니다.
- delivery counter와 due queue는 같은 allocation-free spin guard 안에서 수락·소비되어
  서로 어긋나지 않습니다.
- INT 8 주입은 due timestamp와 interrupt-frame ESP를 활성화합니다. IF=0이고 frame 아래인
  JAMMA 읽기만 history를 사용하며 Task 403 cache를 우회합니다. IRETD 뒤에는 live path로
  복귀합니다.
- 종료 로그에 `edges/history-overflow/due/due-overflow/replays/replay-reads/missing-due`를
  추가했습니다. timeout 강제 종료 뒤 snapshot은 guard를 취하지 않습니다.

### 검증

- Win32 x86 Debug `repiu`: 성공
- Win32 x86 Debug `repiu_aot_probe`: 성공
- `repiu_aot_probe --jamma-input-timeline`: 성공
  - `pit_timer_probe=true, divisor=4972, frequency_hz=240`
  - `timer_tick_delivery_all=true`
  - `jamma_input_timeline_probe=true, edges=3, replays=4, reads=3, missing=1`
  - `missing=1`은 빈 due queue fail-safe를 의도적으로 확인한 probe case입니다.
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: 전체 probe 성공
- `git diff --check`: 오류 없음. 기존 checkout 정책의 LF→CRLF 경고만 있습니다.

합성 due tick은 press 전, press와 release 사이, release 후의 세 시각에서 각각 idle,
pressed, released를 복원했습니다. ISR 종료를 나타내는 IF 복구 뒤에는 replay가 폐기되는
것도 확인했습니다.

### 후속 사용자 검증 완료

Task 493~495 보정 뒤의 120초 사용자 실행에서 1,006개 edge와 다섯 2P 위치의 균형 잡힌
press/release를 확인했습니다. `replay-reads=275350`, `due-overflow=0`, `missing-due=0`으로
실제 OS/SDL 입력이 due-time replay에 소비됐습니다. 최종 근거는
[Task 495 작업 로그](20260819-495-jamma-history-safe-pruning.md)에 누적했습니다.

---

## English

### Result

Timer backlog no longer duplicates one delivery-time key state across every overdue ISR. SDL
press/release timestamps are joined to each PIT tick's original due timestamp, and JAMMA reads in
the original INT 8 ISR reconstruct the pressed mask at that time. Reads outside the ISR retain the
existing `GetAsyncKeyState` and 500 us snapshot path.

### Implementation

- `Win32JammaInputTimeline` uses fixed arrays for 256 full-state history edges and 64 due times,
  matching the existing backlog cap.
- SDL key down/up preserves the event's `SDL_GetTicksNS()` clock timestamp, ignores repeats, and
  records focus loss as a full release.
- `PitIrqSchedule::Poll` exposes the epoch, first due ordinal, and divisor alongside its existing
  count. Absolute due timestamps are created only for ticks retained by delivery accounting.
- One allocation-free spin guard keeps delivery counters and due-queue acceptance/consumption in
  sync.
- INT 8 injection activates the due timestamp and interrupt-frame ESP. Only JAMMA reads with IF
  clear below that frame use history and bypass Task 403 caching; IRETD restoration returns to the
  live path.
- Final logging now reports `edges/history-overflow/due/due-overflow/replays/replay-reads/missing-due`.
  Post-timeout snapshots acquire no guard.

### Verification

- Win32 x86 Debug `repiu`: passed.
- Win32 x86 Debug `repiu_aot_probe`: passed.
- `repiu_aot_probe --jamma-input-timeline`: passed:
  - `pit_timer_probe=true, divisor=4972, frequency_hz=240`
  - `timer_tick_delivery_all=true`
  - `jamma_input_timeline_probe=true, edges=3, replays=4, reads=3, missing=1`
  - `missing=1` is the intentional empty-due fail-safe probe case.
- Complete `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: passed.
- `git diff --check`: no errors; only the existing checkout policy's LF-to-CRLF warnings.

Synthetic due ticks before the press, between press and release, and after release reconstructed
idle, pressed, and released respectively. Restoring IF to model ISR completion also retired replay.

### Subsequent user verification complete

After the Task 493 through 495 corrections, a 120-second user run confirmed 1,006 edges and
balanced press/release counts for all five 2P positions. `replay-reads=275350`,
`due-overflow=0`, and `missing-due=0` show that real OS/SDL input was consumed by due-time replay.
The final evidence is accumulated in the
[Task 495 work log](20260819-495-jamma-history-safe-pruning.md).
