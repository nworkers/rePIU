# FS segment word memory load HLE 설계

## 배경

`INT 21h AH=19h` 현재 drive 조회를 통과한 뒤 `piu_1st`는 relocated base + `0x000F246F`에서 중단된다. 예외 바이트는 `66 65 8B 10 66 65 8B 40 02 ...`이다.

이는 DOS interrupt가 아니라 `FS` segment override가 붙은 `MOV r16, r/m16` word memory load 흐름이다. 현재 segment HLE는 DS 기반 DOS environment read와 ES byte override 일부만 처리하므로 이 패턴은 미분류 상태로 남는다.

## 설계

관측된 `66 65 8B /r` 형태를 제한적으로 처리한다.

* `0x66 0x65 0x8B` prefix/opcode 조합만 처리한다.
* source는 SIB 없는 32-bit base register + optional 8-bit displacement까지만 디코드한다.
* destination은 ModRM reg의 16-bit general register 하위 word에 쓴다.
* segment는 `FS` shadow selector를 사용한다.
* 현재 FS low-memory 모델은 없으므로, `FS` selector가 현재 guest FS이고 offset이 `0x10000` 미만인 word read는 `0`으로 반환한다.
* 처리 결과는 기존 segment memory load 로그에 기록한다.

이는 FS memory model을 완성하는 작업이 아니라, 관측된 low-offset word read를 안전하게 zero-fill하는 전진용 HLE이다.

## 기대 결과

`piu_1st`가 `FS:[0x42]`, `FS:[0x44]` word load 지점을 통과하고 다음 실제 요구사항을 드러내야 한다.

## 범위 밖

* 전체 FS descriptor/linear address 모델
* FS-backed real memory image
* SIB 또는 모든 ModRM addressing 지원

# FS Segment Word Memory Load HLE Design

## Background

After passing `INT 21h AH=19h` get current drive, `piu_1st` stops at relocated base + `0x000F246F`. The exception bytes are `66 65 8B 10 66 65 8B 40 02 ...`.

This is not a DOS interrupt. It is a `MOV r16, r/m16` word memory load with an `FS` segment override. The current segment HLE only handles DS-based DOS environment reads and a small ES byte override case, so this pattern remains unclassified.

## Design

Handle the observed `66 65 8B /r` form in a limited way.

* Handle only the `0x66 0x65 0x8B` prefix/opcode sequence.
* Decode only source operands using a 32-bit base register without SIB plus an optional 8-bit displacement.
* Write the result into the low word of the 16-bit general register selected by the ModRM reg field.
* Use the `FS` shadow selector.
* Because there is no full FS low-memory model yet, return `0` for word reads where the `FS` selector matches the current guest FS and the offset is below `0x10000`.
* Record the handled read through the existing segment memory load observation.

This is not a complete FS memory model. It is a forward-progress HLE for the observed low-offset word read.

## Expected Result

`piu_1st` should pass the `FS:[0x42]` and `FS:[0x44]` word load point and reveal the next real requirement.

## Out Of Scope

* Full FS descriptor/linear address modeling
* FS-backed real memory image
* SIB or all ModRM addressing forms
