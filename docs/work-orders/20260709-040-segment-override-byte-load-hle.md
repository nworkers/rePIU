# segment override byte memory load HLE 작업 지시

## 작업 항목

1. `26 8A 4F FF` 중단 지점의 현재 register/selector 상태를 기준으로 처리 범위를 제한한다.
2. Win32 execution trampoline에 segment override byte load handler를 추가한다.
3. `ES:[EDI - 1]` effective offset을 계산하고 `ES:0x80`에 대해 `CL=0`을 반환한다.
4. execution attempt와 loader 로그에 segment memory load 처리 횟수와 마지막 source를 추가한다.
5. `scripts/test_all.ps1`로 전체 검증을 수행한다.
6. 작업 로그를 남긴다.

## 비목표

* 전체 descriptor/DPMI 구현
* 전체 DOS PSP 구현
* 범용 instruction decoder 구현

# Segment Override Byte Memory Load HLE Work Order

## Tasks

1. Limit the handling scope based on the current register/selector state at the `26 8A 4F FF` stop.
2. Add a segment override byte load handler to the Win32 execution trampoline.
3. Calculate the `ES:[EDI - 1]` effective offset and return `CL=0` for `ES:0x80`.
4. Add segment memory load count and last source details to execution attempts and loader logs.
5. Run full verification with `scripts/test_all.ps1`.
6. Leave a work log.

## Non-Goals

* Full descriptor/DPMI implementation.
* Full DOS PSP implementation.
* General instruction decoder implementation.
