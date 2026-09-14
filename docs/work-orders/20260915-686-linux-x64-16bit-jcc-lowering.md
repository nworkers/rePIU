# 작업 지시 20260915-686 — Linux x64 mode16 conditional branch lowering

## 배경

Task 685에서 mode16 TEST를 통과한 뒤 object 3의 다음 frontier는
`0x01100007: 74 39` mode16 `JZ rel8`입니다. condition은 x64와 같지만
mode16 IP wrap과 code-object-relative target 때문에 원본 short bytes를
그대로 복사할 수 없습니다.

## 목표

* mode16 conditional branch의 target을 기존 planner rebasing 계약으로
  해석합니다.
* mode16 `kConditionalBranch`를 x64 `0F 8x rel32` slot으로 emit합니다.
* conditional direct-target와 block-fallthrough fixup을 기존 계약으로
  처리합니다.
* unresolved target은 기존 fail-closed neutralisation을 유지합니다.

## 범위

1. mode16 conditional branch만 long-mode direct-branch gate에 허용합니다.
2. planner mode16 target rebasing 회귀 검증을 추가합니다.
3. emission probe에서 JZ slot과 두 edge fixup을 검증합니다.
4. unresolved branch neutralisation을 회귀 검증합니다.
5. Linux x64 core probe·repiu 빌드와 object-3 runtime trace를 수행합니다.
6. 설계·작업 지시·작업 로그·analysis 문서를 갱신합니다.

## 구현 순서

1. [x] Task 685 runtime frontier 확인
2. [x] mode16 Jcc 설계 작성
3. [x] mode16 conditional branch gate 구현
4. [x] planner/emission probe 구현
5. [x] Linux x64 core probe 및 `repiu` 빌드
6. [x] object-3 runtime trace에서 Jcc boundary 제거 확인
7. [x] 다음 MOV AX frontier 기록 및 작업 로그/커밋

## 완료 기준

* mode16 `74 cb`가 planner에서 code-object base를 포함한 direct target을
  기록합니다.
* mode16 conditional branch가 `0F 8x rel32`로 emit됩니다.
* conditional target과 block fallthrough fixup이 독립적으로 resolve됩니다.
* unresolved target은 branch slot 전체를 INT3로 neutralise합니다.
* core probe가 failure 없이 통과합니다.
* runtime의 다음 SIGTRAP이 `0x01100009: B8 07 00`으로 이동합니다.
* 특정 주소나 단일 mnemonic 예외가 추가되지 않습니다.

## 제약

* 원본 executable bytes와 short displacement는 수정하지 않습니다.
* mode16 direct jump/call/return과 stack/segment/far-return은 다음 작업으로
  남깁니다.

---

# Work Order 20260915-686 — Linux x64 mode16 conditional branch lowering

## Background

After Task 685 passed mode16 TEST, the next object-3 frontier is
`0x01100007: 74 39`, a mode16 `JZ rel8`. Its condition matches x64, but mode16
IP wrapping and code-object-relative targets prevent copying the short bytes.

## Objectives

* Use the existing planner rebasing contract for mode16 conditional targets.
* Emit mode16 `kConditionalBranch` records as x64 `0F 8x rel32` slots.
* Resolve conditional direct-target and block-fallthrough fixups through the
  existing contracts.
* Preserve fail-closed neutralisation for unresolved targets.

## Scope

1. Allow only mode16 conditional branches through the long-mode direct-branch
   gate.
2. Add mode16 planner target-rebasing regression coverage.
3. Verify the JZ slot and both edge fixups in the emission probe.
4. Regress unresolved branch neutralisation.
5. Build the Linux x64 core probe and `repiu`, then trace object 3.
6. Update design, work order, work log, and analysis documentation.

## Implementation sequence

1. [x] Confirm the Task 685 runtime frontier.
2. [x] Write the mode16 Jcc design.
3. [x] Implement the mode16 conditional-branch gate.
4. [x] Implement planner/emission probe coverage.
5. [x] Build the Linux x64 core probe and `repiu`.
6. [x] Confirm Jcc boundary removal in the object-3 runtime trace.
7. [x] Record the next MOV AX frontier and write the work log/commit.

## Done criteria

* The planner records a code-object-base-inclusive direct target for mode16
  `74 cb`.
* A mode16 conditional branch emits as `0F 8x rel32`.
* Conditional-target and block-fallthrough fixups resolve independently.
* An unresolved target neutralises the complete branch slot with INT3.
* The core probe passes with zero failures.
* The next runtime SIGTRAP moves to `0x01100009: B8 07 00`.
* No address-specific or single-mnemonic exception is added.

## Constraints

* Do not modify original executable bytes or short displacements.
* Leave mode16 direct jump/call/return and stack/segment/far-return semantics
  for later tasks.
