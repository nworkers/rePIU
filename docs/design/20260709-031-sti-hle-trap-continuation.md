# STI HLE trap 계속 실행 설계

## 배경

현재 `piu_1st` 실행은 원본 entry와 guest stack 전환까지 성공한 뒤 relocated address `0x020F3890`에서 `STI` 명령으로 멈춘다.
`STI`는 Win32 user-mode에서 직접 실행할 수 없는 privileged instruction이며, classifier는 이 지점을 `HLE trap candidate`로 분류했다.

기존 trampoline에는 `CLI`/`STI`를 단순히 건너뛰는 코드가 있지만, 이 경로는 `dos4gw_hello` 전용 DOS console HLE 실행에 묶여 있다.
`piu_1st`에는 DOS console HLE 전체가 아니라, 먼저 privileged trap만 켜서 다음 실제 중단 지점을 관찰해야 한다.

## 설계

Win32 execution trampoline에 privileged trap HLE 플래그를 DOS HLE 플래그와 분리해서 추가한다.

* `enable_privileged_trap_hle`: `CLI`/`STI` 같은 privileged trap 후보를 처리한다.
* `enable_dos_hle`: DOS console 샘플에 필요한 INT/DOS memory access HLE를 처리한다.

초기 `STI` 처리 규칙은 다음과 같다.

* opcode `0xFB`를 만나면 Win32 `CONTEXT.EFlags`의 interrupt flag bit를 세운다.
* `Eip`를 1 증가시켜 다음 명령으로 이동한다.
* 처리 count와 마지막 처리 EIP/opcode를 execution attempt에 기록한다.

`CLI`도 같은 경로에서 처리하되 interrupt flag bit를 끈다.
이번 작업은 실제 interrupt delivery를 구현하지 않는다.

## loader 정책

`piu_1st`는 privileged trap HLE를 켠 guest stack 실행 경로를 사용한다.
`dos4gw_hello`는 기존처럼 DOS HLE 경로를 사용하되, 내부적으로 privileged trap HLE도 함께 켠다.

## 검증

* Win32 x86 loader 빌드가 성공해야 한다.
* `piu_1st` 실행에서 첫 `STI`가 처리되어 `handled trap count`가 1 이상이어야 한다.
* 실행은 `0x020F3890`에서 멈추지 않고 다음 중단 지점 또는 timeout으로 진행해야 한다.
* `dos4gw_hello`의 `Hello, world!` 출력은 유지되어야 한다.

# STI HLE Trap Continuation Design

## Background

The current `piu_1st` run successfully enters the original entry with the guest stack, then stops at relocated address `0x020F3890` on `STI`.
`STI` is a privileged instruction that cannot execute in Win32 user mode, and the classifier marks this point as an `HLE trap candidate`.

The existing trampoline can skip `CLI`/`STI`, but that path is tied to the `dos4gw_hello` DOS console HLE mode.
For `piu_1st`, we should first enable only privileged traps, not the whole DOS console HLE path, so the next real stop can be observed.

## Design

Add a privileged trap HLE flag to the Win32 execution trampoline, separate from the DOS HLE flag.

* `enable_privileged_trap_hle`: handles privileged trap candidates such as `CLI`/`STI`.
* `enable_dos_hle`: handles INT and DOS memory access HLE required by the DOS console sample.

Initial `STI` handling:

* When opcode `0xFB` is seen, set the interrupt flag bit in Win32 `CONTEXT.EFlags`.
* Increment `Eip` by 1 to move to the next instruction.
* Record handled count and the last handled EIP/opcode in the execution attempt.

`CLI` is handled by the same path but clears the interrupt flag bit.
This task does not implement actual interrupt delivery.

## Loader Policy

`piu_1st` uses guest stack execution with privileged trap HLE enabled.
`dos4gw_hello` keeps the existing DOS HLE path, which also enables privileged trap HLE internally.

## Verification

* Win32 x86 loader build must pass.
* `piu_1st` should handle the first `STI`, with `handled trap count` at least 1.
* Execution should no longer stop at `0x020F3890`; it should proceed to the next stop or timeout.
* `dos4gw_hello` must keep printing `Hello, world!`.
