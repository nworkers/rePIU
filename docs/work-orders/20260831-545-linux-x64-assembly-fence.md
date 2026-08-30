# 20260831-545 Linux x64 assembly 경계 작업 지시서

## 한국어

### 목적

Linux x64 compile probe가 i386 전용 GAS에서 중단되지 않도록 빌드 입력을 분리하고,
x64에서 실제로 빌드 가능한 POSIX/C++ 범위를 확인합니다.

### 작업

- `repiu_exe`의 Linux assembly 입력을 `CMAKE_SIZEOF_VOID_P == 4`로 제한합니다.
- `repiu_core_probe`의 stack bridge 및 guest stack switch assembly와 대응 C++ probe를
  같은 조건으로 제한합니다.
- x64 probe가 제외된 두 probe를 명시적으로 보고하도록 core probe 출력을 갱신합니다.
- 설계 문서와 누적 Linux frontier에 결정·검증 결과를 기록합니다.

### 비범위

- x86-64 thunk 또는 guest stack switch 구현
- 원본 guest의 64비트 변환
- WSLg/Mesa 설치 변경
- Linux i386 또는 Win32 실행 의미 변경

### 검증 절차

```text
cmake --build build/linux_x64_probe --target repiu_exe --parallel 2
cmake --build build/linux_x64_probe --target repiu_core_probe --parallel 2
cmake --build build/linux_i386 --target repiu_exe --parallel 2
```

가능하면 x64 `repiu_core_probe`를 실행하여 skipped 출력과 나머지 probe 결과를
기록합니다. x64 launcher/link가 별도 장벽을 드러내면 해당 장벽을 다음 작업으로
분리합니다.

## English

### Objective

Separate i386-only GAS inputs so the Linux x64 compile probe can continue past the
current assembly failure and expose the POSIX/C++ scope that is actually buildable on
x64.

### Work items

- Restrict the Linux assembly inputs of `repiu_exe` to `CMAKE_SIZEOF_VOID_P == 4`.
- Apply the same condition to the stack-bridge and guest-stack-switch assembly and
  their C++ probes in `repiu_core_probe`.
- Make the x64 core probe explicitly report the two excluded probes.
- Record the decision and verification result in the design document and cumulative
  Linux frontier.

### Out of scope

- Implementing x86-64 thunks or guest stack switching
- Converting the original guest to 64-bit
- Changing WSLg/Mesa installation
- Changing Linux i386 or Win32 execution semantics

### Verification

Run the x64 executable and core-probe targets, then the Linux i386 executable target.
Run the x64 core probe when it links; record its skipped output and any new launcher or
link barrier as the next work item.
