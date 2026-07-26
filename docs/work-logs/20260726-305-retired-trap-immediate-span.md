# 20260726-305 작업 로그: retired trap 즉시 span 재진입 / Work log: immediate span re-entry after retired traps

설계: [20260726-305-retired-trap-immediate-span.md](../design/20260726-305-retired-trap-immediate-span.md)

작업 지시: [20260726-305-retired-trap-immediate-span.md](../work-orders/20260726-305-retired-trap-immediate-span.md)

## 한국어

### 구현

active/new generation으로 해결되지 않은 retired cache breakpoint에서 기존 보수적
native linear-span scanner를 즉시 호출하는 opt-in 경로를 추가했습니다.
`REPIU_AOT_RETIRED_SPAN_REENTRY=1|on|true`에서만 활성화되며 기본값은 OFF입니다.
live/final telemetry와 supervisor 출력에 `retired_span=attempt/success`를 추가했고,
정책·성공·거절 합성 probe와 A/B 스크립트 지원을 추가했습니다.

벤치마크는 마지막 supervisor elapsed가 요청 시간보다 2초 이상 짧으면 실행을 무효로
판정하도록 보강했습니다. 현재 기본 ON인 native-span 음성 캐시는 retired-span OFF/ON
양쪽에 동일하게 활성화했습니다.

### 회귀 발견과 수정

첫 구현은 span 성공 시 `aot_reentry_pending`과 single-step trace 상태까지 지웠습니다.
60초 ON 실행은 `retired_span=1/1`이 된 직후 guest `RET(C3)` 경계 처리를 건너뛰고 약
19.5초에 종료됐습니다. 기존 linear-span 계약과 동일하게 pending/trace 상태를 보존하고
Dr0 경계에서 기존 AOT/HLE chain을 재개하도록 수정했습니다. 합성 probe도 이 상태 보존을
검증합니다.

### 검증과 성능

- Win32 x86 Debug 전체 빌드 성공.
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 성공:
  `valid=true`, `linear_span_retired_reentry=true`, `linear_span_all=true`,
  `coherence_all=true`.
- 수정 후 30초 smoke: 요청 시간 완료, `4008/3826` attempt/success, fatal 0,
  EEPROM hash 일치.
- 최종 3쌍 결과:
  `build/benchmarks/aot-retired-span-reentry/aot-dbt/20260726-154234/results.csv`.

| 쌍 | progress 변화 | single-step 변화 | attempt/success | 성공률 | texture 변화 |
|---|---:|---:|---:|---:|---:|
| 1 | +0.35% | -2.94% | 4006/3824 | 95.46% | -1,046ms |
| 2 | -0.03% | -2.86% | 4002/3820 | 95.45% | +31ms |
| 3 | +0.45% | -2.65% | 4029/3839 | 95.28% | -17ms |

중앙값은 progress `+0.35%`, single-step `-2.86%`, texture `-17ms`입니다. 모든 실행은
요청 시간을 채웠고 child exit 124, fatal 0, EEPROM hash 일치였습니다. 실제 성공 기회는
많지만 순 처리량 개선은 기본 승격 기준에 미달하므로 opt-in을 유지합니다.

## English

### Implementation, regression, and verification

Added an opt-in immediate call to the existing conservative native linear-span scanner when a
retired cache breakpoint cannot resolve to an active or new generation. The feature is enabled
only by `REPIU_AOT_RETIRED_SPAN_REENTRY=1|on|true` and defaults off. Live/final telemetry,
supervisor output, synthetic policy/success/rejection probes, and A/B script support report
`retired_span=attempt/success`.

The first implementation cleared pending-reentry and single-step trace state on success. Its
60-second ON candidate ended around 19.5 seconds immediately after `retired_span=1/1`, skipping
the guest `RET(C3)` boundary handler. Preserving the state until the Dr0 boundary restored the
existing AOT/HLE chain, and the probe now checks that contract. Benchmark validation also
rejects runs whose final supervisor elapsed is more than two seconds short of the request.

The full Win32 x86 Debug build passed. `repiu_aot_probe` reported `valid=true`,
`linear_span_retired_reentry=true`, `linear_span_all=true`, and `coherence_all=true`. A fixed
30-second smoke completed with `4008/3826` attempts/successes, zero fatal events, and a matching
EEPROM hash.

Three final 30-second alternating pairs are stored at
`build/benchmarks/aot-retired-span-reentry/aot-dbt/20260726-154234/results.csv`. Success rate was
95.28-95.46%; pairwise single-step changes were `-2.94% / -2.86% / -2.65%`, while progress
changed only `+0.35% / -0.03% / +0.45%`. Median texture change was `-17ms`. Every run reached
the requested duration with child exit 124, zero fatal events, and matching EEPROM hashes.
The net throughput gain is too small for default promotion, so the path remains opt-in.
