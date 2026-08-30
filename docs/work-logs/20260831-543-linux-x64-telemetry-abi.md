# 20260831-543 Linux x64 telemetry ABI 정규화 작업 로그

## 한국어

### 변경 내용

- `SharedLiveTelemetry`의 고정 shared-memory word를 `LiveTelemetryWord`라는
  `std::int32_t` alias로 변경했습니다.
- 기존 local `volatile long` counter API는 유지하고, shared telemetry 및 32-bit
  placement counter를 위한 32-bit atomic overload를 추가했습니다.
- Win32 supervisor의 shared telemetry reader를 새 field type에 맞췄습니다.
- Linux AOT dispatch와 Glide milestone pointer를 새 field type에 맞췄습니다.
- 32-bit placement counter를 `volatile long*`로 재해석하던 timer safe-point 경로를
  `volatile std::uint32_t*`로 수정했습니다.

### 검증 결과

Linux x64 probe는 `live_telemetry.h`의 `sizeof(long)==4` assertion을 통과했습니다.
이후 `execution_trampoline.cpp`까지 진행했고, 다음 오류에서 중단되었습니다.

```text
src/engine/execution/execution_trampoline.cpp:2288:9:
error: ‘CallGuestEntryWithStackTimed’ was not declared in this scope
src/engine/execution/execution_trampoline.cpp:2306:9:
error: ‘CallGuestEntryDirectTimed’ was not declared in this scope
```

두 함수의 정의는 `_M_IX86 || __i386__` 조건부 영역에만 있으면서 Linux x64의
일반 guest thread procedure에서는 호출되고 있습니다. 이 오류는 telemetry ABI와
독립적인 다음 실행 진입·stack bridge 장벽으로 분리했습니다.

회귀 검증으로 Linux i386 `repiu_exe` 정적 라이브러리 빌드와 Win32
`repiu_supervisor_win32` Debug 빌드를 성공시켰습니다. x64 실행 파일은 아직
생성되지 않았으며, 원본 guest 실행 경로는 변경하지 않았습니다.

## English

### Changes

- Changed the fixed shared-memory words in `SharedLiveTelemetry` to a
  `std::int32_t`-based `LiveTelemetryWord` alias.
- Kept the existing local `volatile long` counter API and added 32-bit atomic
  overloads for shared telemetry and 32-bit placement counters.
- Updated the Win32 supervisor reader for the new field type.
- Updated the Linux AOT dispatch and Glide milestone pointer types.
- Changed the timer safe-point path from reinterpreting 32-bit placement counters as
  `volatile long*` to using `volatile std::uint32_t*`.

### Verification

The Linux x64 probe passed the `sizeof(long)==4` assertion in `live_telemetry.h`. It
then reached `execution_trampoline.cpp` and stopped at:

```text
src/engine/execution/execution_trampoline.cpp:2288:9:
error: ‘CallGuestEntryWithStackTimed’ was not declared in this scope
src/engine/execution/execution_trampoline.cpp:2306:9:
error: ‘CallGuestEntryDirectTimed’ was not declared in this scope
```

Both functions are defined only inside `_M_IX86 || __i386__`, while the Linux x64
guest thread procedure calls them from the general Linux path. This is independent of
the telemetry ABI and is recorded as the next execution-entry and stack-bridge barrier.

As regression checks, the Linux i386 `repiu_exe` static library and the Win32
`repiu_supervisor_win32` Debug target both built successfully. No x64 executable was
produced and the original guest execution path was not changed.
