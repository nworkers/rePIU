# segment register store HLE 설계

`piu_1st`는 segment register load HLE 이후 relocated address `0x020F39B2`에서 `66 26 8C 1D ED 3A 0F 02`로 중단된다.

이 명령은 prefix `66 26`이 붙은 `8C /r`, 즉 `MOV r/m16, Sreg` 계열이다.
ModRM `1D`는 `mod=00`, `reg=3`, `r/m=5`이므로 guest `DS` selector를 32-bit displacement 대상 주소 `0x020F3AED`에 저장하는 형태로 해석한다.

호스트 Win32의 실제 segment register를 직접 사용하면 원본 DOS/4G selector 의미와 맞지 않으므로, 이전 단계에서 기록한 guest segment shadow state를 source of truth로 사용한다.

## 구현 방향

Win32 execution trampoline에 segment store HLE를 추가한다.

이번 단계의 처리 범위는 다음 한 가지 형태로 제한한다.

* `66 26 8C /r`
* `mod=00`, `r/m=5` absolute 32-bit displacement
* source segment register는 guest shadow state에 존재하는 selector
* destination은 relocated runtime memory 범위 안의 2바이트 write

처리 시에는 다음을 수행한다.

* ModRM에서 source segment register를 디코딩한다.
* guest segment shadow state에서 selector 값을 읽는다.
* destination displacement가 relocated runtime memory 범위 안인지 확인한다.
* selector를 little-endian 16-bit 값으로 destination에 기록한다.
* execution attempt에 처리 count, 마지막 EIP, opcode, segment register, selector, destination을 기록한다.
* 실제 host segment register는 변경하지 않는다.

privileged instruction classifier는 prefix를 건너뛴 뒤 `8C /r`를 `MOV r/m16, Sreg`로 분류한다.

## 검증 기준

* Win32 x86 빌드가 성공해야 한다.
* `dos4gw_hello`가 계속 `Hello, world!`를 출력해야 한다.
* `piu_1st` 실행에서 handled segment store count가 1 이상이어야 한다.
* `piu_1st`가 기존 `0x020F39B2` 중단 지점을 지나 다음 중단 지점까지 진행해야 한다.

# Segment Register Store HLE Design

After segment-register load HLE, `piu_1st` stops at relocated address `0x020F39B2` on `66 26 8C 1D ED 3A 0F 02`.

This instruction is `8C /r` with prefixes `66 26`, which belongs to the `MOV r/m16, Sreg` family.
ModRM `1D` decodes as `mod=00`, `reg=3`, `r/m=5`, so it stores the guest `DS` selector to the 32-bit displacement destination `0x020F3AED`.

The host Win32 segment registers do not carry the original DOS/4G selector meaning, so the guest segment shadow state recorded in the previous step is the source of truth.

## Implementation Direction

Add segment store HLE to the Win32 execution trampoline.

This step handles only this form:

* `66 26 8C /r`
* `mod=00`, `r/m=5` absolute 32-bit displacement
* source segment register selector exists in guest shadow state
* destination is a 2-byte write inside relocated runtime memory

Handling performs the following:

* Decode the source segment register from ModRM.
* Read the selector from guest segment shadow state.
* Verify the destination displacement is inside relocated runtime memory.
* Write the selector as a little-endian 16-bit value to the destination.
* Record handled count, last EIP, opcode, segment register, selector, and destination in the execution attempt.
* Do not modify real host segment registers.

The privileged instruction classifier skips prefixes and classifies `8C /r` as `MOV r/m16, Sreg`.

## Verification Criteria

* The Win32 x86 build should succeed.
* `dos4gw_hello` should still print `Hello, world!`.
* `piu_1st` should report handled segment store count at least 1.
* `piu_1st` should continue past the previous `0x020F39B2` stop to the next stop.
