# 작업 지시 20260915-687 — Linux x64 mode16 MOV immediate lowering

## 배경

Task 686에서 mode16 Jcc를 통과한 뒤 object 3의 다음 frontier는
`0x01100009: B8 07 00`입니다. mode16에서는 `MOV AX,7`이지만 x64 long
mode에서 직접 실행하면 32비트 immediate로 해석되어 다음 bytes까지
소비합니다.

## 목표

* mode16 prefix-free `B8+r iw` immediate-to-GPR subset을 공용 lowering합니다.
* `66 B8+r iw`를 emit하여 low-word write와 upper GPR 보존을 확인합니다.
* guest SP `BC iw`는 기존 Task 680 R15W lowering을 계속 사용합니다.
* compatibility/lowering/core/runtime 검증을 남깁니다.

## 구현 순서

1. [x] Task 686 runtime frontier 확인
2. [x] mode16 MOV immediate 설계 작성
3. [ ] classifier/lowering kind 구현
4. [ ] byte-only lowering 구현
5. [ ] compatibility/lowering probe 구현
6. [ ] Linux x64 core probe 및 `repiu` 빌드
7. [ ] object-3 runtime trace에서 MOV immediate boundary 제거 확인
8. [ ] 다음 `89 CA` frontier 기록 및 작업 로그/커밋

## 완료 기준

* `B8 07 00`이 `k16BitMovImmediateToGuestGprs`로 분류됩니다.
* lowering 결과가 `66 B8 07 00`이고 instruction count가 1입니다.
* 실제 x64 실행에서 low-word immediate와 upper GPR 보존이 확인됩니다.
* `BC 00 20`은 기존 Task 680 R15W lowering을 계속 사용합니다.
* prefix/length/unsupported 변형은 새 lowering을 사용하지 않습니다.
* core probe가 failure 없이 통과합니다.
* runtime의 다음 SIGTRAP이 `0x01100015: 89 CA`로 이동합니다.
* 특정 주소나 immediate 값 예외가 추가되지 않습니다.

## 제약

* 원본 guest executable bytes는 수정하지 않습니다.
* guest SP, stack/segment/far-return semantics는 별도 lowering으로 유지합니다.

---

# Work Order 20260915-687 — Linux x64 mode16 MOV immediate lowering

## Background

After Task 686 passed mode16 Jcc, the next object-3 frontier is
`0x01100009: B8 07 00`. In mode16 it is `MOV AX,7`, but direct long-mode
execution treats it as a 32-bit immediate and consumes the following bytes.

## Objectives

* Add a shared lowering for the prefix-free mode16 `B8+r iw`
  immediate-to-GPR subset.
* Emit `66 B8+r iw` and verify low-word writes and upper-GPR preservation.
* Keep guest-SP `BC iw` on Task 680's existing R15W lowering.
* Leave compatibility, lowering, core, and runtime evidence.

## Implementation sequence

1. [x] Confirm the Task 686 runtime frontier.
2. [x] Write the mode16 MOV-immediate design.
3. [x] Implement the classifier/lowering kind.
4. [x] Implement the byte-only lowering.
5. [x] Add compatibility/lowering probe coverage.
6. [x] Build the Linux x64 core probe and `repiu`.
7. [x] Confirm MOV-immediate boundary removal in the object-3 runtime trace.
8. [x] Record the next `89 CA` frontier and write the work log/commit.

## Done criteria

* `B8 07 00` classifies as `k16BitMovImmediateToGuestGprs`.
* The lowering output is `66 B8 07 00` with instruction count one.
* Actual x64 execution verifies the low-word immediate and upper-GPR state.
* `BC 00 20` continues to use Task 680's R15W lowering.
* Prefix, length, and unsupported variants do not use the new lowering.
* The core probe passes with zero failures.
* The next runtime SIGTRAP moves to `0x01100015: 89 CA`.
* No address- or immediate-specific exception is added.

## Constraints

* Do not modify original guest executable bytes.
* Keep guest-SP, stack, segment, and far-return semantics in separate lowerings.
