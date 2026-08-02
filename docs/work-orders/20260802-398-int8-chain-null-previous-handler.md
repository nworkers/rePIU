# 20260802-398 INT 8 체인 인식 조건 수정 계획 / INT 8 Chain Recognition Fix Plan

## 한국어

### 목표

pumpit3가 자신의 INT 8 ISR 안에서 이전 핸들러로 체인하는 `pushf` + `call far`에서
종료하는 문제를 해소한다. 저장된 이전 핸들러 포인터가 실행 가능한 코드를 가리키지 않는
경우를 타이틀별 offset이 아니라 selector 규칙으로 인식한다.

근거는 [설계 문서](../design/20260802-398-int8-chain-null-previous-handler.md)에 있다.

### 작업 범위

1. `src/platform/win32/boundary/timer_interrupt_boundary.cpp`:
   `HandleTimerInterruptChainBoundary`의 인식 조건을
   `target_offset == 0 && target_selector != 0 && target_selector == DS`에서
   `target_selector != CS`로 교체.
2. `docs/design/20260802-398-int8-chain-null-previous-handler.md`: 설계 문서.
3. `docs/analysis/interrupts-and-port-io.md`: Task 398 항목 및 AH=35h 절단 결함 기록.
4. `docs/analysis/current-execution-frontier.md`: pumpit3 frontier 갱신.
5. `docs/work-logs/20260802-398-int8-chain-null-previous-handler.md`: 작업 로그.

### 범위 밖 (별도 Task 권고)

`HandleDosGetInterruptVector`의 `EBX` 하위 16비트 절단은 확인된 별개 결함이다. `AH=25h`가
32비트 전체를 저장하는 것과 비대칭이며, pumpit1/pumpit2와 공유하는 경로를 바꾸므로 세
타이틀 회귀 검증을 포함한 별도 Task로 분리한다.

### 검증 절차

1. 빌드: `cmake --build build --config Release --target repiu_loader_win32`
2. pumpit3 로그: `Win32 INT 8 chain HLE count`가 0에서 증가, `0x0301F827` 통과,
   `Win32 minimal execution exception address: 0x0301F827` 부재
3. pumpit1/pumpit2 로그: `INT 8 chain HLE count` 회귀 없음

실행 검증은 사용자 제공 로그로 수행한다.

---

## English

### Objective

Stop pumpit3 from terminating on the `pushf` + `call far` that chains to the previous
handler inside its own INT 8 ISR, by recognizing a saved pointer that does not designate
executable code through a selector rule rather than per-title offsets.

Rationale is in the
[design document](../design/20260802-398-int8-chain-null-previous-handler.md).

### Task Scope

1. `src/platform/win32/boundary/timer_interrupt_boundary.cpp`: replace the
   `HandleTimerInterruptChainBoundary` condition
   `target_offset == 0 && target_selector != 0 && target_selector == DS` with
   `target_selector != CS`.
2. `docs/design/20260802-398-int8-chain-null-previous-handler.md`: design document.
3. `docs/analysis/interrupts-and-port-io.md`: Task 398 entry plus the AH=35h truncation
   defect.
4. `docs/analysis/current-execution-frontier.md`: update the pumpit3 frontier.
5. `docs/work-logs/20260802-398-int8-chain-null-previous-handler.md`: work log.

### Out of scope (separate task recommended)

The low-16-bit `EBX` truncation in `HandleDosGetInterruptVector` is a confirmed separate
defect, asymmetric with `AH=25h` storing the full 32 bits. It changes a path shared with
pumpit1 and pumpit2, so it belongs in its own task with three-title regression verification.

### Verification Procedure

1. Build: `cmake --build build --config Release --target repiu_loader_win32`
2. pumpit3 log: `Win32 INT 8 chain HLE count` rises above zero, execution passes
   `0x0301F827`, and no `Win32 minimal execution exception address: 0x0301F827`.
3. pumpit1/pumpit2 logs: no regression in `INT 8 chain HLE count`.

Runtime verification uses logs provided by the user.
