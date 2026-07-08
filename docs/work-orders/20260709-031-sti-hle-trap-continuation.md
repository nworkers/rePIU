# STI HLE trap 계속 실행 작업 지시

## 목표

`piu_1st`가 `0x020F3890`의 `STI`에서 멈추지 않고, HLE trap 처리 후 다음 명령으로 계속 진행하게 한다.

## 범위

1. Win32 execution trampoline에서 privileged trap HLE 플래그를 DOS HLE 플래그와 분리한다.
2. `CLI`/`STI` 처리 시 `EFlags.IF`와 `Eip`를 갱신한다.
3. execution attempt에 처리된 HLE trap count와 마지막 처리 EIP/opcode를 기록한다.
4. loader가 `piu_1st`에 privileged trap HLE 경로를 사용하도록 연결한다.
5. `piu_1st`와 `dos4gw_hello`를 검증한다.
6. TODO와 작업 로그를 갱신한다.

## 제외

* 실제 interrupt delivery는 구현하지 않는다.
* INT21/INT31 일반 dispatcher 구현은 다음 작업으로 둔다.
* selector/descriptor DPMI API 연결은 다음 작업으로 둔다.

# STI HLE Trap Continuation Work Order

## Goal

Allow `piu_1st` to continue past `STI` at `0x020F3890` by handling it as an HLE trap and advancing to the next instruction.

## Scope

1. Separate the privileged trap HLE flag from the DOS HLE flag in the Win32 execution trampoline.
2. Update `EFlags.IF` and `Eip` when handling `CLI`/`STI`.
3. Record handled HLE trap count and the last handled EIP/opcode in the execution attempt.
4. Wire the loader so `piu_1st` uses the privileged trap HLE path.
5. Verify `piu_1st` and `dos4gw_hello`.
6. Update TODO and the work log.

## Out Of Scope

* Do not implement actual interrupt delivery.
* Leave general INT21/INT31 dispatcher implementation for the next task.
* Leave selector/descriptor DPMI API wiring for the next task.
