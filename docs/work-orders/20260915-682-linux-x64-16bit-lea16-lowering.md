# 작업 지시 20260915-682 — Linux x64 16비트 주소 LEA lowering

## 배경

Task 681의 `67 66 LEA` 검증 후 실제 game trace는
`0x0110000E: 8D 8C 24 00`을 요청했습니다. 이는 mode16
`LEA CX,[SI+disp16]`이며, 현재는 x64 cache INT3에서 unhandled SIGTRAP으로
중단됩니다.

## 범위

* 공용 mode16 `LEA r16,m16` lowering subset을 추가합니다.
* SI/DI/BX 기반 address form과 displacement를 low-word semantics로
  계산합니다.
* R14D/R10D scratch와 `MOVZX`/`LEA`만 사용하여 flags를 보존합니다.
* compatibility/lowering/emission regression과 runtime trace를 추가합니다.
* BP/segment/미확정 form은 계속 boundary로 유지합니다.

## 구현 순서

1. [x] Task 681 runtime frontier와 실제 mode16 decode를 확인합니다.
2. [x] 16비트 address 계산과 flags 보존 설계를 작성합니다.
3. [x] classifier/lowerer에 공용 mode16 LEA16 subset을 추가합니다.
4. [x] compatibility/lowering/emission probe를 추가합니다.
5. [x] Linux x64 build와 core probe를 실행합니다.
6. [x] object 3 dynamic trace에서 INT3 제거와 다음 frontier를 확인합니다.
7. [x] analysis/work-log 갱신 후 커밋합니다.

## 제한

* 원본 guest code bytes와 gameplay logic은 수정하지 않습니다.
* `0x0110000E`, `0x0024`, 또는 특정 실행 순서에 대한 주소 예외를
  추가하지 않습니다.
* BP default-SS semantics, segment override, 16비트 PUSH/POP, MOV SS,
  far return 및 일반 stack ABI는 이 작업에서 추정하지 않습니다.

## 완료 기준

1. 실제 `8D 8C 24 00`이 mode16 LEA16 lowering으로 분류됩니다.
2. lowering이 exact bytes, low-word wrap, flags 보존을 probe로 통과합니다.
3. 지원하지 않는 mode16 form은 계속 fail-closed boundary입니다.
4. Linux x64 core probe가 기존 regression과 함께 통과합니다.

---

# Work Order 20260915-682 — Linux x64 16-bit address LEA lowering

## Background

After Task 681 proved the `67 66 LEA` lowering, the real game trace requested
`0x0110000E: 8D 8C 24 00`. This is mode16 `LEA CX,[SI+disp16]` and currently
stops at an unhandled SIGTRAP from an x64-cache INT3 boundary.

## Scope

* Add a shared mode16 `LEA r16,m16` lowering subset.
* Compute SI/DI/BX-based addresses and displacements with low-word semantics.
* Preserve flags using only scratch-register `MOVZX`/`LEA` operations.
* Add compatibility, lowering, emission, and runtime-trace verification.
* Keep BP, segment, and other unproven forms at boundaries.

## Implementation order

1. [x] Confirm the Task 681 runtime frontier and its mode16 decode.
2. [x] Write the 16-bit address and flags-preservation design.
3. [x] Add the shared mode16 LEA16 classifier/lowerer subset.
4. [x] Add compatibility, lowering, and emission regressions.
5. [x] Run the Linux x64 build and core probe.
6. [x] Confirm the object-3 trace no longer emits INT3 for `0x0110000E`.
7. [x] Update analysis/work log and commit.

## Limits

Do not modify original guest bytes or gameplay logic, add address-specific
exceptions, infer BP default-SS or segment semantics, or expand this task to
16-bit PUSH/POP, MOV SS, far return, or a general stack ABI.

## Done criteria

The actual bytes `8D 8C 24 00` classify as mode16 LEA16, probes verify exact
lowering, low-word wrapping, and flags preservation, unsupported forms remain
fail-closed, and the Linux x64 core probe passes.
