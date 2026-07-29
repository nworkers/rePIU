# 20260729-355 현재 성능 다음 작업 기록 작업 지시 / Work order

* 설계: [20260729-355-current-performance-next-actions.md](../design/20260729-355-current-performance-next-actions.md)

## 한국어

### 목표

Task 354 최신 60초 실행을 기준으로 다음 성능 조사 순서와 의미 보존 gate를 기록하고,
현재 작업 브랜치를 `main`에 병합할 수 있는 상태로 마감합니다.

### 작업

1. 최신 control 전체 실행 축과 profile Glide ordinal을 같은 실행끼리 재합산합니다.
2. Task 356 `grLfbLock` phase/guest overwrite coverage를 첫 구현 대상으로 지정합니다.
3. Task 357 frame-local Glide handoff census를 두 번째 대상으로 지정합니다.
4. Task 358 unbiased guest residency 귀속을 세 번째 대상으로 지정합니다.
5. TF/VEH의 재진입 조건과 모든 A/B semantic invariant를 명시합니다.
6. current frontier와 작업 로그를 갱신하고 patch version을 올립니다.

### 완료 조건

* 다음 세 작업의 질문, 측정 범위, 금지되는 의미 변경이 문서에 명확합니다.
* 최신 비중은 서로 다른 실행의 수치를 혼합하지 않습니다.
* 문서 변경을 커밋하고 전체 작업 브랜치를 `main`에 squash merge합니다.
* 증가된 `VERSION`의 annotated tag를 로컬에 만들고 작업 브랜치를 삭제합니다.

---

## English

### Objective and completion

Record the ordered performance investigations and semantic gates from Task
354's latest matched runs, then close the current branch for merge.

The document must define Task 356 `grLfbLock` phase and overwrite coverage,
Task 357 frame-local Glide handoff census, and Task 358 unbiased guest
residency attribution without mixing measurements from different runs. Commit
the documentation, increment the patch version, squash the complete task
branch into `main`, create the matching local annotated tag, and delete the
task branch.
