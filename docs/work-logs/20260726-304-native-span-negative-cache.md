# 20260726-304 작업 로그: 네이티브 span 음성 캐시 / Work log: native-span negative cache

설계: [20260726-304-native-span-negative-cache.md](../design/20260726-304-native-span-negative-cache.md)

작업 지시: [20260726-304-native-span-negative-cache.md](../work-orders/20260726-304-native-span-negative-cache.md)

## 한국어

### 구현

기본 native linear-span scanner가 0개 또는 1개 일반 명령 뒤 정적 경계에서 거절할 때
최대 30바이트의 연속 분석 범위를 남기게 했습니다. `NativeFastPathState`의 entry EIP별
음성 캐시는 해당 바이트 snapshot을 저장하고 조회 시 현재 guest byte와 비교합니다.
일치하면 Zydis 재디코딩 없이 기존 single-step fallback을 선택하고, 불일치하면 stale
항목을 지운 뒤 재스캔합니다.

캐시는 65,536개 entry로 제한했으며 hit/miss/stale/store/capacity-skip을 live 및 최종
telemetry에 연결했습니다. register/page/target 상태에 의존하는 write/jump 실험 모드는
캐시를 우회합니다. supervisor와 direct-loader A/B 스크립트에는
`CompareRejectCache`와 새 계수의 CSV 기록을 추가했습니다.

### 검증

- Win32 x86 Debug 전체 빌드 성공.
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 성공.
- 신규 `linear_span_reject_cache_behavior=true`.
- 신규 `linear_span_reject_cache_capacity=true`.
- 기존 `linear_span_all=true`, `coherence_all=true`.
- PowerShell A/B 스크립트 2종 parser 오류 0.

15초 smoke ON은 거절 8,365건 중 8,351건을 hit로 처리했고 fatal/legacy fallback/span
cancel은 0, EEPROM hash는 fixture와 일치했습니다.

60초 세 쌍의 결과:

- `build/benchmarks/native-linear-span-reject-cache/aot-dbt/20260726-143543/results.csv`
- `build/benchmarks/native-linear-span-reject-cache/aot-dbt/20260726-143817/results.csv`

| 쌍 | progress 변화 | texture 변화 | draw 변화 | swap 변화 | 거절 hit율 |
|---|---:|---:|---:|---:|---:|
| 1 | +1.20% | -46ms | -46ms | -61ms | 99.69% |
| 2 | -1.08% | -1,078ms | -15ms | -15ms | 99.69% |
| 3 | +0.02% | -1,031ms | +15ms | +15ms | 99.68% |

progress 변화 중앙값은 `+0.02%`로 후반 처리량은 실질적으로 같았습니다. texture
milestone은 세 쌍 모두 빨랐고 중앙값 `-1,031ms`(약 4.9%)였으며 draw/swap은 같은
수준이었습니다. 모든 실행에서 fatal과 legacy fallback은 0이고 EEPROM hash가
일치했습니다. 60초 구간의 기존 late span cancel은 OFF/ON에서 각각
`771/782`, `788/780`, `795/794`로 비슷해 새 회귀 증거가 없습니다.

### 결정

초기 resource decode milestone의 반복 개선이 확인되어 음성 캐시를 `aot-dbt` 기본
ON으로 승격했습니다. 다른 backend는 기본 OFF입니다.
`REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE=0|off|false` 또는 알 수 없는 값은 기능을
비활성화합니다.

## English

### Implementation and verification

The default native linear-span scanner now reports up to 30 contiguous analyzed bytes when a
static boundary rejects after zero or one ordinary instruction. A per-entry negative cache
in `NativeFastPathState` snapshots those bytes. Matching bytes select the existing
single-step fallback without another Zydis decode; changed bytes erase the stale entry and
rescan.

The cache is capped at 65,536 entries and exposes hit/miss/stale/store/capacity-skip through
live and final telemetry. Register/page/target-dependent write and jump experiments bypass
it. Both supervisor and direct-loader A/B scripts gained `CompareRejectCache` and CSV fields.

The full Win32 x86 Debug build passed. `repiu_aot_probe` passed the new behavior and capacity
cases plus existing `linear_span_all` and `coherence_all`; both PowerShell scripts parsed
without errors. A 15-second smoke served 8,351 of 8,365 rejections from cache with zero
fatal, legacy fallback, or span cancellation and a matching EEPROM hash.

Across three 60-second pairs, rejection hit rate was 99.68-99.69%. Pairwise progress changed
by `+1.20% / -1.08% / +0.02%` (median `+0.02%`), while texture milestones improved by
`46 / 1,078 / 1,031ms` (median `1,031ms`, about 4.9%). Draw/swap timing was effectively
unchanged. Every run kept zero fatal/legacy fallback and matching EEPROM hashes. Existing
late span cancellations were comparable across OFF/ON.

The repeatable early resource-decode milestone improvement promotes the negative cache to
default-on for `aot-dbt`; other backends remain default-off. `0|off|false` and unknown values
disable it.
