# 작업 지시 20260915-690 — Linux x64 mode16 AND accumulator lowering

## 배경

Task 689 이후 object 3의 다음 frontier는 `0x0110002A: 25 FF 0F`입니다.
mode16 accumulator AND가 long mode에서 32비트 immediate로 widening되는
문제를 공통 lowering으로 처리합니다.

## 목표

* mode16 prefix-free `25 iw` accumulator AND subset을 분류합니다.
* `66 25 iw`를 emit하여 AX word semantics와 boundary를 보존합니다.
* prefix/address/길이 변형은 fail-closed로 유지합니다.
* compatibility/lowering/core/runtime 증거를 남깁니다.

## 구현 순서

1. [x] Task 689 runtime frontier 확인
2. [x] mode16 accumulator AND lowering 설계 작성
3. [x] classifier/lowering kind 구현
4. [x] byte-only lowering 구현
5. [x] compatibility/lowering probe 추가
6. [x] Linux x64 core probe와 `repiu` 빌드
7. [x] object-3 AND boundary 제거 확인
8. [x] 다음 frontier와 작업 로그 기록 및 커밋

## 완료 기준

* `25 FF 0F`가 `k16BitAndAccumulatorImmediate`로 분류됩니다.
* lowering 결과가 `66 25 FF 0F`이고 instruction count가 1입니다.
* 실제 x64 실행에서 AX low word와 upper RAX bits를 확인합니다.
* prefix/address/길이 변형은 새 lowering을 사용하지 않습니다.
* core probe가 failure 없이 통과합니다.
* runtime이 `0E`를 다음 미지원 frontier로 기록합니다.
* 특정 주소나 특정 immediate 값에 대한 예외처리를 추가하지 않습니다.

## 제약

* 원본 guest executable bytes를 수정하지 않습니다.
* stack/segment/far-return semantics는 독립 lowering으로 분리합니다.

---

# Work Order 20260915-690 — Linux x64 mode16 AND accumulator lowering

## Background

The next object-3 frontier after Task 689 is `0x0110002A: 25 FF 0F`.
Handle the widening of the mode16 accumulator AND to a long-mode 32-bit
immediate through a shared lowering.

## Objectives

* Classify the prefix-free mode16 `25 iw` accumulator AND subset.
* Emit `66 25 iw` to preserve AX word semantics and the boundary.
* Keep prefixed and address/length variants fail-closed.
* Leave compatibility, lowering, core, and runtime evidence.

## Implementation sequence

1. [x] Confirm the Task 689 runtime frontier.
2. [x] Write the mode16 accumulator AND design.
3. [x] Implement the classifier/lowering kind.
4. [x] Implement the byte-only lowering.
5. [x] Add compatibility/lowering probe coverage.
6. [x] Build the Linux x64 core probe and `repiu`.
7. [x] Confirm removal of the object-3 AND boundary.
8. [x] Record the next frontier and write the work log/commit.

## Done criteria

* `25 FF 0F` classifies as `k16BitAndAccumulatorImmediate`.
* The lowering output is `66 25 FF 0F` with instruction count one.
* Actual x64 execution verifies AX low-word and upper-RAX state.
* Prefixed and address/length variants do not use the new lowering.
* The core probe passes with zero failures.
* The runtime records `0E` as the next unsupported frontier.
* No address- or immediate-specific exception is added.

## Constraints

* Do not modify original guest executable bytes.
* Keep stack, segment, and far-return semantics in separate lowerings.
