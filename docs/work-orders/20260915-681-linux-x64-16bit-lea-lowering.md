# 작업 지시 20260915-681 — Linux x64 16비트 LEA lowering

## 배경

Task 680에서 `MOV SP,imm16`을 처리한 뒤 최신 dynamic trace는 object 3의
첫 미지원 명령으로 `67 66 8D 8C 24 00 E0 FF FF`를 보고했다. 16비트
code object에서 이는 `LEA ECX,[ESP-0x2000]`이다.

## 작업 범위

* mode16 `67 66 LEA r32,m32`의 검증된 부분집합을 classifier/lowerer에 추가한다.
* `66`을 제거하고 `67`을 유지한다.
* ModRM/SIB의 게스트 ESP를 x64 게스트 상태 레지스터 R15로 remap한다.
* compatibility/lowering/emission probe를 추가한다.
* 기존 mode32와 i386 동작 및 미지원 16비트 경계를 유지한다.

## 구현 순서

1. [x] 최신 branch와 Task 680 결과 확인.
2. [x] 첫 fail-closed 명령의 mode16 decode와 의미 확인.
3. [ ] 설계 문서 작성.
4. [ ] mode16 LEA classifier/lowerer 구현.
5. [ ] probe regression 추가.
6. [ ] Linux x64 build와 core probe 실행.
7. [ ] dynamic trace 재실행 및 다음 경계 기록.
8. [ ] analysis/work-log 갱신 후 커밋.

## 제한

* `0x0110000E` 또는 특정 displacement에 대한 주소 예외를 추가하지 않는다.
* 원본 guest bytes와 gameplay logic을 수정하지 않는다.
* 16비트 주소 계산, 16비트 destination LEA, segment override, PUSH/POP,
  MOV SS, far return, 일반 stack ABI는 이 작업에서 다루지 않는다.

## 완료 기준

1. 지정된 mode16 LEA 형식이 정확한 x64 bytes로 lowering된다.
2. ESP를 포함하는 addressing과 포함하지 않는 addressing의 정책이 분리되어
   검증된다.
3. 미지원 mode16 형식은 계속 boundary로 남는다.
4. Linux x64 core probe가 기존 regression과 함께 통과한다.

---

# Work Order 20260915-681 — Linux x64 16-bit LEA lowering

## Background

After Task 680 handled `MOV SP,imm16`, the latest dynamic trace reported
`67 66 8D 8C 24 00 E0 FF FF` as object 3's first unsupported instruction. In
the 16-bit code object this is `LEA ECX,[ESP-0x2000]`.

## Scope

* Add the proven subset of mode16 `67 66 LEA r32,m32` to the classifier/lowerer.
* Remove `66` and preserve `67`.
* Remap guest ESP in ModRM/SIB to the x64 guest-state register R15.
* Add compatibility, lowering, and emission probe regressions.
* Preserve existing mode32/i386 behavior and fail-closed unsupported 16-bit
  boundaries.

## Implementation order

1. [x] Confirm the latest branch and Task 680 results.
2. [x] Confirm the first fail-closed instruction's mode16 decode and meaning.
3. [x] Write the design documents.
4. [x] Implement the mode16 LEA classifier/lowerer.
5. [x] Add probe regressions.
6. [x] Run the Linux x64 build and core probe.
7. [x] Rerun the dynamic trace and record the next boundary.
8. [x] Update analysis/work log and commit.

## Limits

* Do not add an exception keyed to `0x0110000E` or a particular displacement.
* Do not modify original guest bytes or gameplay logic.
* 16-bit address calculation, 16-bit destination LEA, segment override,
  PUSH/POP, MOV SS, far return, and a general stack ABI are outside this task.

## Done criteria

1. The selected mode16 LEA form lowers to the exact x64 byte sequence.
2. Addressing with and without guest ESP is covered by policy and probes.
3. Unsupported mode16 forms remain boundaries.
4. The Linux x64 core probe passes with existing regressions.

## 검증 보정

실제 runtime trace에서는 `0x0110000E`가 `8D 8C 24 00` mode16 명령으로
요청되었다. `67 66 LEA` lowering과 probe는 완료되었지만, 이 실행 경로의
다음 작업은 16비트 주소 계산을 보존하는 별도 Task 682로 분리한다.

## Verification correction

The runtime trace requested `0x0110000E` as `8D 8C 24 00` in mode16. The
`67 66 LEA` lowering and its probes are complete; preserving 16-bit address
calculation is separated into Task 682.
