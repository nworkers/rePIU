# Task 274 작업 로그: AOT 간접 inline cache 통제 A/B

## 한국어

### 작업 결과

같은 Win32 x86 Debug 바이너리에서 간접 call/jump cache의 1슬롯과 4슬롯을 선택할 수
있도록 runtime build policy를 추가했습니다. host가 선택한 entry 수는 정적 image와 동적
append에 함께 전달되며, 기본값은 4입니다. 허용되지 않은 값은 실행 전에 오류로 종료합니다.

반복 측정의 상태 오염을 막기 위해 `REPIU_EEPROM_PATH`를 추가했습니다. benchmark harness는
각 실행마다 같은 원본 EEPROM의 독립 사본을 만들며, 1→4→4→1 교차 순서로 실행합니다.
Glide 경계의 반복 로그 대신 shared live telemetry에 window gate/open, texture, draw, swap
최초 도달 시점을 한 번만 기록하도록 했습니다.

### 검증

- Win32 x86 Debug 전체 빌드 성공
- `repiu_aot_probe`의 기존 4-entry, replacement, retirement, coherence 검증 통과
- 새 1-entry emitter layout 검증 통과 (`inline_cache_one_entry_layout=true`)
- `REPIU_AOT_INDIRECT_CACHE_SLOTS=2`가 오류와 함께 종료되는 fail-closed 검증 통과
- 5초 harness smoke A/B 성공
- 240초 × 4회 통제 A/B 성공
- 모든 실행에서 fatal 0, legacy fallback 0
- 네 실행의 최종 EEPROM SHA-256 동일

원시 결과: `build/benchmarks/aot-inline-cache/20260723-110911/results.csv`

| 중앙값 지표 | 1슬롯 | 4슬롯 | 변화 |
|---|---:|---:|---:|
| `indir` boundary | 33,923 | 33,745 | -0.5% |
| 전체 boundary | 103,486.5 | 102,326.5 | -1.1% |
| window open | 11.805초 | 10.258초 | -13.1% |
| 첫 texture upload | 216.055초 | 223.406초 | 3.4% 느림 |
| 첫 swap | 230.328초 | 237.922초(1표본) | 개선 확인 안 됨 |

4슬롯 실행 하나는 240초 안에 swap에 도달하지 않아 해당 milestone은 censored입니다.
window open 차이는 cache 부하가 누적되기 전의 시작 편차로 보며 성능 향상으로 귀속하지
않습니다. 이후 texture와 swap에서 개선이 없고 `indir`도 0.5%만 줄었으므로, Task 273의
교차 빌드 28.2% 감소는 재현되지 않았습니다. 4슬롯은 안전하게 유지하지만 다음 성능
작업은 슬롯 확장이 아니라 native coverage와 반복 single-step/경계 원인에 집중합니다.

### 구현 중 보정

초기 smoke harness의 pipeline 기반 stdout/stderr 수집은 종료 대기를 불안정하게 만들었습니다.
프로세스별 파일 redirect와 shared telemetry polling으로 교체한 뒤 smoke 및 장기 측정을
완료했습니다. 이 변경은 게임 실행 의미에는 영향을 주지 않습니다.

## English

### Result

Added a runtime build policy that selects one or four indirect call/jump cache entries in the
same Win32 x86 Debug binary. The selected count propagates through the static image and every
dynamic append. Four is the default, and unsupported values fail before guest execution.

Added `REPIU_EEPROM_PATH` so every benchmark run can receive an independent copy of identical
persistent state. The harness alternates 1→4→4→1 and records one-shot window gate/open,
texture, draw, and swap milestones through shared live telemetry instead of hot-path logging.

### Verification

- Full Win32 x86 Debug build passed.
- Existing four-entry, replacement, retirement, and coherence probes passed.
- The new one-entry layout probe passed (`inline_cache_one_entry_layout=true`).
- Invalid slot value `2` failed closed.
- The 5-second smoke A/B and the 240-second × 4 controlled A/B passed.
- Every run reported zero fatal and legacy-fallback counts.
- Final EEPROM SHA-256 hashes matched across all four runs.

Raw results: `build/benchmarks/aot-inline-cache/20260723-110911/results.csv`

Median indirect boundaries fell only 0.5%, and total boundaries fell 1.1%. Texture upload
was 3.4% slower. One four-slot run did not reach swap before the 240-second censoring point,
so swap did not improve. The early window-open difference is treated as startup variance,
not a cache gain. The controlled result does not reproduce Task 273's 28.2% cross-build
observation. Keep the safe four-slot mechanism, but direct the next performance task toward
native coverage and recurring single-step/boundary exits rather than additional slots.

### Harness correction

The first smoke version used pipeline-based process output collection, which made shutdown
waiting unreliable. It was replaced with per-process redirected files and shared-telemetry
polling before the smoke and long runs were completed. This does not change guest semantics.
