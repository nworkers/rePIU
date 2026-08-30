# 20260830-532 pumpipx3 FF /4 pointer·target 변경 분리 계측 작업 지시서

## 목적

Task 531의 `target_change_count`만으로는 FF /4 target 변화가 register/index 변화인지,
같은 memory pointer의 table dword 변화인지 구분할 수 없습니다. 이번 작업에서는 두 경우를
실행 의미를 바꾸지 않고 분리해 관측합니다.

## 작업 범위

* `AotFfBoundarySite`와 hotspot 복사 경로에 resolved memory pointer 변경 상태를 추가합니다.
* target 변화 전체 count는 유지하고, 동일 pointer target 변화와 pointer 동반 target 변화로
  분리합니다.
* register-form target은 pointer 변경 분류에서 제외하고 target 변화 전체 count만 유지합니다.
* live profile 한 줄에 pointer validity 및 분류 count를 추가합니다.
* synthetic probe에서 동일 pointer의 dword 변경과 다른 index 선택을 각각 검증합니다.
* Task 531과 같은 조건으로 `pumpipx3`, `pumpit1`을 재측정합니다.
* jump-table writer 추적과 target별 cycle 귀속은 이번 작업 범위에서 결정하지 않습니다.

## 구현 순서

1. Task 531의 현재 target attribution 및 문서 결론을 확인합니다.
2. 포인터 변경과 target 변경의 상태·분류 필드를 설계합니다.
3. 공용 attribution 상태, hotspot 복사, live reporter, synthetic probe를 수정합니다.
4. Win32 x86 Debug probe와 Linux i386 Release 실행 파일을 빌드합니다.
5. 동일한 trace-free 60초 조건으로 두 타이틀을 측정하고 live line을 추출합니다.
6. 확인됨·추정·미확정 내용을 분석 문서와 작업 로그에 기록하고 커밋합니다.

## 검증 기준

* 기존 Task 531 probe 검사와 새 pointer/target 분류 검사가 모두 통과해야 합니다.
* 동일 pointer에서 dword만 바꾼 synthetic sample은 `target_change_with_same_pointer_count`에
  기록되어야 합니다.
* index를 바꾼 synthetic sample은 `pointer_change_count` 및
  `target_change_with_pointer_change_count`에 기록되어야 합니다.
* Linux i386 Release 빌드가 성공해야 합니다.
* 두 타이틀에서 resolved/unresolved와 site별 pointer/target 분류를 확보해야 합니다.

## 산출물

* 설계: `docs/design/20260830-532-pumpipx3-ff4-pointer-target-change-attribution.md`
* 분석 누적: `docs/analysis/current-execution-frontier.md`,
  `docs/analysis/runtime-aot-dynamic-translation.md`
* 작업 로그: `docs/work-logs/20260830-532-pumpipx3-ff4-pointer-target-attribution.md`

---

# 20260830-532 Work Order: Separating FF /4 Pointer and Target Changes in Pumpipx3

## Objective

Task 531's `target_change_count` cannot distinguish a register/index change from a dword mutation
at the same memory pointer. This unit observes those cases separately without changing execution
semantics.

## Scope

* Add resolved memory-pointer history and change state to `AotFfBoundarySite` and hotspot copies.
* Keep the total target-change count and split it into same-pointer and pointer-accompanied target
  changes.
* Exclude register-form targets from pointer-change classification while retaining their total target
  changes.
* Add pointer validity and classification counts to one live-profile line.
* Verify same-pointer dword mutation and alternate-index selection in the synthetic probe.
* Rerun `pumpipx3` and `pumpit1` under Task 531's identical conditions.
* Leave jump-table writer attribution and per-target cycle attribution unresolved in this unit.

## Implementation order

1. Review Task 531's target attribution and current documented conclusion.
2. Design pointer-change and target-change state/classification fields.
3. Update shared attribution state, hotspot copying, live reporting, and the synthetic probe.
4. Build the Win32 x86 Debug probe and Linux i386 Release executable.
5. Measure both titles under identical trace-free 60-second conditions and extract live lines.
6. Record confirmed, inferred, and unresolved findings in the analysis documents and work log, then
   commit the task.

## Acceptance criteria

* Existing Task 531 probe checks and the new pointer/target classification checks pass.
* A synthetic dword change at an unchanged pointer increments
  `target_change_with_same_pointer_count`.
* A synthetic index change increments `pointer_change_count` and
  `target_change_with_pointer_change_count`.
* The Linux i386 Release build succeeds.
* Resolved/unresolved results and per-site pointer/target classifications are captured for both
  titles.

## Deliverables

* Design: `docs/design/20260830-532-pumpipx3-ff4-pointer-target-change-attribution.md`
* Cumulative analysis: `docs/analysis/current-execution-frontier.md` and
  `docs/analysis/runtime-aot-dynamic-translation.md`
* Work log: `docs/work-logs/20260830-532-pumpipx3-ff4-pointer-target-attribution.md`
