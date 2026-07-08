# memory-source segment register load HLE 설계

`piu_1st`는 segment register store HLE 이후 relocated address `0x020F39C8`에서 `66 8E 05 E4 65 1A 02`로 중단된다.

이 명령은 prefix `66`이 붙은 `8E /r`, 즉 `MOV Sreg, r/m16` 계열이다.
ModRM `05`는 `mod=00`, `reg=0`, `r/m=5`이므로 32-bit displacement source `0x021A65E4`에서 16-bit selector를 읽어 guest `ES`에 load하는 형태로 해석한다.

이전 단계의 register-source segment load와 동일하게 host segment register는 변경하지 않고, guest segment shadow state만 갱신한다.

## 구현 방향

기존 Win32 execution trampoline의 segment load HLE를 확장한다.

이번 단계의 추가 처리 범위는 다음 한 가지 형태로 제한한다.

* optional instruction prefix 뒤의 `8E /r`
* `mod=00`, `r/m=5` absolute 32-bit displacement memory source
* source는 relocated runtime memory 범위 안의 2바이트 read
* target segment register는 guest shadow state에 기록

처리 시에는 다음을 수행한다.

* prefix를 건너뛰고 opcode, ModRM, displacement를 디코딩한다.
* relocated runtime memory 범위 안에서 16-bit selector를 읽는다.
* target segment register와 selector를 guest segment shadow state에 기록한다.
* execution attempt에 마지막 segment load source address를 함께 기록한다.
* 실제 host segment register는 변경하지 않는다.

## 검증 기준

* Win32 x86 빌드가 성공해야 한다.
* `dos4gw_hello`가 계속 `Hello, world!`를 출력해야 한다.
* `piu_1st` 실행에서 handled segment load count가 증가해야 한다.
* `piu_1st`가 기존 `0x020F39C8` 중단 지점을 지나 다음 중단 지점까지 진행해야 한다.

# Memory-Source Segment Register Load HLE Design

After segment-register store HLE, `piu_1st` stops at relocated address `0x020F39C8` on `66 8E 05 E4 65 1A 02`.

This instruction is `8E /r` with prefix `66`, which belongs to the `MOV Sreg, r/m16` family.
ModRM `05` decodes as `mod=00`, `reg=0`, `r/m=5`, so it reads a 16-bit selector from the 32-bit displacement source `0x021A65E4` and loads it into guest `ES`.

As with the earlier register-source segment load, the host segment register is not modified. Only guest segment shadow state is updated.

## Implementation Direction

Extend the existing segment load HLE in the Win32 execution trampoline.

This step adds only this form:

* `8E /r` after optional instruction prefixes
* `mod=00`, `r/m=5` absolute 32-bit displacement memory source
* source is a 2-byte read inside relocated runtime memory
* target segment register is recorded in guest shadow state

Handling performs the following:

* Skip prefixes and decode opcode, ModRM, and displacement.
* Read the 16-bit selector inside relocated runtime memory.
* Record the target segment register and selector in guest segment shadow state.
* Record the last segment load source address in the execution attempt.
* Do not modify the real host segment register.

## Verification Criteria

* The Win32 x86 build should succeed.
* `dos4gw_hello` should still print `Hello, world!`.
* `piu_1st` should increase handled segment load count.
* `piu_1st` should continue past the previous `0x020F39C8` stop to the next stop.
