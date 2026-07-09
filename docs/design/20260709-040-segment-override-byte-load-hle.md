# segment override byte memory load HLE 설계

## 배경

현재 `piu_1st`는 relocated address `0x020F4D7D`에서 `26 8A 4F FF` 명령으로 중단된다.

명령 해석은 다음과 같다.

```asm
mov cl, byte ptr es:[edi - 1]
```

실행 당시 guest `ES` shadow selector는 `0x0024`이고 `EDI`는 `0x00000081`이다.
따라서 guest 관점의 offset은 `0x00000080`이다.

Win32 host는 실제 `ES` segment base를 DOS/4GW guest selector처럼 바꾸지 않는다.
그 결과 CPU가 host flat address `0x00000080`을 직접 읽으려 하면서 access violation이 발생한다.

이 중단점은 게임 로직 재구현 대상이 아니라 DOS/4GW 주변 low-memory/selector 환경을 HLE로 제공해야 하는 지점이다.

## 목표

* `26 8A 4F FF`를 segment override byte memory load HLE로 처리한다.
* 실제 host segment register는 변경하지 않는다.
* guest segment shadow state와 effective offset을 사용해 HLE 값을 반환한다.
* 현재 확인된 `ES:0x80` DOS command tail length read는 빈 command tail로 보고 `0`을 반환한다.
* 처리 결과를 execution attempt 로그에서 구분할 수 있도록 마지막 segment memory load 정보를 기록한다.

## 비목표

* 전체 selector descriptor table 구현
* 전체 DOS PSP/low-memory 구현
* 범용 x86 ModRM decoder 구현
* 원본 executable 코드 수정

## 설계

Win32 vectored exception handler의 segment HLE 경로에 `HandleSegmentOverrideByteLoadInstruction`을 추가한다.

이번 단계에서 지원하는 명령 형태는 하나로 제한한다.

```text
26 8A 4F FF
```

처리 절차:

1. opcode byte window가 `26 8A 4F FF`인지 확인한다.
2. segment override `0x26`을 guest `ES`로 해석한다.
3. ModRM `0x4F`를 `mod=01`, `reg=1(CL)`, `r/m=7(EDI)`로 해석한다.
4. signed displacement `0xFF`를 `-1`로 해석해 effective offset `EDI - 1`을 계산한다.
5. guest `ES` selector와 offset이 현재 지원 범위인지 확인한다.
6. `ES:0x80`은 DOS command tail length byte로 보고 `0`을 반환한다.
7. `CL`에 byte 값을 기록하고 `EIP`를 4만큼 진행시킨다.
8. handled segment memory load count와 마지막 source 정보를 기록한다.

지원하지 않는 selector/offset 조합은 처리하지 않고 기존 예외 관찰 경로로 넘긴다.

## 검증

* `scripts/test_all.ps1`가 성공해야 한다.
* `dos4gw_hello`는 기존처럼 `Hello, world!`를 출력해야 한다.
* `piu_1st`는 기존 `26 8A 4F FF` 중단점을 지나 다음 중단점까지 진행해야 한다.
* loader 로그에 handled segment memory load count와 마지막 source `ES:0x00000080`이 출력되어야 한다.

# Segment Override Byte Memory Load HLE Design

## Background

`piu_1st` currently stops at relocated address `0x020F4D7D` on instruction `26 8A 4F FF`.

The instruction decodes as:

```asm
mov cl, byte ptr es:[edi - 1]
```

At that point, the guest `ES` shadow selector is `0x0024` and `EDI` is `0x00000081`.
The guest effective offset is therefore `0x00000080`.

The Win32 host does not change the real `ES` segment base to match the DOS/4GW guest selector.
As a result, the CPU tries to read host flat address `0x00000080`, causing an access violation.

This stop is not a gameplay reimplementation target. It is a point where the DOS/4GW surrounding low-memory/selector environment must be provided through HLE.

## Goals

* Handle `26 8A 4F FF` as a segment override byte memory load HLE.
* Do not modify the real host segment register.
* Return an HLE value using guest segment shadow state and effective offset.
* Treat the currently observed `ES:0x80` DOS command tail length read as an empty command tail and return `0`.
* Record the last segment memory load in the execution attempt logs.

## Non-Goals

* Implementing the full selector descriptor table.
* Implementing the full DOS PSP/low-memory model.
* Implementing a general x86 ModRM decoder.
* Modifying the original executable code.

## Design

Add `HandleSegmentOverrideByteLoadInstruction` to the segment HLE path in the Win32 vectored exception handler.

This step supports exactly one instruction form:

```text
26 8A 4F FF
```

Handling sequence:

1. Confirm that the opcode byte window is `26 8A 4F FF`.
2. Interpret segment override `0x26` as guest `ES`.
3. Decode ModRM `0x4F` as `mod=01`, `reg=1(CL)`, `r/m=7(EDI)`.
4. Interpret signed displacement `0xFF` as `-1` and calculate effective offset `EDI - 1`.
5. Check whether the guest `ES` selector and offset are in the currently supported scope.
6. Treat `ES:0x80` as the DOS command tail length byte and return `0`.
7. Write the byte value to `CL` and advance `EIP` by 4.
8. Record handled segment memory load count and last source details.

Unsupported selector/offset combinations are left unhandled and flow to the existing exception observation path.

## Verification

* `scripts/test_all.ps1` must pass.
* `dos4gw_hello` must continue to print `Hello, world!`.
* `piu_1st` must pass the previous `26 8A 4F FF` stop and continue to the next stop.
* Loader logs must print handled segment memory load count and last source `ES:0x00000080`.
