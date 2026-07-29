# 20260730-363 Glide 호출 증가 성능 개선 계획 작업 지시 / Work order

* 설계: [20260730-363-glide-call-performance-plan.md](../design/20260730-363-glide-call-performance-plan.md)

## 한국어

### 목표

최신 Release Glide profile을 근거로 호출량 증가 시 FPS 저하의 병목과 후속 작업 순서를
문서화하고, `current-execution-frontier.md`를 다음 세션에서 바로 재개할 수 있는 상태로
갱신합니다. 이번 작업에서는 성능 코드를 변경하지 않습니다.

### 작업

1. `repiu_glide_profile_release_20260730_030212.txt`의 전체 실행 축, ordinal별
   gate/backend 시간, swap 세부 시간과 호출 횟수를 재검증합니다.
2. 과거 Task 354/355의 LFB 우선순위가 이번 장면에는 적용되지 않는 이유를 기록합니다.
3. Task 364 상태 반복률/세부 귀속, Task 365 동일 성공 상태 생략, Task 366 triangle
   batching, Task 367 전체 축 재귀속 순서를 설계합니다.
4. 각 단계의 cache invalidation, frame barrier, ABI/렌더링 동등성 조건과 금지 사항을
   명시합니다.
5. `docs/analysis/current-execution-frontier.md`의 최상위 결론, 최근 Task와 다음 순서를
   최신 기준으로 갱신합니다.
6. 작업 로그를 남기고 문서 정합성을 검사합니다.
7. 사용자 요청에 따라 `VERSION`을 변경하지 않고 작업 브랜치를 `main`에 squash
   병합한 뒤 작업 브랜치를 삭제합니다.

### 완료 조건

* 최신 수치와 과거 수치를 서로 다른 실행에서 임의로 혼합하지 않습니다.
* 다음 세션이 Task 364의 계측 항목과 완료 gate를 추가 질문 없이 확인할 수 있습니다.
* 구현 전에 증명해야 할 의미 보존 조건과 기각 조건이 명확합니다.
* `git diff --check`가 성공하고 변경 범위가 문서로 제한됩니다.
* `VERSION`은 `0.0.113`으로 유지되며 새 tag를 만들지 않습니다.

---

## English

### Objective and completion

Document the current Glide call-volume bottleneck and an immediately
resumable implementation order from the latest Release profile. Update the
current execution frontier without changing performance code.

Revalidate the whole-run, ordinal, backend, and swap figures; explain why the
older LFB-first priority does not apply to this no-LFB scene; define Tasks
364 through 367 for setter attribution, exact successful-state elision,
ordered triangle batching, and final re-attribution; and record every cache
invalidation, frame barrier, ABI, rendering-equivalence, and rejection gate.

Finish with documentation consistency checks, commit the task, squash it
into `main` without changing `VERSION`, create no tag, and delete the task
branch as explicitly requested.
