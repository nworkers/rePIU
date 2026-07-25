# 20260726-301 작업 지시: 타이머 pending 안전 VEH 경계 전달 / Work order

설계: [20260726-301-timer-pending-safe-veh-boundary.md](../design/20260726-301-timer-pending-safe-veh-boundary.md)

## 한국어

### 목표

poll thread의 강제 TF rendezvous를 제거하고, pending IRQ0를 기존 guest-thread VEH 경계에서
전달하여 반복되는 미처리 `EXCEPTION_SINGLE_STEP` 종료를 없앱니다.

### 구현 순서

1. poll loop에서 guest thread suspend/context/TF 조작을 제거합니다.
2. `timer_interrupt_wakeup_armed` 상태와 VEH wakeup 전용 분기를 제거합니다.
3. 공용 single-step 처리에서 HLE/AOT 상태를 정리한 뒤 native 실행 재진입 전에
   `InjectPendingInterrupts`를 호출합니다.
4. 주입으로 EIP가 ISR로 바뀌면 그 경계 처리를 즉시 끝냅니다.
5. 관련 analysis를 갱신합니다.
6. Win32 x86 Debug 빌드와 장시간 격리 실행으로 검증합니다.
7. 결과와 제한을 작업 로그에 기록하고 커밋합니다.

### 완료 조건

- 코드에 timer wakeup용 `SetThreadContext`/TF arming이 남지 않습니다.
- `Armed INT 8 VEH wakeup` 로그가 0건입니다.
- 이전 125초 지점을 넘어 progress와 INT 8 chain이 증가합니다.
- 새 `0x80000004` APPCRASH가 없습니다.

---

## English

### Objective and steps

Remove poll-thread TF rendezvous and deliver pending IRQ0 at existing
guest-thread VEH boundaries. Delete wakeup state and handling, invoke the
common injector after HLE/AOT reconciliation and before native re-entry, then
build and run beyond the former 125-second crash frontier.

Completion requires zero timer wakeup arms, continuing progress and INT 8
chain completion, and no new Windows `0x80000004` APPCRASH.
