# Traced C7 ModR/M memory store HLE 작업 로그

## 변경 내용

기존 `C7 01 imm32` 특수 처리를 `C7 /0 r/m32, imm32` ModR/M memory store 처리로 확장했다. SIB 없는 32-bit addressing만 지원한다.

## 결과

`0x0201DF1A`의 `C7 40 28 00 00 80 3F` store를 통과했고, 다음 관측 지점은 `0x0201DF21`의 `D9 40 28` FPU memory load로 이동했다.

# Traced C7 ModR/M Memory Store HLE Work Log

## Changes

Expanded the previous `C7 01 imm32` special case into `C7 /0 r/m32, imm32` ModR/M memory-store handling. Only 32-bit addressing without SIB is supported.

## Result

Execution advanced past the `C7 40 28 00 00 80 3F` store at `0x0201DF1A`. The next observed point moved to the `D9 40 28` FPU memory load at `0x0201DF21`.
