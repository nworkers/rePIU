# AOT-DBT 4단계 로드맵 문서화 작업 지시 / AOT-DBT four-stage roadmap documentation work order

## 한국어

### 목적

Task 276과 277에서 완료한 DBT 기반을 후속 작업과 연결하는 단일 순서 문서를
작성합니다. 이번 작업은 실행 의미나 코드를 변경하지 않습니다.

### 작업 범위

1. 다음 네 단계를 고정된 순서로 기록합니다.
   - HLE 후 기존 cache entry 즉시 복귀 — 완료
   - translated RET miss host-stack dispatch — 완료
   - RET fallback 원인 계측·분류 — 다음 작업
   - indirect call/jump miss host-stack dispatch — 후속 작업
2. 각 단계의 안전 계약, 계측, 완료 조건과 선행 관계를 명시합니다.
3. FPS와 DBT 처리량을 구분하는 공통 성능 판정 기준을 기록합니다.
4. `ARCHITECTURE.md`의 AOT-DBT 절에서 로드맵을 연결합니다.
5. 사용자 요청에 따라 기존 `.gitignore`의 `WIP/` 제외 규칙을 이번 작업에 포함합니다.

### 검증

- 한국어 다음에 영어 번역이 존재하는지 확인
- Mermaid 순서와 본문 상태가 일치하는지 확인
- 참조한 Task 276/277 수치가 기존 분석·작업 로그와 일치하는지 확인
- `git diff --check`
- 문서와 `.gitignore` 외에 의도하지 않은 변경이 staging되지 않았는지 확인

## English

### Purpose

Create one ordered roadmap connecting the completed Task 276/277 DBT foundation
to the next work. This task changes neither runtime code nor guest semantics.

### Scope

1. Fix the following four-stage order:
   - immediate existing-cache re-entry after HLE — complete;
   - host-stack dispatch for translated RET misses — complete;
   - RET fallback instrumentation and classification — next;
   - host-stack dispatch for indirect call/jump misses — follow-up.
2. Define each stage's safety contract, observability, completion criteria, and
   dependency.
3. Record common performance criteria that distinguish FPS from DBT throughput.
4. Link the roadmap from the AOT-DBT section of `ARCHITECTURE.md`.
5. Include the existing `.gitignore` rule for `WIP/` as requested.

### Verification

- Confirm Korean is followed by an English translation.
- Confirm the Mermaid order matches prose status.
- Cross-check Task 276/277 measurements against existing analysis/work logs.
- Run `git diff --check`.
- Confirm staging contains only the intended documentation and `.gitignore`
  change.
