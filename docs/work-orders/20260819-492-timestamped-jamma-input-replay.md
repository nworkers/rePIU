# Task 492 작업 지시 — 타임스탬프 기반 JAMMA 입력 재생

설계: [20260819-492](../design/20260819-492-timestamped-jamma-input-replay.md)

## 한국어

### 목표

timer backlog를 소진할 때 각 원본 ISR이 해당 PIT tick의 due 시각 입력 상태를 관측하게
하여 press/release 지연, 누락, 잘못된 guest tick 귀속을 제거합니다.

### 작업 항목

- [x] SDL key down/up과 focus loss를 JAMMA pressed-mask timeline에 기록합니다.
- [x] `PitIrqSchedule`이 due tick ordinal과 epoch를 제공하게 합니다.
- [x] delivery accounting과 동일한 범위의 due timestamp를 큐에 넣고 주입 시 하나씩
      소비합니다.
- [x] timer ISR JAMMA 읽기만 due-time replay를 사용하고 일반 읽기는 기존 live snapshot을
      유지합니다.
- [x] replay가 tick별로 Task 403 cache를 우회하도록 합니다.
- [x] PIT, timeline, timer accounting probe를 작성·갱신합니다.
- [x] 관련 아키텍처, 분석, TODO와 작업 로그를 갱신합니다.

### 검증

- Win32 x86 Debug `repiu`, `repiu_aot_probe` 빌드
- `repiu_aot_probe --jamma-input-timeline` 실행
- 저작물 DOS4GW fixture가 있으면 전체 `repiu_aot_probe` 실행
- `git diff --check`
- 실제 key timing은 Task 495 최종 사용자 실행에서 다섯 2P 위치의 균형 잡힌 transition과
  timeline loss counter 0으로 확인했습니다.

---

## English

### Objective

Make every recovered original ISR observe input at its PIT tick's due time, eliminating delayed,
lost, or guest-tick-misattributed press/release edges while a timer backlog drains.

### Work items

- [x] Record SDL key down/up and focus loss into a JAMMA pressed-mask timeline.
- [x] Extend `PitIrqSchedule` to expose the due tick ordinal and epoch.
- [x] Queue exactly the due timestamps retained by delivery accounting and consume one per
      injection.
- [x] Use due-time replay only for timer-ISR JAMMA reads; retain the live snapshot elsewhere.
- [x] Bypass the Task 403 cache per replayed tick.
- [x] Add or update PIT, timeline, and timer-accounting probes.
- [x] Update architecture, analysis, TODO, and the work log.

### Verification

- Build Win32 x86 Debug `repiu` and `repiu_aot_probe`.
- Run `repiu_aot_probe --jamma-input-timeline`.
- Run the complete suite when the copyrighted DOS4GW fixture is available.
- Run `git diff --check`.
- The final Task 495 user run confirmed balanced transitions for all five 2P positions and zero
  timeline loss counters.
