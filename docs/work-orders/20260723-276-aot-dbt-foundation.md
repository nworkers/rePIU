# 20260723-276 작업 지시: AOT-DBT 기반과 HLE 후 즉시 복귀

## 한국어

### 목표

기존 AOT 공용 인프라를 유지한 채 `aot-dbt` 실행 정책을 추가하고, 성공적으로 처리된
HLE boundary 뒤의 불필요한 single-step 한 명령을 제거합니다.

### 구현 범위

1. 플랫폼 공용 execution backend enum, parser, 이름과 capability helper를 추가합니다.
2. Win32 host의 중복 환경 변수 판정을 공용 정책으로 교체하고 `aot-dbt`를 허용합니다.
3. AOT 실행 API와 thread context에 bool 대신 backend 정책을 전달합니다.
4. `aot-dbt` 전용 HLE 후 기존 cache entry 즉시 re-entry helper를 추가합니다.
5. 즉시 복귀 시도/성공 계측과 실제 backend 이름 출력을 추가합니다.
6. parser synthetic probe, 전체 빌드, 기존 probe와 통제 실행을 검증합니다.
7. 결과를 아키텍처·분석·작업 로그에 반영하고 한 작업 단위로 커밋합니다.

### 비범위

- 일반 return/indirect transfer sentinel의 완전한 host jump 전환
- 자체 x86 interpreter 또는 새로운 IR/code generator
- guest 실행 파일 수정
- 기존 `aot-dynamic` 정책 변경

### 완료 기준

- 네 backend 문자열과 capability가 결정론적으로 해석됨
- `aot-dbt`에서만 성공한 HLE 뒤 즉시 cache로 복귀
- 실패·quarantine은 기존 TF single-step으로 fail-closed
- Win32 x86 Debug 빌드와 기존 probe 무회귀
- 통제 비교에서 fatal/새 legacy fallback 없이 DBT 계측과 single-step 변화를 확인

## English

### Goal

Add an `aot-dbt` execution policy on the shared AOT infrastructure and remove one
unnecessary single-stepped instruction after a successfully handled HLE boundary.

### Scope

1. Add a platform-neutral backend enum, parser, name, and capability helpers.
2. Replace duplicate Win32 host environment checks and accept `aot-dbt`.
3. Pass the backend policy through the AOT execution API and thread context.
4. Add DBT-only immediate re-entry to an existing cache entry after handled HLE.
5. Record attempts/successes and print the actual backend name.
6. Run parser probes, the full build, existing probes, and a controlled runtime comparison.
7. Update architecture, analysis, and the work log, then commit the task unit.

### Out of scope

Complete exception-free return/indirect dispatch, a new interpreter or IR/code
generator, guest executable modification, and changes to existing
`aot-dynamic` behavior.

### Completion criteria

All four policies parse deterministically; only `aot-dbt` performs immediate
post-HLE re-entry; failures remain fail-closed under TF; build and probes pass;
and controlled execution shows the new counters and single-step effect without
fatal state or new legacy fallback.
