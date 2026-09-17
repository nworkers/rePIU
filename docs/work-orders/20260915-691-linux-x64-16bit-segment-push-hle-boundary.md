# 작업 지시 20260915-691 — Linux x64 mode16 segment push HLE boundary

## 배경

Task 690 이후 object 3의 frontier `0x0110002D: 0E`는 mode16 `PUSH CS`입니다.
기존 CS segment-push HLE가 AOT planner에서 사용되도록 boundary 연결만
추가합니다.

## 목표

* mode16 prefix-free segment push를 기존 `kHleBoundary`로 계획합니다.
* 기존 `HandleSegmentPushInstruction`의 selector/guest-stack semantics를
  재사용합니다.
* mode32 native policy와 prefixed 변형은 변경하지 않습니다.
* compatibility/core/runtime 증거를 남깁니다.

## 구현 순서

1. [x] Task 690 runtime frontier 확인
2. [x] 기존 segment-push HLE와 planner 흐름 조사
3. [x] mode16 HLE boundary 설계 작성
4. [x] planner predicate 및 probe 구현
5. [x] Linux x64 core probe와 `repiu` 빌드
6. [x] runtime에서 `0E` HLE 경로 확인
7. [x] 다음 `50` frontier와 작업 로그 기록 및 커밋

## 완료 기준

* synthetic mode16 `0E` record가 `kHleBoundary`입니다.
* core probe가 failure 없이 통과합니다.
* runtime plan trace가 `0E ... hle=1`을 기록합니다.
* 기존 segment-push HLE를 통해 `0E` 이후 `0x0110002E: 50` frontier에
  도달합니다.
* 특정 guest 주소에 대한 예외처리를 추가하지 않습니다.

## 제약

* segment selector/stack semantics를 새로 구현하지 않습니다.
* mode32 native segment-push와 prefixed segment push 정책을 변경하지 않습니다.

---

# Work Order 20260915-691 — Linux x64 mode16 segment push HLE boundary

## Background

The object-3 frontier after Task 690, `0x0110002D: 0E`, is mode16 `PUSH CS`.
Connect the existing CS segment-push HLE to the AOT planner as a boundary.

## Objectives

* Plan prefix-free mode16 segment pushes as `kHleBoundary`.
* Reuse existing selector and guest-stack semantics in
  `HandleSegmentPushInstruction`.
* Keep the mode32 native policy and prefixed variants unchanged.
* Leave compatibility, core, and runtime evidence.

## Implementation sequence

1. [x] Confirm the Task 690 runtime frontier.
2. [x] Inspect existing segment-push HLE and planner flow.
3. [x] Write the mode16 HLE-boundary design.
4. [x] Implement the planner predicate and probe.
5. [x] Build the Linux x64 core probe and `repiu`.
6. [x] Confirm the `0E` HLE path at runtime.
7. [x] Record the next `50` frontier and write the work log/commit.

## Done criteria

* A synthetic mode16 `0E` record is `kHleBoundary`.
* The core probe passes with zero failures.
* The runtime plan trace records `0E ... hle=1`.
* Existing segment-push HLE reaches the `0x0110002E: 50` frontier after `0E`.
* No guest-address-specific exception is added.

## Constraints

* Do not implement new selector or stack semantics.
* Do not change mode32 native segment-push or prefixed segment-push policy.
