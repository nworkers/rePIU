# 20260726-299 작업 지시: 타이머 선점 VEH rendezvous / Work order

설계: [docs/design/20260726-299-timer-preemption-veh-rendezvous.md](../design/20260726-299-timer-preemption-veh-rendezvous.md)

## 한국어

### 목표

poll thread의 직접 INT 8 frame/context 변조를 제거하고, TF wakeup으로 guest
thread의 VEH 경계에서 기존 공통 주입기를 실행합니다.

### 구현 항목

1. `ThreadContext`에 atomic wakeup armed 상태를 추가합니다.
2. `PollThreadUntilExit`에서 guest memory 및 ESP/EIP/CS 변경을 제거하고, EIP/IF/TF
   조건을 확인해 pending/armed 설정 후 TF만 변경합니다.
3. `SetThreadContext` 실패 시 armed를 해제하고 pending은 유지합니다.
4. `DispatchGuestException`에서 armed single-step을 기존 handler보다 먼저 소비하고
   wakeup TF를 제거한 뒤 `InjectPendingInterrupts`를 호출합니다.
5. 다른 예외가 먼저 도착하면 wakeup 상태와 TF만 해제하고 정상 dispatch를
   계속합니다.
6. opt-in timer 로그에 arm source EIP/ESP를 남기고 관련 analysis를 갱신합니다.

### 검증

1. `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32`
2. 격리 EEPROM과 제한 timeout으로 `aot-dbt` 한 번 실행
3. arm → VEH injection 대응, 직접 preemptive frame write 0건 확인
4. exception/malformed/return target과 INT 8 chain/Glide 진행 확인
5. `git diff --check`

### 안전 조건

- 사용자 소유 `repiu_log.txt`는 수정하거나 commit하지 않습니다.
- 원본 guest 코드 및 ISR을 수정하지 않습니다.
- 기존 TF가 설정된 컨텍스트에는 wakeup을 arm하지 않습니다.
- 실행 검증은 격리된 복사본과 제한 timeout을 사용합니다.

---

## English

Remove direct poll-thread mutation of the INT 8 frame and execution context.
Use a TF wakeup to run the existing common injector at a guest-thread VEH
boundary. Add atomic wakeup state, preserve pending on failure, disarm safely
if another exception arrives first, and log the arm source only when timer
logging is enabled.

Build Win32 x86 Debug, run one bounded isolated `aot-dbt` smoke test, verify
arm-to-VEH injection correspondence and zero direct preemptive frame writes,
then inspect exception, return, INT 8 chain, and Glide progress summaries.
