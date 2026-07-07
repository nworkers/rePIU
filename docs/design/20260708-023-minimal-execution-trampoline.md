# Minimal Execution Trampoline 설계

## 배경

Relocated image가 Win32 x86 process memory에 배치되고 object protection까지 적용되었다.

다음 단계는 원본 entry를 실제로 아주 짧게 호출해 보고, 첫 실패 지점이나 예외를 관찰하는 것이다.

아직 guest stack 전환, HLE dispatcher, INT/DPMI trap은 준비되지 않았으므로 이번 실행 시도는 제한적이고 관찰 중심이어야 한다.

## 목표

* Win32 x86 전용 최소 execution trampoline을 추가한다.
* relocated entry를 별도 thread에서 한 번 호출한다.
* SEH로 access violation, illegal instruction, privileged instruction 등을 잡아 process가 죽지 않게 한다.
* 일정 시간 안에 돌아오지 않으면 timeout으로 보고한다.
* execution host에서 attempt 결과를 출력한다.
* 남은 작업은 `docs/TODO.md`에 유지한다.

## 비목표

* guest stack 전환
* 원본 코드의 정상 실행 보장
* DOS/DPMI/HLE trap 처리
* 게임 화면 진입

## 설계

`src/platform/win32/execution_trampoline.cpp`와 대응 header를 추가한다.

API는 `AttemptWin32MinimalExecution`으로 둔다.

입력은 relocated image placement 결과와 relocated entry address다.

32-bit host process가 아니면 unsupported로 보고한다.

32-bit host process에서는 `CreateThread`로 별도 thread를 만들고, thread proc 안에서 relocated entry를 함수 포인터로 호출한다.

thread proc은 `__try/__except`로 감싸 예외를 잡는다. 예외가 발생하면 exception code와 exception address를 기록한다.

thread가 지정 timeout 안에 종료되지 않으면 timeout으로 기록하고 thread를 강제 종료한다. 이 방식은 첫 관찰용이며 장기 실행 방식이 아니다.

## 검증 기준

* Win32 x86 빌드가 성공한다.
* execution host가 relocated image placement 뒤 minimal execution attempt 결과를 출력한다.
* 예외, 정상 복귀, timeout 중 하나로 결과가 정리된다.
* x64 빌드는 성공하고 attempt는 unsupported로 남는다.

## 향후 확장

다음 단계에서는 skipped relocation 10개와 실제 예외 주소를 함께 분석해 HLE trap 또는 guest stack/trampoline 설계를 구체화한다.

## 관찰 결과

Win32 x86 execution host의 첫 실행 시도는 relocated entry `0x020F3818`로 진입한 뒤 `0x020F3890`에서 SEH 예외 `0xC0000096`을 기록했다.

이는 privileged instruction 계열 예외이므로, 다음 단계에서는 해당 주소 주변 명령을 확인하고 DOS/DPMI/HLE trap 또는 CPU state 초기화 부족 여부를 판단한다.

## Background

The relocated image is now placed in Win32 x86 process memory with object protection applied.

The next step is to call the original entry point very briefly and observe the first failure point or exception.

Guest stack switching, HLE dispatch, and INT/DPMI traps are not ready yet, so this execution attempt must be limited and observation-oriented.

## Goal

* Add a Win32 x86-only minimal execution trampoline.
* Call the relocated entry once from a separate thread.
* Use SEH to catch access violations, illegal instructions, privileged instructions, and similar exceptions so the process survives.
* Report timeout if the call does not return in time.
* Print the attempt result from the execution host.
* Keep remaining tasks in `docs/TODO.md`.

## Non-Goals

* Guest stack switching.
* Guaranteed normal execution of original code.
* DOS/DPMI/HLE trap handling.
* Reaching the game screen.

## Design

Add `src/platform/win32/execution_trampoline.cpp` and a matching header.

The API is `AttemptWin32MinimalExecution`.

Inputs are the relocated image placement result and the relocated entry address.

If the host process is not 32-bit, the attempt reports unsupported.

In a 32-bit host process, the function creates a separate thread with `CreateThread`, and the thread proc calls the relocated entry as a function pointer.

The thread proc wraps the call in `__try/__except` and records exception code and exception address.

If the thread does not finish within the timeout, the attempt records timeout and terminates the thread. This is only for first observation and is not a long-term execution model.

## Verification Criteria

* The Win32 x86 build succeeds.
* The execution host prints minimal execution attempt results after relocated image placement.
* The result is reported as exception, return, or timeout.
* The x64 build succeeds and the attempt remains unsupported.

## Future Extension

The next step will analyze the skipped 10 relocations together with the actual exception address to refine HLE traps or guest stack/trampoline design.

## Observation

The first Win32 x86 execution host attempt entered relocated entry `0x020F3818` and recorded SEH exception `0xC0000096` at `0x020F3890`.

This is a privileged-instruction class exception, so the next step should inspect instructions around that address and decide whether this requires a DOS/DPMI/HLE trap or missing CPU state initialization.
