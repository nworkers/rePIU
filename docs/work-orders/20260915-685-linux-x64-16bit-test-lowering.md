# 작업 지시 20260915-685 — Linux x64 mode16 TEST lowering

## 배경

Task 684에서 mode16 original-byte fallback을 차단한 결과, 실제 dynamic
image의 첫 unsupported instruction은 `0x01100004: 66 85 FF`가 되었습니다.
이 bytes는 mode16에서는 32비트 `TEST EDI,EDI`이지만 long mode에 그대로
들어가면 다른 operand width를 선택합니다.

## 목표

* mode16 prefix-free `66 85 /r` register-register TEST를 공용 lowering으로
  분류합니다.
* `66`을 제거한 `85 /r`를 x64 cache에 emit하여 flags와 GPR state를 보존합니다.
* memory, ESP, segment, address-size 변형은 검증 전까지 boundary로 유지합니다.
* compatibility/lowering/core/runtime 검증을 남깁니다.

## 범위

1. `LongModeLowering`에 mode16 TEST lowering kind를 추가합니다.
2. mode-aware classifier에 opcode/prefix/ModRM/ESP 조건을 추가합니다.
3. byte-only lowerer에 `66` 제거 경로를 추가합니다.
4. compatibility 및 x64 lowering probe를 확장합니다.
5. Linux x64 core probe와 object-3 runtime trace로 다음 frontier를 확인합니다.
6. 설계·작업 지시·작업 로그·analysis 문서를 갱신합니다.

## 구현 순서

1. [x] Task 684 mode-aware re-entry 결과 확인
2. [x] mode16 TEST lowering 설계 작성
3. [x] classifier/lowering kind 구현
4. [x] byte-only emitter 구현
5. [x] compatibility/lowering probe 구현
6. [x] Linux x64 core probe 및 `repiu` 빌드
7. [x] object-3 runtime trace에서 TEST boundary 제거 확인
8. [x] 다음 Jcc frontier 기록 및 작업 로그/커밋

## 완료 기준

* `66 85 FF`가 mode16 `k16BitTest32ToGuestGprs`로 분류됩니다.
* lowering 결과가 `85 FF`이고 instruction count가 1입니다.
* 실제 x64 실행에서 TEST의 ZF/CF/OF와 upper register state가 보존됩니다.
* unsupported memory/ESP/prefix 변형은 새 lowering을 사용하지 않습니다.
* core probe가 failure 없이 통과합니다.
* runtime의 다음 SIGTRAP이 TEST 이후 `0x01100007: 74 39` Jcc boundary로
  이동합니다.
* 특정 주소 예외가 추가되지 않습니다.

## 제약

* 원본 guest executable bytes는 수정하지 않습니다.
* TEST lowering은 register-register, no-ESP subset으로 제한합니다.
* mode16 Jcc, MOV AX immediate, stack/segment/far-return semantics는 다음
  작업으로 분리합니다.

---

# Work Order 20260915-685 — Linux x64 mode16 TEST lowering

## Background

Task 684 blocked the mode16 original-byte fallback, exposing the first
unsupported dynamic-image instruction at `0x01100004: 66 85 FF`. These bytes
are 32-bit `TEST EDI,EDI` in mode16 but select a different operand width when
copied directly into long mode.

## Objectives

* Classify prefix-free mode16 `66 85 /r` register-register TEST through a shared
  lowering.
* Emit `85 /r` in the x64 cache while preserving flags and GPR state.
* Keep memory, ESP, segment, and address-size variants as boundaries until
  proven.
* Leave compatibility, lowering, core, and runtime verification evidence.

## Scope

1. Add a mode16 TEST lowering kind to `LongModeLowering`.
2. Add opcode/prefix/ModRM/ESP conditions to the mode-aware classifier.
3. Add the `66` removal path to the byte-only lowerer.
4. Extend compatibility and x64 lowering probes.
5. Build the Linux x64 core probe and `repiu`, then trace object 3.
6. Update design, work order, work log, and analysis documentation.

## Implementation sequence

1. [x] Confirm the Task 684 mode-aware re-entry result.
2. [x] Write the mode16 TEST lowering design.
3. [x] Implement the classifier/lowering kind.
4. [x] Implement the byte-only emitter path.
5. [x] Add compatibility/lowering probe coverage.
6. [x] Build the Linux x64 core probe and `repiu`.
7. [x] Confirm TEST boundary removal in the object-3 runtime trace.
8. [x] Record the next Jcc frontier and write the work log/commit.

## Done criteria

* `66 85 FF` classifies as mode16 `k16BitTest32ToGuestGprs`.
* The lowering output is `85 FF` with instruction count one.
* Actual x64 execution preserves TEST ZF/CF/OF and upper register state.
* Unsupported memory/ESP/prefix variants do not use the new lowering.
* The core probe passes with zero failures.
* The next runtime SIGTRAP moves to the `0x01100007: 74 39` Jcc boundary after
  TEST.
* No address-specific exception is added.

## Constraints

* Do not modify original guest executable bytes.
* Limit TEST lowering to the register-register, no-ESP subset.
* Split mode16 Jcc, MOV AX immediate, and stack/segment/far-return semantics
  into later tasks.
