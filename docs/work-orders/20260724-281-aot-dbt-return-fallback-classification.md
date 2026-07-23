# AOT-DBT RET fallback 원인 분류 작업 지시 / AOT-DBT RET fallback classification work order

## 한국어

### 목표

Task 280 로드맵 3단계를 구현해 Task 277의 RET fallback을 배타적인 원인별로
계측하고, 실제 실행에서 다음 DBT 작업의 지배 원인을 확정합니다.

### 작업 범위

1. 공개 Win32 execution result에 고정 RET fallback reason 모델과 count를 추가합니다.
2. `ThreadContext`에 reason별 atomic counter를 추가합니다.
3. DBT return adapter가 C++ 진입 attempt를 먼저 기록하고 invalid site를 포함한 모든
   handler 실패를 total + reason 하나로 기록하게 합니다.
4. `HandleAotReturnTransfer`가 기존 동작을 보존하면서 선택적으로 실패 원인을 반환하게
   합니다.
5. 종료 snapshot과 host log에 reason vector를 추가합니다.
6. 모든 reason slot과 회계 불변식을 검증하는 synthetic probe를 추가합니다.
7. Win32 x86 Debug 빌드, 전체 AOT probe와 격리 EEPROM 실제 실행을 수행합니다.
8. 결과를 기존 analysis, architecture와 Task 281 작업 로그에 반영하고 Stage 4 설계를
   지배 원인에 맞게 구체화합니다.

### 제외 범위

- 이번 단계에서 fallback을 직접 성공 경로로 바꾸는 최적화
- indirect call/jump emitter 또는 thunk 구현
- 기존 `legacy`, `aot`, `aot-dynamic` image layout 변경
- guest-visible 상태 또는 게임 로직 변경

### 완료 조건

- `attempt = success + fallback`과 `fallback = reason sum`이 probe와 실제 실행에서
  성립합니다.
- 빌드와 기존 probe가 통과합니다.
- fatal, exception, legacy fallback과 EEPROM 변경이 없습니다.
- 지배 fallback 원인과 다음 구현의 의사결정 지점이 문서화됩니다.

## English

### Goal

Implement Task 280 Stage 3 by classifying Task 277 RET fallbacks into exclusive
causes and establish the dominant live cause that controls the next DBT work.

### Scope

1. Add the fixed RET fallback reason model and counts to the public Win32
   execution result.
2. Add atomic per-reason counters to `ThreadContext`.
3. Account C++ resolver entry before site validation and record every handler
   failure as total plus exactly one reason.
4. Let `HandleAotReturnTransfer` optionally report a reason without changing
   established behavior.
5. Copy and print the reason vector in final execution telemetry.
6. Add a synthetic probe for every slot and both accounting invariants.
7. Run the Win32 x86 Debug build, all AOT probes, and isolated-EEPROM live runs.
8. Update analysis, architecture, and the work log, then refine Stage 4 design
   from the dominant cause.

### Out of scope

- Converting a fallback into a new success path in this stage
- Implementing the indirect call/jump emitter or thunk
- Changing `legacy`, `aot`, or `aot-dynamic` image layouts
- Changing guest-visible state or game logic

### Completion criteria

- Both accounting invariants hold in probes and live execution.
- The build and existing probes pass.
- There is no fatal, exception, legacy fallback, or EEPROM change.
- The dominant cause and next implementation decision point are documented.
