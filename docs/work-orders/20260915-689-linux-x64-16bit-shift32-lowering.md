# 작업 지시 20260915-689 — Linux x64 mode16 32-bit shift lowering

## 배경

Task 688 이후 object 3의 다음 frontier는 `0x01100017: 66 C1 E9 10`입니다.
mode16의 32비트 shift가 long mode에서 16비트로 좁아지는 문제를 공통
operand-size lowering으로 처리합니다.

## 목표

* mode16 `66 C1 /r ib` register-only 32비트 shift subset을 분류합니다.
* operand-size prefix를 제거해 long mode의 32비트 shift semantics를 보존합니다.
* guest SP, memory, address/prefix 변형은 fail-closed로 유지합니다.
* compatibility/lowering/core/runtime 증거를 남깁니다.

## 구현 순서

1. [x] Task 688 runtime frontier 확인
2. [x] mode16 32-bit shift lowering 설계 작성
3. [ ] classifier/lowering kind 구현
4. [ ] byte-only lowering 구현
5. [ ] compatibility/lowering probe 추가
6. [ ] Linux x64 core probe와 `repiu` 빌드
7. [ ] object-3 shift boundary 제거 확인
8. [ ] 다음 frontier와 작업 로그 기록 및 커밋

## 완료 기준

* `66 C1 E9 10`이 `k16BitShift32ToGuestGprs`로 분류됩니다.
* lowering 결과가 `C1 E9 10`이고 instruction count가 1입니다.
* 실제 x64 실행에서 32비트 shift 결과를 확인합니다.
* guest SP/memory/prefix/address 변형은 새 lowering을 사용하지 않습니다.
* core probe가 failure 없이 통과합니다.
* runtime이 `CD 31` HLE 경계를 통과하고 다음 frontier로 이동합니다.
* 특정 주소나 특정 shift count에 대한 예외처리를 추가하지 않습니다.

## 제약

* 원본 guest executable bytes를 수정하지 않습니다.
* guest SP/stack/segment/far-return semantics는 독립 lowering으로 분리합니다.

---

# Work Order 20260915-689 — Linux x64 mode16 32-bit shift lowering

## Background

The next object-3 frontier after Task 688 is `0x01100017: 66 C1 E9 10`.
Handle the narrowing of the mode16 32-bit shift to a 16-bit long-mode shift
through a shared operand-size lowering.

## Objectives

* Classify the mode16 `66 C1 /r ib` register-only 32-bit shift subset.
* Remove the operand-size prefix so long mode preserves the 32-bit shift
  semantics.
* Keep guest-SP, memory, and address/prefix variants fail-closed.
* Leave compatibility, lowering, core, and runtime evidence.

## Implementation sequence

1. [x] Confirm the Task 688 runtime frontier.
2. [x] Write the mode16 32-bit shift design.
3. [ ] Implement the classifier/lowering kind.
4. [ ] Implement the byte-only lowering.
5. [ ] Add compatibility/lowering probe coverage.
6. [ ] Build the Linux x64 core probe and `repiu`.
7. [ ] Confirm removal of the object-3 shift boundary.
8. [ ] Record the next frontier and write the work log/commit.

## Done criteria

* `66 C1 E9 10` classifies as `k16BitShift32ToGuestGprs`.
* The lowering output is `C1 E9 10` with instruction count one.
* Actual x64 execution verifies the 32-bit shift result.
* Guest-SP, memory, prefix, and address variants do not use the new lowering.
* The core probe passes with zero failures.
* The runtime passes the `CD 31` HLE boundary and stops at the next frontier.
* No address- or shift-count-specific exception is added.

## Constraints

* Do not modify original guest executable bytes.
* Keep guest SP, stack, segment, and far-return semantics in separate lowerings.
