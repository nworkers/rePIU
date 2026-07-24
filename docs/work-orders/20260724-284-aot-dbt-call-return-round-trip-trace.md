# AOT-DBT CALL/RET 왕복 결정적 관측 작업 지시서 / AOT-DBT CALL/RET deterministic round-trip trace work order

## 한국어

### 목표

Task 283에서 CALL-only로 이분된 indirect host-dispatch 크래시의 최초 발산 지점을
추측 수정 없이 관측합니다. dispatcher-visible CALL의
`(source,target,return_address,entry_esp)`와 이를 소비하는 RET의 actual target/ESP를
VEH/host origin과 함께 연계합니다.

### 작업 범위

1. `docs/design/20260724-284-aot-dbt-call-return-round-trip-trace.md`의 고정 크기,
   allocation-free 데이터 모델을 구현합니다.
2. Win32 전용 `aot_dbt_call_return_trace.{h,cpp}`에 기록·연계 정책을 분리합니다.
3. 공용 indirect/return handler에 기본 VEH origin과 명시적 host origin을 전달합니다.
4. 기존 call shadow frame에 sequence, entry ESP, origin을 추가합니다.
5. 최근 256 event ring, 누적 카운터와 first-divergence sticky entry를
   `ThreadContext`와 공개 실행 결과에 추가합니다.
6. `REPIU_AOT_DBT_CALL_TRACE=1`일 때만 계측합니다.
7. 종료 snapshot과 loader 로그에 요약·최초 발산·최근 event를 출력합니다.
8. synthetic probe를 추가하고 CMake 대상에 새 소스를 연결합니다.
9. 동일 binary·격리 EEPROM으로 240초 control(indirect off)과
   calls-only A/B를 실행합니다.
10. 분석, 아키텍처와 작업 로그를 갱신합니다.

### 비범위

- inline-cache hit CALL/RET에 새 trampoline 삽입
- guest code 또는 code-cache emitter layout 변경
- CALL host dispatch 크래시의 추측 수정
- shared live telemetry POD 버전 변경
- JUMP-only 기본 활성화 정책 변경

### 완료 조건

- 비활성 시 event가 0이고 기존 실행 의미가 유지됩니다.
- probe가 origin, CALL/RET sequence 연계, target/ESP 판정, 상관 RET의 sticky ESP
  first divergence, 연계 불가 RET 필터와 ring wrap을 검증합니다.
- Win32 x86 Debug 전체 빌드와 기존 probe가 통과합니다.
- 240초 A/B의 final snapshot이 회수되고, 최초 dispatcher-visible 발산 또는
  "관측 범위 내 발산 없음"이 명시적으로 결론 납니다.
- EEPROM hash와 기존 fatal/exception 결과가 계측 외 의미 변화가 없음을 뒷받침합니다.

## English

### Goal

Observe, without a speculative fix, the first divergence behind the indirect
host-dispatch crash that Task 283 isolated to CALL-only. Correlate every
dispatcher-visible CALL tuple `(source,target,return_address,entry_esp)` with the
actual target/ESP of the RET that consumes it, tagged by VEH or host origin.

### Scope

1. Implement the fixed, allocation-free model from the Task 284 design.
2. Isolate policy in Win32 `aot_dbt_call_return_trace.{h,cpp}`.
3. Pass default VEH and explicit host origins into the common indirect/return handlers.
4. Extend the diagnostic call shadow frame with sequence, entry ESP, and origin.
5. Add a 256-event ring for CALLs and target-correlated RETs, aggregate counters for all
   observed RETs, and sticky first divergence to live and public result state.
6. Enable recording only with `REPIU_AOT_DBT_CALL_TRACE=1`.
7. Copy and print summary, first divergence, and recent chronological events at exit.
8. Add synthetic probes and connect the new source to CMake.
9. Run a 240-second isolated-EEPROM A/B: indirect-off control and calls-only experiment.
10. Update analysis, architecture, and the work log.

### Out of scope

- Instrumenting inline-cache-hit CALL/RET with new generated trampolines
- Changing guest code or code-cache emitter layout
- A speculative crash fix
- Changing the shared-live-telemetry POD version
- Changing the default JUMP-only policy

### Completion criteria

Disabled tracing records zero events. Probes cover origins, CALL/RET sequence correlation,
target/ESP checks, correlated-RET sticky ESP divergence, unrelated-RET filtering, and ring
wrap. The Win32 x86 Debug build and all existing probes pass. The 240-second A/B yields final snapshots and an explicit conclusion:
either the first dispatcher-visible divergence or no divergence inside the observation
boundary. EEPROM hashes and existing fatal/exception outcomes show no semantic change beyond
instrumentation.
