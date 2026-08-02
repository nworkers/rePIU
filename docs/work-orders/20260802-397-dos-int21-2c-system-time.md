# 20260802-397 DOS INT 21h AH=2Ch 구현 계획 / DOS INT 21h AH=2Ch Implementation Plan

## 한국어

### 목표

`pumpit3`가 초기화 후반부에서 실행 중단되는 원인인 미구현 `INT 21h AH=2Ch`
(Get System Time)를 DOS 사양대로 구현하여 게스트 loop 보정 루틴이 완주하도록 한다.

근거와 분석은 [설계 문서](../design/20260802-397-dos-int21-2c-system-time.md)에 있다.

### 작업 범위

1. `src/platform/win32/dos/dos_int21_services.h`: `HandleDosGetSystemTime` 선언 추가.
2. `src/platform/win32/dos/dos_int21_services.cpp`:
   - `HandleDosGetSystemTime` 구현 (`GetLocalTime` → `CX` = 시:분, `DX` = 초:1/100초).
   - `HandleDosInterrupt21`의 `switch (ah)`에 `case 0x2C` 추가.
3. `src/platform/win32/cpu_emul/instruction_emulation.cpp`:
   - `HandleTracedDosInterrupt21`의 `switch (ah)`에 `case 0x2C` 추가.
4. `docs/analysis/interrupts-and-port-io.md`: 지원 INT 21h 함수 목록에 AH=2Ch 반영.
5. `docs/kb/important-interrupts.md`: AH=2Ch 레지스터 규약 반영.
6. `docs/analysis/current-execution-frontier.md`: pumpit3 frontier 항목 기록.
7. `docs/work-logs/20260802-397-dos-int21-2c-system-time.md`: 작업 로그 작성.

### 검증 절차

1. 빌드: `cmake --build build/win32_x86_debug`
2. `repiu_host --target pumpit3` 실행 로그에서 확인:
   - `Current execution blocker: unhandled HLE trap candidate` 부재
   - `0x030D3941`(`int 21h` AH=2Ch) 통과
3. `pumpit1`, `pumpit2` 회귀 확인.

실행 검증은 사용자 제공 로그로 수행한다.

---

## English

### Objective

Implement the missing `INT 21h AH=2Ch` (Get System Time) per the DOS contract so
the guest loop-calibration routine that stops `pumpit3` late in initialization can
run to completion.

Rationale and analysis live in the
[design document](../design/20260802-397-dos-int21-2c-system-time.md).

### Task Scope

1. `src/platform/win32/dos/dos_int21_services.h`: declare `HandleDosGetSystemTime`.
2. `src/platform/win32/dos/dos_int21_services.cpp`:
   - Implement `HandleDosGetSystemTime` (`GetLocalTime` → `CX` = hour:minute,
     `DX` = second:hundredths).
   - Add `case 0x2C` to the `switch (ah)` in `HandleDosInterrupt21`.
3. `src/platform/win32/cpu_emul/instruction_emulation.cpp`:
   - Add `case 0x2C` to the `switch (ah)` in `HandleTracedDosInterrupt21`.
4. `docs/analysis/interrupts-and-port-io.md`: record AH=2Ch in the supported list.
5. `docs/kb/important-interrupts.md`: record the AH=2Ch register contract.
6. `docs/analysis/current-execution-frontier.md`: record the pumpit3 frontier entry.
7. `docs/work-logs/20260802-397-dos-int21-2c-system-time.md`: write the work log.

### Verification Procedure

1. Build: `cmake --build build/win32_x86_debug`
2. In the `repiu_host --target pumpit3` log, confirm:
   - no `Current execution blocker: unhandled HLE trap candidate`
   - execution passes `0x030D3941` (the AH=2Ch `int 21h`)
3. Confirm no regression on `pumpit1` and `pumpit2`.

Runtime verification uses logs provided by the user.
