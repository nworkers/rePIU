# 20260830-531 pumpipx3 FF /4 displacement·resolved target 귀속 작업 지시서

설계: [20260830-531](../design/20260830-531-pumpipx3-ff4-target-attribution.md)

## 목표

Task 530에서 dominant site로 확인된 AOT `FF /4`의 displacement, memory operand pointer,
raw dword target을 관측합니다. 원본 실행 의미와 기존 site census의 count/overflow 의미는
그대로 유지합니다.

## 작업 순서

1. 기존 `AotFfBoundarySite`와 live snapshot 구조에 target 상태를 추가합니다.
2. 별도 bounded target evaluator에서 legacy prefix, ModRM/SIB, disp8/disp32를 decode합니다.
3. register target과 32-bit memory target을 읽기 전용으로 기록하고 실패 원인을 집계합니다.
4. `HandleAotReentry`의 기존 `kOther` 지점에서만 evaluator를 호출합니다.
5. `[repiu-live-ff-site]`에 마지막 displacement/pointer/target과 resolution counters를
   추가합니다.
6. Windows probe에 synthetic decode, memory read, target change, truncated/unreadable
   검사를 추가합니다.
7. Linux i386 Release build와 Windows probe를 실행합니다.
8. 동일한 60초 조건에서 `pumpipx3`와 `pumpit1`을 측정하고 dominant site를 비교합니다.
9. analysis, work log를 갱신하고 task commit을 남깁니다.

## 완료 기준

* 기존 Task 530 probe check와 새 target probe check가 모두 통과합니다.
* register와 SIB/disp32 target이 expected value로 관측됩니다.
* truncated, unsupported address-size/segment, unreadable 경로가 target을 추정하지 않고
  unresolved breakdown으로 집계됩니다.
* Linux i386 Release build가 통과합니다.
* 각 타이틀에서 최소 4개 live profile/site snapshot과 shutdown reason을 확보합니다.
* #3/#4/#5 구간에서 dominant site의 displacement, pointer, target, resolved/unresolved
  count를 기록합니다.
* 측정만으로 실행 경로 최적화나 원본 코드 수정을 적용하지 않습니다.

## 검증 제한

* evaluator는 최대 15바이트 observation window만 사용합니다.
* 명시적 non-CS segment와 16-bit address-size는 보수적으로 unresolved 처리합니다.
* target 값은 raw dword이며 실행 가능성 검증이나 dispatch 변경을 수행하지 않습니다.
* 표본별 formatted logging은 추가하지 않습니다.

---

# 20260830-531 Work Order: pumpipx3 FF /4 Displacement and Resolved-Target Attribution

Design: [20260830-531](../design/20260830-531-pumpipx3-ff4-target-attribution.md)

## Objective

Observe the displacement, memory-operand pointer, and raw dword target of the AOT `FF /4`
dominant sites identified by Task 530. Preserve original execution semantics and the existing
site census count/overflow meanings.

## Procedure

1. Extend the existing `AotFfBoundarySite` and live snapshot with target state.
2. Decode legacy prefixes, ModRM/SIB, and disp8/disp32 in a separate bounded target evaluator.
3. Record register and 32-bit memory targets read-only, with explicit failure reasons.
4. Invoke the evaluator only at the existing `kOther` point in `HandleAotReentry`.
5. Add the last displacement/pointer/target and resolution counters to `[repiu-live-ff-site]`.
6. Add synthetic decode, memory-read, target-change, truncated, and unreadable checks to the
   Windows probe.
7. Run the Linux i386 Release build and the Windows probe.
8. Measure both titles for 60 seconds under the same conditions and compare dominant sites.
9. Update analysis and work log, then leave a task commit.

## Acceptance criteria

* Existing Task 530 probe checks and all new target checks pass.
* Register and SIB/disp32 targets are observed with expected values.
* Truncated, unsupported address-size/segment, and unreadable paths do not infer targets and
  are included in the unresolved breakdown.
* Linux i386 Release build passes.
* At least four live profile/site snapshots and a shutdown reason are captured for each title.
* Dominant-site displacement, pointer, target, and resolved/unresolved counts are recorded for
  the #3/#4 intervals.
* No execution-path optimization or original-code change is applied from measurement alone.

## Verification limits

* The evaluator uses a maximum 15-byte observation window.
* Explicit non-CS segments and 16-bit address-size forms fail closed as unresolved.
* Targets are raw dwords; executability is not checked and dispatch is not changed.
* No formatted logging is added per sample.
