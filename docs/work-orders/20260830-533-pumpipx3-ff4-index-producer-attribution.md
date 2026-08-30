# 20260830-533 pumpipx3 FF /4 index 값과 producer 후보 관측 작업 지시서

## 목적

Task 532의 pointer-change 결과를 SIB index/base register 값으로 세분화하고, runtime evidence와
정적 disassembly를 결합해 producer 후보를 좁힙니다. 예외 경계 계측만으로 producer instruction
자체를 확정하지 않습니다.

## 작업 범위

* target result에 SIB index/base register 값과 validity를 추가합니다.
* site/hotspot에 component last value와 change count를 추가합니다.
* live site line에 component 정보를 추가합니다.
* synthetic probe에서 index 변화와 base 불변을 검증합니다.
* Task 532와 같은 조건으로 `pumpipx3`, `pumpit1`을 재측정합니다.
* static probe로 site 주변 bounded disassembly를 확인하고 producer 후보를 기록합니다.
* target instruction의 순수 cycle 비용과 실제 producer 확정은 다음 작업으로 남깁니다.

## 구현 순서

1. Task 532 결과와 현재 target evaluator 경계를 확인합니다.
2. SIB component 값과 변화 상태를 설계합니다.
3. target result, site/hotspot, live reporter, synthetic probe를 수정합니다.
4. Win32 x86 Debug probe와 Linux i386 Release를 빌드합니다.
5. 두 타이틀을 동일 조건으로 측정하고 component 분류를 추출합니다.
6. 정적 disassembly를 runtime 결과와 대조하고 문서·로그를 갱신한 뒤 커밋합니다.

## 검증 기준

* 기존 Task 532 probe와 새 component probe가 모두 통과해야 합니다.
* synthetic index 변화는 index change count에, base 변화 없음은 base change count 0에
  반영되어야 합니다.
* Windows probe 및 Linux i386 Release build가 성공해야 합니다.
* 두 타이틀의 runtime component 및 static producer 후보가 기록되어야 합니다.
* 확인됨·추정·미확정 결과를 구분해야 합니다.

---

# 20260830-533 Work Order: FF /4 Index Values and Producer Candidates in Pumpipx3

## Objective

Refine Task 532's pointer-change result with SIB index/base register values and narrow producer
candidates by correlating runtime evidence with static disassembly. The producer instruction itself
is not claimed to be proven by the exception-boundary sample.

## Scope

* Add SIB index/base values and validity to the target result.
* Add component history and change counts to sites and hotspots.
* Add component information to live site reporting.
* Verify index changes and base stability in the synthetic probe.
* Rerun `pumpipx3` and `pumpit1` under Task 532's identical conditions.
* Inspect bounded static disassembly around each site and record producer candidates.
* Leave pure target-instruction cycle cost and producer proof for a later unit.

## Implementation order

1. Review Task 532 and the current target-evaluator boundary.
2. Design SIB component values and change state.
3. Update the target result, site/hotspot, live reporter, and synthetic probe.
4. Build the Win32 x86 Debug probe and Linux i386 Release.
5. Measure both titles under identical conditions and extract component classification.
6. Correlate static disassembly with runtime results, update documents/log, and commit.

## Acceptance criteria

* Existing Task 532 and new component probe checks pass.
* A synthetic index change increments index-change count while base-change count remains zero.
* The Windows probe and Linux i386 Release build succeed.
* Runtime components and static producer candidates are recorded for both titles.
* Confirmed, inferred, and unresolved findings are distinguished.
