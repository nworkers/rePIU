# 20260831-544 Linux x64 guest entry 빌드 경계 작업 지시서

## 한국어

### 목적

현재 x64 Linux에서 사용할 수 없는 i386 native guest entry 호출을 compile-time
경계로 차단하고, Linux x64 feasibility probe가 다음 구조적 장벽까지 진행되도록
합니다.

### 범위

- Linux non-Win32 guest thread procedure에 x64 fail-closed 경계를 추가합니다.
- i386 guest entry 구현과 original guest 실행 semantics는 변경하지 않습니다.
- x64 timed entry의 no-op 또는 임시 32비트 ABI shim은 추가하지 않습니다.
- x64 assembly·fault resume·AOT/DBT 구현은 다음 작업으로 남깁니다.

### 완료 기준

- Linux x64에서 i386 timed entry 미선언 오류가 사라지고 다음 probe 장벽이
  확인됩니다.
- Linux i386 `repiu_exe` 정적 라이브러리가 성공합니다.
- Win32 supervisor 회귀 target이 성공합니다.
- 설계·작업 로그·누적 분석 문서에 fail-closed 이유와 probe 결과를 기록합니다.

## 결과

x64 C++ 단계는 통과했고 `aot_dbt_dispatch_thunks.S`의 32비트 assembler 의존성이
다음 장벽으로 확인되었습니다. Linux i386 경로를 보호하는 fail-closed 경계는
완료되었으며, 다음 작업은 x86-64 guest entry 및 thunk bridge 설계입니다.

## English

### Objective

Add a compile-time fail-closed boundary around the i386 native guest-entry calls that
are unavailable on Linux x64, allowing the x64 feasibility probe to reach the next
structural barrier.

### Scope

- Add an x64 fail-closed branch to the non-Win32 Linux guest thread procedure.
- Do not change i386 guest entry or original guest execution semantics.
- Do not add an x64 timed-entry no-op or temporary 32-bit ABI shim.
- Leave x64 assembly, fault resume, and AOT/DBT implementation for later work.

### Completion criteria

- The Linux x64 i386 timed-entry declaration error disappears and the next probe
  barrier is identified.
- The Linux i386 `repiu_exe` static library succeeds.
- The Win32 supervisor regression target succeeds.
- Record the fail-closed rationale and probe result in the design, work log, and
  cumulative analysis document.

## Result

The x64 C++ stage passed and the 32-bit assembler dependency in
`aot_dbt_dispatch_thunks.S` was identified as the next barrier. The fail-closed
boundary protecting the Linux i386 path is complete; the next unit is the x86-64
guest-entry and thunk-bridge design.
