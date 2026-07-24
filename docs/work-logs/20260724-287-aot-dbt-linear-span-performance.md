# 20260724-287 작업 로그: AOT-DBT fallback native linear span 기본 승격

## 한국어

### 하네스와 통제 조건

- `scripts/benchmark_native_linear_span.ps1`에 `aot-dynamic|aot-dbt` backend 선택,
  backend별 결과 디렉터리와 EEPROM hash 검증을 추가했습니다.
- `scripts/task287_direct_linear_span_ab.ps1`를 추가해 loader graceful timeout의
  progress, single-step, AOT/DBT 회계와 Glide ordinal 호출 수를 CSV로 회수했습니다.
- 모든 실행은 같은 Win32 x86 Debug binary, 격리 EEPROM, 4-slot cache를 사용했습니다.
- indirect Stage 4, native region, CALL trace/step probe는 제거했습니다.
- 순서는 `OFF→ON`, `ON→OFF`, `OFF→ON`, 각 240초였습니다.

### Supervisor 3쌍

원시 결과:
`build/benchmarks/native-linear-span/aot-dbt/20260724-194851/results.csv`

| 중앙값 | OFF | ON | 변화 |
|---|---:|---:|---:|
| progress | 49,588 | 53,732 | +8.36% |
| single-step | 1,328,320 | 919,110 | -30.81% |
| guest instruction proxy | 1,328,320 | 1,842,982 | +38.75% |

pair progress 변화는 `+8.36%`, `+1.00%`, `+24.57%`였습니다. ON span:

- `212051/212051/0`
- `208289/208289/0`
- `220430/220430/0`

모두 entry/boundary가 같고 cancel은 0입니다. fatal/legacy fallback과 EEPROM mismatch도
0이었습니다. 다만 6회 모두 texture/draw/swap 전에 검열됐습니다.

### 직접 loader 3쌍

원시 결과:
`build/benchmarks/native-linear-span/aot-dbt-direct/20260724-201830/results.csv`

| 중앙값 | OFF | ON | 변화 |
|---|---:|---:|---:|
| progress | 95,455 | 106,772 | +11.86% |
| single-step | 2,315,826 | 1,344,717 | -41.93% |
| texture download | 2 | 4 | +100% |
| draw | 0 | 42 | 실제 draw 진입 |
| buffer swap | 0 | 11 | 실제 swap 진입 |

pair별 progress:

- Pair 1: `83,175 → 106,325` (+27.83%)
- Pair 2: `96,046 → 106,772` (+11.17%)
- Pair 3: `95,455 → 107,781` (+12.91%)

ON의 texture/draw/swap은 `4/42/11`, `4/42/11`, `4/40/10`으로 반복됐습니다. 모든
실행은 loader exit 0, caught exception false, graceful timeout true,
fatal/legacy fallback 0이고 EEPROM SHA-256은
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`으로 같았습니다.

### 기본 승격 구현

- `NativeLinearSpanEnabled`가 execution backend를 받아 backend 기본값과 명시적
  환경 설정을 결합합니다.
- unset/empty는 `aot-dbt`에서만 ON입니다.
- `1|on|true`는 명시적 ON, `0|off|false`는 명시적 OFF입니다.
- 알 수 없는 값과 너무 긴 값은 fail-closed OFF입니다.
- benchmark OFF는 unset이 아니라 `0`을 사용합니다.
- policy 합성 probe를 `linear_span_all`에 포함했습니다.

실제 10초 policy smoke:

- unset `aot-dbt`: span `334/334/0/1603/827`
- explicit `0`: span `0/0/0/0/0`

### 최종 검증

- Visual Studio Win32 x86 Debug 전체 빌드 성공
- `linear_span_policy=true`, `linear_span_all=true`
- DBT indirect/return, CALL trace/step과 coherence probe 전체 통과
- 기존 C4819 경고 외 새 빌드 오류 없음

### 결론

native linear span은 `aot-dbt` fallback에서 반복 가능한 처리량과 의미 기반 렌더
진행 개선을 보였고 모든 정확성 불변식을 만족했습니다. 따라서 `aot-dbt`에만 기본
활성화합니다. 전체 프로그램 기본 backend와 다른 backend의 기본 span 정책은
바꾸지 않습니다.

## English

Task 287 generalized the existing benchmark for backend selection and EEPROM verification,
then added a graceful direct-loader harness that records progress, single-step, AOT/DBT
accounting, and per-ordinal Glide calls. Every run used one Win32 x86 Debug binary, an
isolated EEPROM copy, four cache slots, Stage 4 off, native region off, and CALL diagnostics
off in three alternating 240-second pairs.

Supervisor medians improved by +8.36% progress, -30.81% single-step, and +38.75% local
guest-instruction proxy. Every pair favored ON, all three enabled runs had exact
entry/boundary equality and zero cancellation, and no correctness invariant failed. The
supervisor nevertheless censored all late milestones.

The direct-loader medians improved from 95,455 to 106,772 progress (+11.86%) and from
2,315,826 to 1,344,717 single steps (-41.93%). Texture/draw/swap advanced from median
`2/0/0` to `4/42/11`; enabled runs repeated `4/42/11`, `4/42/11`, and `4/40/10`.
Pairwise progress gains were 27.83%, 11.17%, and 12.91%. All runs exited zero after a
graceful timeout with no caught exception, fatal state, legacy fallback, or EEPROM change.

The selection policy now defaults spans ON only for `aot-dbt` when unset. Explicit
`1|on|true` enables and `0|off|false` disables the path for any backend; unknown or
oversized values fail closed to disabled. A real policy smoke recorded default
`334/334/0` entry/boundary/cancel and explicit-off `0/0/0`. The full Win32 build and all
AOT, DBT, CALL diagnostic, linear-span policy, and coherence probes passed. The program-wide
default backend and other backend span defaults do not change.
