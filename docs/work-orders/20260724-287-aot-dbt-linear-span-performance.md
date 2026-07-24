# AOT-DBT fallback native linear-span 성능 판정 작업 지시서 / AOT-DBT fallback native linear-span performance work order

## 한국어

### 목표

기존 opt-in native linear span을 최신 기본 `aot-dbt` fallback에 적용해 3쌍 교차
240초 A/B로 정확성, single-step 절감과 의미 기반 게임 진행 개선을 판정합니다.

### 작업 범위

1. 기존 benchmark script에 `aot-dynamic|aot-dbt` backend 선택을 추가합니다.
2. `aot-dbt` 측정에서 indirect Stage 4, native region과 CALL 진단을 제거합니다.
3. backend, 격리 EEPROM hash와 fixture 일치 여부를 CSV에 추가합니다.
4. 짧은 smoke로 하네스와 metric parsing을 검증합니다.
5. 최신 Win32 x86 Debug build와 관련 probe를 통과시킵니다.
6. `OFF→ON`, `ON→OFF`, `OFF→ON` 순서로 240초씩 실행합니다.
7. 중앙값, pair 방향, milestone 검열과 정확성 불변식을 분석합니다.
8. 6회 모두 늦은 milestone 이전에 검열되면 직접 loader graceful timeout으로 같은
   3쌍을 실행해 texture/draw/swap 호출 횟수를 비교합니다.
9. 승격 조건을 충족하면 `aot-dbt`에만 unset 기본 ON을 적용하고 명시적 ON/OFF
   환경 정책과 probe를 추가합니다.
10. benchmark OFF는 명시적 `0`으로 바꿉니다.
11. 설계·분석·아키텍처·작업 로그에 결과와 활성화 결정을 반영합니다.

### 비범위

- quarantine/SMC 정책 완화
- indirect Stage 4 활성화 또는 최적화
- native span scanner/executor 의미 변경
- 새 hot-path 계측
- 일반 HLE/fallthrough miss dispatcher 구현

### 완료 조건

- 하네스가 두 backend를 지원하고 기존 `aot-dynamic` 기본값과 호환됩니다.
- 모든 ON 실행이 `entry == boundary`, cancel 0을 만족합니다.
- fatal/legacy fallback은 0이고 EEPROM hash가 fixture와 같습니다.
- 6회 결과와 원시 로그가 보존됩니다.
- 검열 시 직접 loader 6회 결과와 원시 로그도 보존됩니다.
- 반복 결과로 opt-in 유지 또는 기본 승격 후보 여부를 명확히 결정합니다.
- 승격 시 `aot-dbt` 기본 ON과 명시적 OFF가 probe로 검증됩니다.

## English

### Goal and scope

Apply the existing opt-in native linear span to current baseline `aot-dbt` fallback and run
three alternating 240-second A/B pairs to judge correctness, single-step reduction, and
semantic game-progress improvement.

Generalize the existing benchmark script for `aot-dynamic|aot-dbt`, explicitly remove
indirect Stage 4, native-region, and CALL-diagnostic variables in DBT runs, add backend and
isolated EEPROM hash/match columns, smoke-test metric parsing, verify the current Win32 x86
Debug build and probes, then run `OFF→ON`, `ON→OFF`, `OFF→ON`. Analyze medians, each pair's
direction, milestone censoring, and correctness invariants, and update the design,
architecture, analysis, and work log with the enablement decision.

If all six supervisor runs censor late milestones, repeat the same three alternating pairs
with the loader's graceful timeout and compare final texture, draw, and swap call counts.
If promotion criteria pass, enable spans by default only for `aot-dbt`, add explicit
on/off environment policy and probes, and make benchmark OFF use explicit `0`.

Quarantine/SMC relaxation, Stage 4 changes, native-span semantic changes, new hot-path
telemetry, and a general HLE/fallthrough miss dispatcher are out of scope. Completion
requires backward-compatible backend selection, `entry == boundary` and zero cancellation
for every ON run, zero fatal and legacy fallback, matching EEPROM hashes, preserved raw
results from both runners when needed, and a clear opt-in or promotion-candidate conclusion.
Promotion additionally requires probe coverage for the `aot-dbt` default and explicit
disable escape hatch.
