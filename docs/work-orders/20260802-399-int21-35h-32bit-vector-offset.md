# 20260802-399 INT 21h AH=35h 32비트 offset 수정 계획 / INT 21h AH=35h 32-bit Offset Fix Plan

## 한국어

### 목표

`HandleDosGetInterruptVector`가 `EBX` 하위 16비트만 기록해 호출자의 상위 절반을 남기는
결함을 수정하고, `AH=25h` 및 DPMI `AX=0204/0205`와 대칭인 32비트 서비스로 만든다.

근거는 [설계 문서](../design/20260802-399-int21-35h-32bit-vector-offset.md)에 있다.

### 작업 범위

1. `src/platform/win32/dos/dos_int21_services.cpp`:
   `HandleDosGetInterruptVector`가 `dpmi_interrupt_vectors`를 우선 조회하고
   `EBX`에 32비트 offset 전체를 기록하도록 수정.
2. `docs/design/20260802-399-int21-35h-32bit-vector-offset.md`: 설계 문서.
3. `docs/analysis/interrupts-and-port-io.md`: Task 398이 남긴 미해결 항목을 해소로 갱신.
4. `docs/kb/important-interrupts.md`: `AH=35h` 32비트 규약 기록.
5. `docs/work-logs/20260802-399-int21-35h-32bit-vector-offset.md`: 작업 로그.

### 검증 절차

1. 빌드: `cmake --build build --config Release --target repiu_loader_win32`
2. pumpit3 로그: `Win32 INT 8 chain HLE count/source/pointer/target`의 target offset이
   `0x03010000` → `0x00000000`으로 바뀌고 count가 계속 증가하는지 확인
3. pumpit1/pumpit2 로그: `INT 8 chain HLE count` 회귀 없음 확인

실행 검증은 사용자 제공 로그로 수행한다.

---

## English

### Objective

Fix `HandleDosGetInterruptVector` writing only the low 16 bits of `EBX` and leaving the
caller's high half behind, making it a 32-bit service symmetric with `AH=25h` and DPMI
`AX=0204/0205`.

Rationale is in the
[design document](../design/20260802-399-int21-35h-32bit-vector-offset.md).

### Task Scope

1. `src/platform/win32/dos/dos_int21_services.cpp`: have
   `HandleDosGetInterruptVector` read `dpmi_interrupt_vectors` first and write the full
   32-bit offset to `EBX`.
2. `docs/design/20260802-399-int21-35h-32bit-vector-offset.md`: design document.
3. `docs/analysis/interrupts-and-port-io.md`: mark the Task 398 open item resolved.
4. `docs/kb/important-interrupts.md`: record the 32-bit `AH=35h` contract.
5. `docs/work-logs/20260802-399-int21-35h-32bit-vector-offset.md`: work log.

### Verification Procedure

1. Build: `cmake --build build --config Release --target repiu_loader_win32`
2. pumpit3 log: the target offset in `Win32 INT 8 chain HLE count/source/pointer/target`
   changes from `0x03010000` to `0x00000000` and the count keeps rising.
3. pumpit1/pumpit2 logs: no regression in `INT 8 chain HLE count`.

Runtime verification uses logs provided by the user.
