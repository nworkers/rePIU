# 20260723-275 작업 지시: 네이티브 직선 span 실행

설계: [20260723-275-native-linear-span-execution.md](../design/20260723-275-native-linear-span-execution.md)

## 한국어

### 목표

함수 단위 native region이 포착하지 못하는 `aot-dynamic` single-step hot 구간에서,
민감 명령과 제어 전이 사이의 안전한 직선 명령을 게스트 코드 수정 없이 네이티브로
실행합니다.

### 구현 범위

1. `verified_region_analyzer`에 직선 span scanner와 결과 구조를 추가합니다.
2. `NativeFastPathState`에 독립적인 span 실행 상태와 계측을 추가합니다.
3. Route A single-step 경로에서 함수형 region 실패 후 span 진입을 시도합니다.
4. Dr0 boundary와 예상하지 않은 예외에서 debug register·TF를 fail-closed 복원합니다.
5. synthetic probe와 off/on benchmark harness를 추가합니다.
6. 통제 A/B 결과를 분석·아키텍처·작업 로그에 반영합니다.

### 비범위

- INT3 guest-code patch
- 함수형 region의 Dr1~Dr3 정책 변경
- span scanner 결과 cache 또는 SMC invalidation
- 원본 실행 파일이나 게임 로직 변경

### 완료 조건

- scanner가 민감 명령, 제어 전이, 명시적 memory write 경계를 결정론적으로 판별
- span 중 예상하지 않은 exception에서 기존 handler로 안전하게 복귀
- 전체 빌드와 기존 probe 무회귀
- 동일 binary A/B에서 fatal/fallback 없이 single-step 또는 milestone 개선 확인
- 개선이 없으면 기능을 기본 비활성 진단 경로로 유지하고 다음 병목을 기록

## English

### Goal

Run safe straight-line instructions natively between HLE-sensitive and control-transfer
boundaries in `aot-dynamic` hot paths that function-level regions do not cover, without
modifying guest code.

### Scope

1. Add a linear-span scanner and result type to `verified_region_analyzer`.
2. Add independent span execution state and counters to `NativeFastPathState`.
3. Try span entry after the existing function-region attempt fails.
4. Restore debug registers and TF fail-closed at Dr0 boundaries and unexpected exceptions.
5. Add a synthetic probe and an off/on benchmark harness.
6. Record controlled results in analysis, architecture, and a work log.

### Out of scope

- INT3 guest-code patching
- Changes to the existing Dr1-Dr3 function-region policy
- Scan-result caching or SMC invalidation
- Original executable or gameplay changes

### Completion criteria

- Deterministic sensitive, control-transfer, and explicit-memory-write boundaries
- Safe fallback to the existing handlers after unexpected span exceptions
- Full build and existing probes pass
- Same-binary A/B shows lower single-step cost or earlier milestones without fatal/fallback
- If no gain is measured, keep the path opt-in and record the next bottleneck
