# 20260802-401 BIOS INT 16h 구현 계획 / BIOS INT 16h Implementation Plan

## 한국어

### 목표

Task 399로 폴링 정지가 해소된 뒤 드러난 `0x03011537` 종료의 원인인 미구현 `INT 16h`
(BIOS 키보드)를 구현한다. 함께, 미지원 인터럽트 **벡터**가 로그에 이름을 남기지 않던
진단 공백을 닫는다.

근거는 [설계 문서](../design/20260802-401-bios-int16-keyboard-services.md)에 있다.

### 작업 범위

1. `src/platform/win32/bios/bios_keyboard_services.{h,cpp}` 신규:
   `HandleBiosInterrupt16`. `AH=00/01/02/10/11/12` 처리, 그 외는 `hle_message` 기록 후
   미처리.
2. `CMakeLists.txt`: 새 소스와 `src/platform/win32/bios` include 경로 등록.
3. `src/platform/win32/execution/execution_trampoline.cpp`:
   `HandleDosHleInstruction`에 `CD 16` 분기 추가.
4. `src/platform/win32/cpu_emul/instruction_emulation.{h,cpp}`:
   `HandleTracedBiosInterrupt16` 추가 후 traced dispatch chain 3곳에 연결.
5. `RecordUnsupportedTracedSoftwareInterrupt` 추가. VEH에서 소프트웨어 인터럽트를
   처리할 수 있는 handler를 모두 지난 뒤 호출해 `unsupported software interrupt 0xNN`을
   기록.
6. 문서: 설계, 작업 로그, `interrupts-and-port-io.md`,
   `current-execution-frontier.md`, `important-interrupts.md`.

### 검증 절차

1. 빌드: `cmake --build build --config Release --target repiu_loader_win32`
2. `REPIU_EXECUTION_TIMEOUT_MS=45000`으로 pumpit3 실행:
   `0x03011537` 종료 부재, timeout까지 진행 확인
3. `pumpit1`, `pumpit2` 회귀 확인

---

## English

### Objective

Implement the missing `INT 16h` (BIOS keyboard) behind the `0x03011537` termination that
appeared once Task 399 cleared the polling stall, and close the diagnostic gap where an
unsupported interrupt **vector** was never named in the log.

Rationale is in the
[design document](../design/20260802-401-bios-int16-keyboard-services.md).

### Task Scope

1. New `src/platform/win32/bios/bios_keyboard_services.{h,cpp}` with
   `HandleBiosInterrupt16` serving `AH=00/01/02/10/11/12` and recording `hle_message`
   for anything else.
2. `CMakeLists.txt`: register the source and the `src/platform/win32/bios` include path.
3. `src/platform/win32/execution/execution_trampoline.cpp`: add the `CD 16` branch to
   `HandleDosHleInstruction`.
4. `src/platform/win32/cpu_emul/instruction_emulation.{h,cpp}`: add
   `HandleTracedBiosInterrupt16` and wire it into the three traced dispatch chains.
5. Add `RecordUnsupportedTracedSoftwareInterrupt`, called in the VEH once every handler
   that can service a software interrupt has declined, recording
   `unsupported software interrupt 0xNN`.
6. Documentation: design, work log, `interrupts-and-port-io.md`,
   `current-execution-frontier.md`, `important-interrupts.md`.

### Verification Procedure

1. Build: `cmake --build build --config Release --target repiu_loader_win32`
2. Run pumpit3 with `REPIU_EXECUTION_TIMEOUT_MS=45000` and confirm the `0x03011537`
   termination is gone and the run continues to timeout.
3. Confirm no regression on `pumpit1` and `pumpit2`.
