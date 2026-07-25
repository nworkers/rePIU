# 20260726-297 작업 지시: 타이머 주입 IF 게이트 및 중첩 방지 / Work order

설계: [docs/design/20260726-297-timer-injection-if-gate-nesting.md](../design/20260726-297-timer-injection-if-gate-nesting.md)

## 한국어

### 목표
INT 8h 타이머 주입을 게스트 IF 상태에 맞춰(하드웨어 충실) 게이트하여, 게스트 스택/EIP 손상(연속 크래시의
근본 원인)을 제거한다.

### 선결 (선택, 권장)
- **A. 인과 확정:** 선점 주입을 일시 비활성화하고 aot-dbt로 구동 → 296류 크래시가 사라지는지 확인.
  범인이 확정되면 되돌리고 B 착수. (사용자 확인 후 최소 1회 실행)

### 구현 항목
1. `src/platform/win32/execution/execution_trampoline.cpp` — `InjectPendingInterrupts` (2256~):
   - 기존 EIP 게이트(2270) 통과 후, `timer_interrupt_pending`을 클리어하기 **전에**
     `if ((win32_context->EFlags & 0x200U) == 0U) { return; }` 추가 → IF=0이면 pending 유지·지연.
2. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` — 선점 경로 (294~340):
   - EIP 게이트(294) 통과 후 프레임 쓰기/`SetThreadContext`를 `if ((win32_context.EFlags & 0x200U) != 0U)`로 감싼다.
   - IF=0이면 아무것도 하지 않고 `preemptive_injected=false` 유지 → 기존 `timer_interrupt_pending=true`로 지연.
3. (선택) `0x200U`를 `kEFlagsInterruptEnable` 상수로 명명(execution_internal.h 또는 인접).

### 검증
1. `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32` 성공.
2. aot-dbt `pumpit1`: 296류 RET/`0x287` 크래시 소멸 또는 진행 향상, `malformed count` 관찰.
3. `REPIU_TIMER_INJECT_LOG=1`: 주입 프레임 ESP가 ISR 사이클마다 단조 복원(연속 하강 소멸).
4. busy-wait 유지: glide gate 수 ≥ 74로 계속 진행.
- 실행 검증은 최소·사용자 확인 후.

### 범위 밖
AOT 리턴 예측, 게스트 스택 TIB 정합, traced 핸들러 Eip 가드.

## English

### Goal
Gate INT 8h timer injection on the guest IF flag (hardware-faithful) to remove the guest stack/EIP
corruption that is the root cause of the recurring crashes.

### Precondition (optional, recommended)
A. Confirm causation by temporarily disabling preemptive injection and running aot-dbt; if the 296-era
crashes vanish, revert and proceed with B. (Minimal, user-confirmed run.)

### Items
1. `InjectPendingInterrupts`: after the EIP gate and **before** clearing `timer_interrupt_pending`, add
   `if ((EFlags & 0x200) == 0) return;` so IF=0 defers with pending retained.
2. Preemptive path: wrap the frame write / `SetThreadContext` in `if ((EFlags & 0x200) != 0)`; on IF=0 do
   nothing so the existing `timer_interrupt_pending=true` defers it.
3. (Optional) name the `0x200` constant `kEFlagsInterruptEnable`.

### Verification
Build succeeds; 296-era crashes gone / progress improved; no nesting (per-ISR ESP restore under
`REPIU_TIMER_INJECT_LOG=1`); busy-wait escape preserved (glide gates ≥ 74). Keep runs minimal and
user-confirmed.

### Out of scope
AOT return prediction, guest-stack TIB alignment, traced-handler Eip guards.
