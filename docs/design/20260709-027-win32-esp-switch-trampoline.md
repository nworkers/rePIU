# Win32 ESP 전환 trampoline 설계

## 배경

현재 Win32 실행 trampoline은 별도 thread에서 relocated entry를 직접 호출하여 첫 예외를 관찰한다.
이 경로는 원본 entry 진입 여부와 privileged instruction 예외 위치를 확인하는 데 충분했지만, host stack 위에서 원본 코드를 실행한다.

다음 단계에서는 원본 DOS/4G 코드가 기대하는 relocated stack top으로 ESP를 실제 전환한 뒤 entry를 호출한다.
이는 원본 게임 로직을 재작성하지 않고 실행 환경만 맞추는 HLE 방향에 맞다.

## 목표

* Win32 x86 빌드에서만 동작하는 ESP 전환 trampoline을 추가한다.
* `GuestStackSwitchPlan`의 entry EIP와 initial ESP를 사용한다.
* entry가 정상 return하면 host stack으로 복귀해 결과를 기록한다.
* entry 실행 중 SEH 예외가 발생하면 기존 예외 기록 경로를 유지한다.
* x64 또는 비 Win32 빌드에서는 명시적인 unsupported 결과를 유지한다.

## 비목표

* INT21/INT31 handler 구현
* HLE dispatcher에서 guest context를 수정한 뒤 원본 코드로 복귀
* 원본 executable patch
* 완전한 DOS/4G stack frame 재현

## 설계

기존 `AttemptWin32MinimalExecution`은 유지하고, stack 전환용 `AttemptWin32GuestStackExecution`을 추가한다.
새 API는 `Win32RelocatedImagePlacement`, `GuestStackSwitchPlan`, timeout, 결과 구조체를 입력받는다.

32-bit MSVC 빌드에서는 작은 naked helper가 다음 순서로 동작한다.

1. host callee-saved register와 host ESP를 보존한다.
2. ESP를 `GuestStackSwitchPlan.initial_esp`로 바꾼다.
3. guest stack에 trampoline context 포인터를 하나 남긴 뒤 entry를 call한다.
4. entry가 return하면 context 포인터를 회수하고 host ESP를 복원한다.
5. host thread proc으로 돌아와 정상 return 결과를 기록한다.

guest stack 위에서 예외가 발생하면 일반 SEH 복귀만으로는 host stack 복원이 불안정할 수 있다.
따라서 stack 전환 실행 중에는 VEH를 먼저 등록하고, 예외 발생 시 exception code/address를 기록한 뒤 `EIP`와 `ESP`를 host 복구 stub으로 바꾼다.
복구 stub은 callee-saved register와 host stack을 복원하고 thread proc으로 돌아간다.
이 단계에서는 예외를 HLE handler로 재개하지 않고 관찰 결과로만 반환한다.

## 검증

* Win32 x86 CMake 빌드가 성공한다.
* Win32 x86 loader 실행 시 guest stack switch attempt가 attempted로 출력된다.
* 예외 위치와 relocated byte window가 계속 출력된다.
* x64 빌드는 성공하고 guest stack execution은 unsupported로 보고된다.

# Win32 ESP-Switching Trampoline Design

## Background

The current Win32 execution trampoline calls the relocated entry from a separate thread to observe the first exception.
That path was enough to confirm entry transfer and the privileged-instruction exception location, but it runs original code on the host stack.

The next step is to switch ESP to the relocated stack top expected by the original DOS/4G code before calling the entry.
This matches the HLE direction: preserve the original game logic and adapt only the surrounding execution environment.

## Goals

* Add an ESP-switching trampoline that only runs in Win32 x86 builds.
* Use the entry EIP and initial ESP from `GuestStackSwitchPlan`.
* If the entry returns normally, restore the host stack and record the result.
* If SEH catches an exception during entry execution, keep the existing exception recording path.
* Keep explicit unsupported results for x64 or non-Win32 builds.

## Non-Goals

* Implementing INT21/INT31 handlers.
* Returning to original code after the HLE dispatcher modifies guest context.
* Patching the original executable.
* Fully recreating a DOS/4G stack frame.

## Design

Keep `AttemptWin32MinimalExecution` and add `AttemptWin32GuestStackExecution` for stack switching.
The new API accepts `Win32RelocatedImagePlacement`, `GuestStackSwitchPlan`, timeout, and the result structure.

In 32-bit MSVC builds, a small naked helper runs this sequence.

1. Preserve host callee-saved registers and host ESP.
2. Switch ESP to `GuestStackSwitchPlan.initial_esp`.
3. Leave the trampoline context pointer on the guest stack and call entry.
4. If entry returns, recover the context pointer and restore host ESP.
5. Return to the host thread proc and record normal return.

If an exception occurs on the guest stack, returning only through normal SEH can make host stack restoration unreliable.
Therefore, stack-switched execution first registers a VEH handler. On exception, it records the exception code/address and rewrites `EIP` and `ESP` to a host recovery stub.
The recovery stub restores callee-saved registers and the host stack, then returns to the thread proc.
This step returns the exception as an observation result rather than resuming through an HLE handler.

## Verification

* Win32 x86 CMake build succeeds.
* The Win32 x86 loader prints a guest stack switch attempt as attempted.
* The exception address and relocated byte window are still printed.
* The x64 build succeeds and reports guest stack execution as unsupported.
