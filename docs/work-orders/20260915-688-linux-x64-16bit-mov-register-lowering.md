# 작업 지시 20260915-688 — Linux x64 mode16 MOV register lowering

## 배경

Task 687 이후 object 3의 다음 mode16 frontier는 `0x01100015: 89 CA`입니다.
mode16의 `MOV DX,CX`가 long mode에서 32비트 이동으로 넓어지는 문제를
공통 register-register lowering으로 처리합니다.

## 목표

* mode16 prefix-free `89/8B /r` register-register subset을 분류합니다.
* guest SP가 포함되지 않은 형식은 `66`을 붙여 16비트 semantics를 보존합니다.
* guest SP, memory, prefix 변형은 별도 증명 전까지 fail-closed로 유지합니다.
* compatibility/lowering/core/runtime 증거를 남깁니다.

## 구현 순서

1. [x] Task 687 runtime frontier 확인
2. [x] mode16 MOV register lowering 설계 작성
3. [x] classifier/lowering kind 구현
4. [x] byte-only lowering 구현
5. [x] compatibility/lowering probe 추가
6. [x] Linux x64 core probe와 `repiu` 빌드
7. [x] object-3 `89 CA` boundary 제거 확인
8. [x] 다음 frontier와 작업 로그 기록 및 커밋

## 완료 기준

* `89 CA`와 `8B D1`이 `k16BitMovRegisterToGuestGprs`로 분류됩니다.
* lowering 결과가 각각 `66 89 CA`와 `66 8B D1`이고 instruction count가 1입니다.
* 실제 x64 실행에서 destination upper GPR bits가 보존됩니다.
* guest SP 또는 memory를 포함한 변형은 새 lowering을 사용하지 않습니다.
* core probe가 failure 없이 통과합니다.
* runtime SIGTRAP이 `89 CA` 이후 다음 미지원 frontier로 이동합니다.
* 특정 주소나 특정 register 값에 대한 예외처리를 추가하지 않습니다.

## 제약

* 원본 guest executable bytes를 수정하지 않습니다.
* guest SP/stack/segment/far-return semantics는 독립 lowering으로 분리합니다.

---

# Work Order 20260915-688 — Linux x64 mode16 MOV register lowering

## Background

The next mode16 object-3 frontier after Task 687 is `0x01100015: 89 CA`.
Handle its widening from mode16 `MOV DX,CX` to long-mode 32-bit movement through
a shared register-register lowering.

## Objectives

* Classify the prefix-free mode16 `89/8B /r` register-register subset.
* Add `66` to forms without guest SP so 16-bit semantics are preserved.
* Keep guest-SP, memory, and prefixed variants fail-closed until separately
  proven.
* Leave compatibility, lowering, core, and runtime evidence.

## Implementation sequence

1. [x] Confirm the Task 687 runtime frontier.
2. [x] Write the mode16 MOV-register design.
3. [x] Implement the classifier/lowering kind.
4. [x] Implement the byte-only lowering.
5. [x] Add compatibility/lowering probe coverage.
6. [x] Build the Linux x64 core probe and `repiu`.
7. [x] Confirm removal of the object-3 `89 CA` boundary.
8. [x] Record the next frontier and write the work log/commit.

## Done criteria

* `89 CA` and `8B D1` classify as `k16BitMovRegisterToGuestGprs`.
* Their lowerings are `66 89 CA` and `66 8B D1`, each with instruction count one.
* Actual x64 execution verifies destination upper-GPR preservation.
* Guest-SP and memory forms do not use the new lowering.
* The core probe passes with zero failures.
* The runtime SIGTRAP moves to the next unsupported frontier after `89 CA`.
* No address- or register-value-specific exception is added.

## Constraints

* Do not modify original guest executable bytes.
* Keep guest SP, stack, segment, and far-return semantics in separate lowerings.
