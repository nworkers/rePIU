# segment register load HLE 설계

## 배경

`piu_1st`는 `INT 21h AH=0xFF` 최소 처리 후 relocated address `0x020F3995`에서 opcode `0x8E`로 중단된다.
byte window의 focus bytes는 `8E D9`이다.

`8E /r`는 `MOV Sreg, r/m16` 명령이다.
ModRM `D9`는 `mod=3`, `reg=3`, `rm=1`로 해석되므로 `MOV DS, CX`이다.
실행 당시 `ECX=0x00000024`였기 때문에 원본 코드는 guest `DS` selector를 `0x0024`로 설정하려고 한다.

Win32 user-mode host에서 원본 코드가 원하는 selector 값을 실제 `DS`에 그대로 반영하는 것은 안전하지 않다.
따라서 이 단계에서는 host segment register를 바꾸지 않고 guest segment 상태로 기록한 뒤 다음 명령으로 진행한다.

## 설계

Win32 execution trampoline에 segment load HLE 플래그를 추가한다.
trace 기반 실행 경로에서 다음 명령을 처리한다.

* `8E /r` 중 `mod=3` register source 형태

초기 처리 대상은 `MOV DS, CX`이다.
처리 규칙은 다음과 같다.

* source register의 low 16-bit 값을 selector로 읽는다.
* target segment register와 selector를 `ThreadContext`의 guest segment shadow state에 기록한다.
* execution attempt에 처리 count, 마지막 EIP, opcode, segment register, selector를 기록한다.
* host `CONTEXT`의 실제 segment register 값은 변경하지 않는다.
* `EIP += 2`

classifier에도 `0x8E`를 `MOV Sreg, r/m16`으로 분류하는 규칙을 추가한다.
이 명령은 HLE trap이라기보다 selector/descriptor 상태 HLE 요구사항으로 본다.

## 검증

* Win32 x86 loader 빌드가 성공해야 한다.
* `piu_1st` 실행에서 handled segment load count가 1 이상이어야 한다.
* 처리 로그가 segment register 이름과 selector 값을 출력해야 한다.
* 실행은 `0x020F3995`에서 멈추지 않고 다음 중단 지점으로 진행해야 한다.
* `dos4gw_hello` 출력은 유지되어야 한다.

# Segment Register Load HLE Design

## Background

After minimal handling for `INT 21h AH=0xFF`, `piu_1st` stops at opcode `0x8E` at relocated address `0x020F3995`.
The focused bytes in the byte window are `8E D9`.

`8E /r` is `MOV Sreg, r/m16`.
ModRM `D9` decodes as `mod=3`, `reg=3`, `rm=1`, so the instruction is `MOV DS, CX`.
At the stop, `ECX=0x00000024`, so the original code is trying to set guest `DS` selector to `0x0024`.

Mirroring the original selector directly into the real Win32 user-mode `DS` is not safe.
This step records the selector in guest segment state and advances execution without changing the host segment register.

## Design

Add a segment load HLE flag to the Win32 execution trampoline.
The trace-based execution path handles:

* `8E /r` with `mod=3` register source

The initial target is `MOV DS, CX`.
Handling rules:

* Read the source register low 16-bit value as the selector.
* Record the target segment register and selector in `ThreadContext` guest segment shadow state.
* Record handled count, last EIP, opcode, segment register, and selector in the execution attempt.
* Do not modify the real segment register in host `CONTEXT`.
* Advance `EIP += 2`.

The classifier also adds `0x8E` as `MOV Sreg, r/m16`.
This instruction is treated as a selector/descriptor state HLE requirement rather than a normal HLE trap.

## Verification

* Win32 x86 loader build must pass.
* `piu_1st` should report handled segment load count at least 1.
* The handling log should print the segment register name and selector value.
* Execution should no longer stop at `0x020F3995`; it should proceed to the next stop.
* `dos4gw_hello` output must be preserved.
