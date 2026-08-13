# Return 최적화 이후 병목 귀속 작업 지시

설계: [20260814-482-post-return-bottleneck-attribution.md](../design/20260814-482-post-return-bottleneck-attribution.md)

1. opt-in return stage profile 구조와 환경 변수 정책을 추가합니다.
2. return adapter/resolver를 상호 배타적인 다섯 stage와 residual로 계측합니다.
3. return patch policy의 site별 bypass 수를 allocation 없는 기존 상태에 누적합니다.
4. execution snapshot과 종료 로그에 stage totals/counts/coverage와 상위 16개 site를
   연결합니다.
5. stage accounting, disabled 상태, clamp와 top-N 정렬 probe를 추가합니다.
6. Win32 x86 Debug/Release probe와 앱을 빌드하고 pumpit8 probe를 실행합니다.
7. Glide ordinal timing과 return stage timing을 각각 별도 attribution pass로 수집합니다.
8. 결과에 따라 ordinal HLE, return stage 또는 direct-return table 중 다음 구현을
   선택하고 analysis/frontier와 작업 로그를 갱신합니다.

## 완료 조건

계측 off 동작이 바뀌지 않고 모든 probe가 통과해야 합니다. attribution 실행은 return
성공률 100%, fallback 0과 index scan 0을 유지하며, Glide ordinal과 return stage가 각각
outer bucket을 해석할 수 있는 coverage와 안정된 상위 순위를 제공해야 합니다.

---

# Post-Return-Optimization Bottleneck Attribution Work Order

Design: [20260814-482-post-return-bottleneck-attribution.md](../design/20260814-482-post-return-bottleneck-attribution.md)

1. Add an opt-in return-stage profile and environment policy.
2. Instrument the return adapter/resolver with five mutually exclusive stages
   plus residual.
3. Accumulate per-site bypass counts in the existing allocation-free policy state.
4. Connect stage totals/counts/coverage and the top sixteen sites to the execution
   snapshot and final log.
5. Add probes for accounting, disabled behavior, clamping, and top-N ordering.
6. Build the Win32 x86 Debug/Release probe and application and run the pumpit8 probe.
7. Collect separate Glide-ordinal and return-stage attribution passes.
8. Use the result to select ordinal HLE, return-stage, or direct-return-table work,
   then update analysis/frontier and the work log.

## Completion criteria

Instrumentation-off behavior is unchanged and every probe passes. Attribution
runs retain 100% return success, zero fallback, and zero index scans, while Glide
ordinal and return stages provide enough outer-bucket coverage and stable ranking
to select the next implementation.
