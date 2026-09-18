# Task 715 작업 지시 — 게스트 시계를 벽시계와 함께 기록하기

설계: [20260918-715](../design/20260918-715-guest-clock-sampling.md)

## 범위

게스트의 틱 카운터를 벽시계와 함께 두 host에서 기록해, Linux x64 게임 시계가 느린지
판정한다. 코드 변경은 선택 진단 하나다.

## 단계

1. `REPIU_LIVE_GUEST_PEEK` — live telemetry 옆에 게스트 dword를 기록
2. 재배치된 ISR 코드에서 카운터 주소 확인
3. 두 host 30초, Linux opt-in 주입 30초 기록
4. 작업 로그, frontier

## 검증

* 두 host core probe
* 설정하지 않았을 때 출력 변화 없음

---

## English

Design: [20260918-715](../design/20260918-715-guest-clock-sampling.md)

### Scope

Record the guest's tick counter against wall time on both hosts to judge whether the
Linux x64 game clock is slow. The only code change is one optional diagnostic.

### Steps

1. `REPIU_LIVE_GUEST_PEEK`, logging guest dwords beside live telemetry.
2. Confirm the counter's address from the relocated ISR code.
3. Record 30 seconds on both hosts, and 30 seconds on Linux with opt-in injection.
4. Work log and frontier.

### Verification

* Core probes on both hosts.
* No change in output when unset.
