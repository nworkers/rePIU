# 20260830-536 FF /4 target hotspot timing 경계 확인 작업 지시서

## 목적

Task 535에서 남은 resolved target cycle 비용 질문에 대해, 기존 `SingleStepHotspotProfile`이
실제 target 주소를 포착하는지 먼저 확인합니다. 이를 통해 추가 계측 없이 얻을 수 있는
관측과 별도 timing boundary가 필요한 영역을 구분합니다.

## 작업 범위

* 코드는 수정하지 않고 기존 `repiu` binary로 측정합니다.
* Task 535와 동일한 환경에 single-step hotspot profile과 dump 경로를 추가합니다.
* `pumpipx3` target `0x010EF6E6`, `0x010EF8E9`를 확인합니다.
* `pumpit1` target `0x010F1CFD`, `0x010F1CF4`를 확인합니다.
* dump의 sample/cycle/outcome/stage를 기록합니다.
* hotspot cycle을 pure target cycle로 해석하지 않습니다.

## 실행 순서

1. Task 535 branch와 binary 상태 및 dirty user artifact를 확인합니다.
2. 위 target 주소와 기존 single-step profile 경계를 설계 문서에 고정합니다.
3. `pumpipx3`, `pumpit1`을 각각 60초 실행하고 title별 hotspot dump를 수집합니다.
4. dump에서 target 후보 주소의 entry와 cycle/stage 데이터를 추출합니다.
5. FF live target/transition 결과 및 cleanup 상태와 대조합니다.
6. 기존 경계가 target 주소를 관측하는지 판정하고 analysis/work-log를 갱신합니다.
7. 문서 변경을 커밋합니다.

## 검증 기준

* 두 title의 hotspot dump가 생성되어야 합니다.
* target 주소별 observed/not observed를 명시해야 합니다.
* dump cycle의 의미가 `HandleSingleStepTrace` handler window임을 문서에 남겨야 합니다.
* target resolution failure와 dump/cleanup limitation을 구분해야 합니다.
* pure target cycle과 late-drop causality는 미확정으로 유지해야 합니다.

---

# 20260830-536 Work Order: FF /4 Target Hotspot Timing-Boundary Check

## Objective

First determine whether the existing `SingleStepHotspotProfile` observes the resolved target
addresses relevant to the unresolved Task 535 target-cycle question. This separates evidence
available without new instrumentation from evidence requiring a dedicated timing boundary.

## Scope

* Make no code changes; measure with the existing `repiu` binary.
* Add only the single-step hotspot profile and dump path to Task 535's runtime conditions.
* Check pumpipx3 targets `0x010EF6E6`, `0x010EF8E9`.
* Check pumpit1 targets `0x010F1CFD`, `0x010F1CF4`.
* Record dump sample/cycle/outcome/stage data.
* Do not interpret hotspot cycles as pure target cycles.

## Execution order

1. Confirm the Task 535 branch, binaries, and dirty user artifacts.
2. Fix the target addresses and existing single-step boundary in the design.
3. Run each title for 60 seconds and collect a title-specific hotspot dump.
4. Extract target-candidate entries and cycle/stage data from each dump.
5. Compare with FF live target/transition results and cleanup state.
6. Decide whether the existing boundary observes target addresses and update analysis/work-log.
7. Commit the documentation changes.

## Acceptance criteria

* Both title hotspot dumps are generated.
* Observed/not-observed status is explicit for every target candidate.
* The dump cycle meaning is documented as the `HandleSingleStepTrace` handler window.
* Target-resolution failure is distinguished from dump/cleanup limitations.
* Pure target cycles and late-drop causality remain unresolved.
