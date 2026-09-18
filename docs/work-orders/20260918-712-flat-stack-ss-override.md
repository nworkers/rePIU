# Task 712 작업 지시 — flat 스택 선택자의 명시적 SS override

설계: [20260918-712](../design/20260918-712-flat-stack-ss-override.md)

## 범위

loader의 초기 스택 선택자 아래에서 명시적 `SS:` override가 base 0으로 fold되게
한다. 다른 SS, DS/ES, 선택자 표의 descriptor, 텍스처 문제는 건드리지 않는다.

## 단계

1. `runtime::ApplyFlatStackSegmentFold` — 순수 규칙 함수
   (`aot_segment_patch.h/.cpp`)
2. `ThreadContext::flat_stack_selector`, 실행 시작 때 `guest_initial_ss`로 설정
3. `BuildAotSegmentTable`에서 SS 항목에 규칙 적용
4. core probe `flat_stack_segment_fold` 신규
5. Win32 수정 전후 비교, 작업 로그, frontier, EXE_DESIGN

## 검증

* 두 host core probe 전체
* Linux 30초: DOS read/seek/open, 폴트, gate #51
* Win32 전후(같은 조건): 파일 연산, PIU.BIN seek, gate 열 96개, 삼각형 12개,
  LFB, frames, 폴트

## 완료 조건

* Linux 읽기가 Win32 수준으로 떨어진다.
* Win32 비교에서 회귀가 없거나, 차이가 정당한 결과로 설명된다.

---

## English

Design: [20260918-712](../design/20260918-712-flat-stack-ss-override.md)

### Scope

Make an explicit `SS:` override fold base 0 under the loader's initial stack
selector. Other SS values, DS/ES, the selector table's descriptors, and the texture
problem are untouched.

### Steps

1. `runtime::ApplyFlatStackSegmentFold`, a pure rule (`aot_segment_patch.h/.cpp`).
2. `ThreadContext::flat_stack_selector`, set to `guest_initial_ss` at start.
3. Apply the rule to the SS entry in `BuildAotSegmentTable`.
4. A new `flat_stack_segment_fold` core-probe group.
5. The Win32 before/after comparison, work log, frontier and EXE_DESIGN.

### Verification

* Every core-probe group on both hosts.
* Linux 30 s: DOS read/seek/open counts, faults, gate #51.
* Win32 before/after under identical conditions: file operations, PIU.BIN seeks,
  all 96 gates, 12 triangles, LFB, frames, faults.

### Done when

* Linux reads fall to the Win32 level.
* The Win32 comparison shows no regression, or each difference is explained as a
  legitimate consequence.
