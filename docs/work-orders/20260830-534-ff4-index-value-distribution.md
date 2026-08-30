# 20260830-534 FF /4 index 값 분포 관측 작업 지시서

## 목적

Task 533의 마지막 index 관측을 site별 고정 용량 histogram으로 확장하여, `FF /4` pointer
변화가 실제로 어떤 index 값 집합에서 발생하는지 확인합니다. 이를 통해 중간 index 변경이
마지막 snapshot에 가려지는 문제를 제거합니다.

## 작업 범위

* resolved SIB index register/value slot과 관측 count를 site/hotspot에 추가합니다.
* slot overflow를 별도 count로 기록합니다.
* live reporter에 slot별 register/value/count를 출력합니다.
* synthetic probe에서 반복 index와 두 번째 index를 검증합니다.
* Task 533과 같은 조건으로 `pumpipx3`와 `pumpit1`을 재측정합니다.
* static jump-table index 및 producer 후보와 histogram을 대조합니다.
* 순수 target cycle 측정과 producer 확정은 후속 작업으로 남깁니다.

## 구현 순서

1. Task 533의 site/hotspot state와 live line 형식을 확인합니다.
2. 고정 용량 index histogram 구조와 overflow 정책을 설계합니다.
3. target attribution, hotspot copy, live reporter, synthetic probe를 수정합니다.
4. Win32 x86 Debug probe와 Linux i386 Release를 빌드합니다.
5. 두 title을 동일 조건에서 측정하고 histogram을 추출합니다.
6. analysis 문서와 작업 로그를 갱신하고 검증된 변경을 커밋합니다.

## 검증 기준

* 동일 index 반복은 기존 slot count만 증가합니다.
* 두 번째 index는 새 slot에 기록됩니다.
* index histogram 합계와 index component observation count가 일치합니다.
* 기존 probe 및 두 플랫폼 빌드가 통과합니다.
* 확인됨·추정·미확정 결과를 구분해 기록합니다.

---

# 20260830-534 Work Order: FF /4 Index-Value Distribution Observation

## Objective

Extend Task 533's last-index observation with a fixed-capacity per-site histogram, showing which
index values actually produce `FF /4` pointer changes and exposing transient values hidden by the
last live snapshot.

## Scope

* Add resolved SIB index register/value slots and observation counts to site/hotspot state.
* Record histogram overflow separately.
* Report each slot's register/value/count in the live site line.
* Verify repeated and second-index cases in the synthetic probe.
* Rerun pumpipx3 and pumpit1 under Task 533's identical conditions.
* Correlate histograms with static jump-table indexes and producer candidates.
* Defer pure target-cycle measurement and producer proof to later work.

## Implementation order

1. Review Task 533 site/hotspot state and live line format.
2. Define fixed-capacity index histogram and overflow behavior.
3. Update target attribution, hotspot copy, live reporter, and synthetic probe.
4. Build the Win32 x86 Debug probe and Linux i386 Release.
5. Measure both titles under identical conditions and extract histograms.
6. Update analysis and work-log documents, then commit the verified change.

## Acceptance criteria

* Repeated values increment an existing slot.
* A second index is recorded in a new slot.
* Index histogram totals equal index-component observation counts.
* Existing probes and both platform builds pass.
* Confirmed, inferred, and unresolved findings are distinguished.
