# 20260830-535 FF /4 index 전환 순서 관측 작업 지시서

## 목적

Task 534 histogram에서 확인한 index 값들의 실제 전환 순서를 fixed trace로 기록합니다.
이를 통해 `pumpipx3`의 EDX=0/7 전환과 `pumpit1`의 EAX=3/2 전환 패턴을 비교합니다.

## 작업 범위

* resolved SIB index 변경의 from/to register/value를 site/hotspot에 기록합니다.
* 최대 32개 transition slot과 overflow count를 추가합니다.
* live reporter에 transition count와 저장된 전환을 출력합니다.
* synthetic probe에서 반복 index와 0→1 전환을 검증합니다.
* Task 534와 같은 조건으로 두 title을 재측정합니다.
* static jump-table target과 transition 순서를 대조합니다.
* 순수 target cycle 측정과 producer 확정은 후속 작업으로 남깁니다.

## 구현 순서

1. Task 534의 component-change와 histogram 갱신 지점을 확인합니다.
2. 고정 용량 transition 구조와 overflow 정책을 설계합니다.
3. site/hotspot, live reporter, synthetic probe를 수정합니다.
4. Win32 x86 Debug probe와 Linux i386 Release를 빌드합니다.
5. 두 title을 동일 조건에서 측정하고 transition trace를 추출합니다.
6. analysis 문서와 작업 로그를 갱신하고 검증된 변경을 커밋합니다.

## 검증 기준

* 동일 index 반복은 transition을 추가하지 않습니다.
* index 0→1 변경은 transition 하나로 기록됩니다.
* transition count가 index value change count와 일치합니다.
* slot과 overflow 합계가 전체 transition count와 일치합니다.
* 기존 probe 및 두 플랫폼 빌드가 통과합니다.
* 확인됨·추정·미확정 결과를 구분해 기록합니다.

---

# 20260830-535 Work Order: FF /4 Index Transition Order Observation

## Objective

Record the transition order of index values found by Task 534's histogram, comparing pumpipx3's
EDX=0/7 pattern with pumpit1's EAX=3/2 pattern.

## Scope

* Record from/to index register/value for resolved SIB index changes per site and hotspot.
* Add up to 32 fixed transition slots and an overflow count.
* Report transition counts and stored transitions in the live line.
* Verify repeated values and a 0→1 transition in the synthetic probe.
* Rerun both titles under Task 534's identical conditions.
* Correlate transition order with static jump-table targets.
* Defer pure target-cycle measurement and producer proof.

## Implementation order

1. Review Task 534's component-change and histogram update points.
2. Define the fixed transition structure and overflow policy.
3. Update site/hotspot state, live reporter, and synthetic probe.
4. Build the Win32 x86 Debug probe and Linux i386 Release.
5. Measure both titles under identical conditions and extract transition traces.
6. Update analysis and work-log documents, then commit the verified change.

## Acceptance criteria

* Repeated values create no transition.
* An index 0→1 change creates one transition.
* Transition count equals index-value change count.
* Stored plus overflow transitions equal total transitions.
* Existing probes and both platform builds pass.
* Confirmed, inferred, and unresolved findings are distinguished.
