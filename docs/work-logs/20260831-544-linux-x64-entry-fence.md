# 20260831-544 Linux x64 guest entry 빌드 경계 작업 로그

## 한국어

### 변경 내용

Linux non-Win32 `GuestEntryThreadProc`에 x86 native entry 지원 여부를 compile-time으로
나누는 fail-closed 경계를 추가했습니다. x64에서는 기존 i386 timed entry를 호출하지
않고 unsupported code 4를 반환하며, i386 경로의 guest entry와 fault 처리 코드는
변경하지 않았습니다.

### 검증 결과

Linux x64 C++ 컴파일은 i386 timed entry 미선언 오류를 통과했고, engine의 C++ 소스와
Linux platform C++ 소스까지 진행했습니다. 다음 assembler 오류가 첫 장벽으로
확인되었습니다.

```text
src/platform/linux/aot_dbt_dispatch_thunks.S:62:
Error: `pusha' is not supported in 64-bit mode
src/platform/linux/aot_dbt_dispatch_thunks.S:89:
Error: operand size mismatch for `push'
src/platform/linux/aot_dbt_dispatch_thunks.S:95:
Error: `popa' is not supported in 64-bit mode
```

이는 기존 thunk가 32비트 register save frame과 32비트 stack ABI를 직접 사용한다는
증거입니다. x64에서 기존 assembly를 단순히 `-m64`로 재사용할 수 없으며, 다음
작업에서 x86-64 register frame·SysV ABI·guest state bridge를 새로 설계해야 합니다.

Linux i386 `repiu_exe` 정적 라이브러리와 Win32 supervisor Debug target은 이 단위
변경 후에도 성공해야 하며, x64 실행 파일은 아직 제공하지 않습니다.

## English

### Changes

Added a compile-time fail-closed boundary to the non-Win32 Linux
`GuestEntryThreadProc`. On x64 it returns unsupported code 4 instead of calling the
existing i386 timed entry, while the i386 guest-entry and fault paths remain unchanged.

### Verification

The Linux x64 C++ compilation passed the missing i386 timed-entry declarations and
reached the engine and Linux platform C++ sources. The first assembler barrier was:

```text
src/platform/linux/aot_dbt_dispatch_thunks.S:62:
Error: `pusha' is not supported in 64-bit mode
src/platform/linux/aot_dbt_dispatch_thunks.S:89:
Error: operand size mismatch for `push'
src/platform/linux/aot_dbt_dispatch_thunks.S:95:
Error: `popa' is not supported in 64-bit mode
```

This confirms that the existing thunk directly depends on a 32-bit register-save
frame and 32-bit stack ABI. The assembly cannot be reused by merely adding `-m64`; the
next unit must design an x86-64 register frame, SysV ABI bridge, and guest-state bridge.

The Linux i386 `repiu_exe` static library and Win32 supervisor Debug target remain the
regression targets for this unit; no x64 executable is provided yet.
