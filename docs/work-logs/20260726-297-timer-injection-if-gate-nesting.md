# 20260726-297 작업 로그: 타이머 주입 IF 게이트 및 중첩 방지 / Work log

설계: [docs/design/20260726-297-timer-injection-if-gate-nesting.md](../design/20260726-297-timer-injection-if-gate-nesting.md)

작업 지시: [docs/work-orders/20260726-297-timer-injection-if-gate-nesting.md](../work-orders/20260726-297-timer-injection-if-gate-nesting.md)

## 한국어

### 결과

두 INT 8 타이머 주입 경로에 실제 게스트 `EFlags.IF` 게이트를 적용했다. IF=0이면 게스트
스택과 실행 컨텍스트를 변경하지 않고 `timer_interrupt_pending`을 유지하므로, 틱은 최대 한
건으로 합쳐져 다음 IF=1 경계까지 보류된다. 주입 시 IF를 지우는 기존 동작과 결합되어 ISR
실행 중 재주입을 막는다.

### 변경 사항

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/execution_internal.h` | `kEFlagsInterruptEnable` 공용 상수 추가. |
| `src/platform/win32/execution/execution_trampoline.cpp` | CLI/STI가 공용 상수를 사용하도록 정리. `InjectPendingInterrupts`가 pending을 지우기 전에 IF를 검사하고, IF=0이면 보류하도록 수정. |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 선점 주입의 EIP 게이트에 IF 조건을 추가. IF=0이면 `preemptive_injected=false`를 유지하여 기존 보류 경로가 pending을 설정하게 함. |
| `docs/analysis/interrupts-and-port-io.md` | 구현 의미와 25초 격리 검증 결과를 확인됨/미확정으로 구분해 누적. |

명시적 in-flight 플래그는 추가하지 않았다. IF 게이트 적용 후 검증에서 중첩 하강 패턴이
재현되지 않았으므로, 설계에 따라 실제 재발이 확인될 때만 방어 계층을 추가한다.

### 검증

1. **빌드:**
   `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32` 성공.
   기존 C4819 코드페이지 경고 세 건만 출력되었다.
2. **격리 스모크 실행:** `aot-dbt`, `REPIU_EXECUTION_TIMEOUT_MS=25000`,
   `REPIU_TIMER_INJECT_LOG=1`, 격리 `REPIU_EEPROM_PATH`로 `pumpit1`을 한 번 실행했다.
   원시 로그는 `build/task297-if-gate-smoke-20260726-013026/`에 남겼다.
3. **결과:** 정상 타임아웃(exit 0), INT 8 주입 37회, 진행도 13,133, Glide 게이트
   `84/84`, 창 생성 `1/640x480`, exception caught `false`, malformed count `0`.
4. **중첩 관찰:** 프레임 주소는 호출 깊이에 따라 `0x035D6834~0x035D6DCC` 사이에서
   변했지만, 깊은 구간 뒤 다시 `0x035D6DA4` 부근으로 복귀했다. 수정 전의 지속적인
   `0x...6864→0x...6800` 단조 하강은 재현되지 않았다.
5. **상태 보존:** 실행용 EEPROM SHA-256은 원본 fixture와 일치했다.

### 검증 한계

25초 단일 실행이므로 장시간 비결정적이던 296류 RET/`0x287` 크래시 전체의 소멸을 확정하지
않는다. 확인된 범위는 대상 IF 게이트의 코드 순서, 중첩 하강 패턴 소멸, Glide gate 84까지의
busy-wait 돌파 유지, 예외 및 malformed dispatch 0건이다.

---

## English

### Result

Both INT 8 timer-injection paths now gate delivery on the real guest `EFlags.IF`. With IF clear they
leave the guest stack and execution context unchanged and preserve one coalesced
`timer_interrupt_pending` request until an IF-enabled boundary. Combined with the existing clearing of
IF on injection entry, this prevents re-injection while the ISR runs with interrupts disabled.

### Changes

| File | Change |
|---|---|
| `src/platform/win32/execution/execution_internal.h` | Added the shared `kEFlagsInterruptEnable` constant. |
| `src/platform/win32/execution/execution_trampoline.cpp` | Made CLI/STI use the shared constant; made `InjectPendingInterrupts` test IF before consuming pending state. |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | Added IF to the preemptive EIP gate; IF-clear snapshots remain uninjected and flow into the existing pending deferral. |
| `docs/analysis/interrupts-and-port-io.md` | Accumulated the implementation semantics and isolated-run evidence with confirmed/unresolved labels. |

No explicit in-flight flag was added. The post-gate verification did not reproduce the nested descent,
so the design reserves that additional defense for an observed recurrence.

### Verification

1. `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32` passed. Only three
   pre-existing C4819 code-page warnings appeared.
2. One isolated `pumpit1` run used `aot-dbt`, a 25-second timeout, timer-injection logging, and a copied
   EEPROM fixture. Raw logs are under `build/task297-if-gate-smoke-20260726-013026/`.
3. The run timed out cleanly with exit 0: 37 INT 8 injections, progress 13,133, 84/84 Glide gates, one
   640x480 window, no caught exception, and malformed count zero.
4. Frame addresses varied with call depth over `0x035D6834..0x035D6DCC` but returned near
   `0x035D6DA4` after the deep interval. The former continuously descending pattern did not recur.
5. The isolated EEPROM SHA-256 matched the fixture.

### Verification limit

A single 25-second run does not establish that every long-running, nondeterministic Task-296-era
RET/`0x287` crash is gone. It verifies the IF-gate ordering, absence of the targeted nesting pattern,
preservation of busy-wait escape through Glide gate 84, and zero exceptions/malformed dispatches within
this run.
