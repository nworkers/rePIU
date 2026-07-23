# AOT-DBT 4단계 로드맵 문서화 작업 로그 / AOT-DBT four-stage roadmap documentation log

## 한국어

### 결과

- Task 276/277의 완료 근거와 다음 두 작업을 하나의 4단계 로드맵으로 통합했습니다.
- 3단계에 RET fallback의 배타적 원인 분류와 회계 불변식을 정의했습니다.
- 4단계에 indirect call/jump miss host-stack dispatch의 ABI, fail-closed 정책과
  완료 조건을 정의했습니다.
- FPS를 DBT 처리량의 단독 지표로 사용하지 않는 공통 A/B 기준을 기록했습니다.
- `ARCHITECTURE.md`에서 로드맵과 단계 상태를 연결했습니다.
- 사용자 요청에 따라 `.gitignore`의 `WIP/` 규칙을 포함했습니다.

### 검증

- 기존 Task 276/277 분석과 작업 로그의 계측 수치를 대조했습니다.
- 문서의 한국어/영어 구조와 Mermaid 단계 순서를 확인했습니다.
- runtime source는 변경하지 않았으므로 빌드는 생략했습니다.
- `git diff --check`와 staging 범위 점검을 수행했습니다.

## English

### Result

- Consolidated the completed Task 276/277 evidence and the next two tasks into
  one four-stage roadmap.
- Defined exclusive RET fallback cause accounting and its invariant for Stage 3.
- Defined the ABI, fail-closed policy, and completion criteria for indirect
  call/jump miss host dispatch in Stage 4.
- Recorded controlled A/B criteria that do not treat FPS as standalone DBT
  throughput.
- Linked the roadmap and stage status from `ARCHITECTURE.md`.
- Included the existing `.gitignore` rule for `WIP/` as requested.

### Verification

- Cross-checked measurements against the Task 276/277 analysis and work logs.
- Reviewed the Korean/English structure and Mermaid stage order.
- Skipped a build because no runtime source changed.
- Ran `git diff --check` and reviewed the staging scope.
