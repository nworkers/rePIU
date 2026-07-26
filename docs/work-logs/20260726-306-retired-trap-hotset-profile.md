# 20260726-306 작업 로그: retired trap hotset 및 해결 결과 계측 / Work log: retired-trap hotset and resolution profiling

설계: [20260726-306-retired-trap-hotset-profile.md](../design/20260726-306-retired-trap-hotset-profile.md)

작업 지시: [20260726-306-retired-trap-hotset-profile.md](../work-orders/20260726-306-retired-trap-hotset-profile.md)

## 한국어

### 구현

`REPIU_AOT_RETIRED_TRAP_PROFILE=1|on|true`에서만 활성화되는 retired trap profiler를
추가했습니다. guest 주소와 cache 주소를 각각 exact histogram으로 집계하고 종류별
65,536개 상한과 overflow count를 둡니다. cache sample은 inactive address-map entry의
guest 주소, generation, guest/emitted 길이와 metadata 유효성을 보존합니다.

retired trap 뒤 기존 resolver는 선택적 결과 포인터로 active hit, generation publish,
quarantine, generation failure, fallback, trace sentinel 중 실제 분기를 보고합니다.
profiler가 꺼져 있으면 histogram lookup과 결과 기록을 수행하지 않습니다. 실행 종료 시
guest/cache 상위 16개, guest top-16 coverage, short/relinkable/metadata-miss count와 결과별
count를 최종 로그에 출력합니다.

합성 probe는 설정 parser, 짧은 entry와 5바이트 이상 entry, 동일 guest의 여러 generation,
반복 count, count 내림차순·주소 오름차순 정렬, 결과 분류와 65,536-entry overflow를
검증합니다.

### 검증

- Win32 x86 Debug 전체 빌드 성공: loader, AOT probe, supervisor 포함.
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 성공:
  `valid=true`, `linear_span_all=true`, `retired_trap_profile_policy=true`,
  `retired_trap_profile_behavior=true`, `retired_trap_profile_capacity=true`,
  `retired_trap_profile_all=true`, `coherence_all=true`.
- 60초 direct profile:
  `build/benchmarks/aot-retired-trap-profile/aot-dbt/20260726-172512`.
- 로더 내부 timeout까지 정상 진행: `timed_out=true`, AOT legacy fallback 0.
- EEPROM SHA-256는 fixture와 결과 모두
  `A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`.
- terminal guest fatal은 없었습니다. 기존 미구현 Glide 5종은 의도한
  `[repiu-fatal] GLIDE_UNIMPLEMENTED_FUNCTION action=continue`로 명확히 남고 실행은
  계속됐습니다.

### 측정 결과와 결론

| 지표 | 결과 |
|---|---:|
| retired trap | 7,401 |
| distinct guest/cache | 61 / 146 |
| guest top-16 coverage | 98.24% |
| relinkable / short / metadata miss | 108 / 7,293 / 0 |
| histogram overflow guest/cache | 0 / 0 |
| active/generation/quarantine/failure/fallback/trace | 0 / 107 / 7,293 / 1 / 0 / 0 |

상위 guest `0x030F4A94`는 2,850회(38.51%), `0x030F507C`는 1,891회(25.55%)로
두 주소만 전체의 64.06%를 차지했습니다. 상위 5개는 6,288회(84.96%)였습니다.
전체 trap의 98.54%가 5바이트 미만의 짧은 entry이므로 기존 `E9 rel32` in-place relink를
확대해도 대부분을 제거할 수 없습니다. 다음 성능 작업은 1~4바이트 inactive entry를
side table 또는 공용 dispatch gate로 최신 generation/guest fallback에 보내 `INT3`
예외 자체를 피하는 구조를 설계하는 것입니다.

## English

### Implementation and verification

Added an opt-in retired-trap profiler enabled only by
`REPIU_AOT_RETIRED_TRAP_PROFILE=1|on|true`. It keeps separate exact guest and cache-address
histograms, each capped at 65,536 distinct addresses with overflow accounting. Cache samples
retain inactive address-map guest address, generation, guest/emitted lengths, and metadata
validity. The existing resolver optionally reports active hit, generation publication,
quarantine, generation failure, fallback, or trace sentinel. Profiling disabled performs no
histogram lookup or outcome recording.

The final execution log reports guest/cache top 16 lists, guest top-16 coverage,
short/relinkable/metadata-miss counts, and resolver outcomes. Synthetic probes cover parser
policy, short and relinkable entries, multiple generations, repeated counts, deterministic
sorting, outcome classification, and the 65,536-entry overflow boundary.

The full Win32 x86 Debug build passed, including loader, AOT probe, and supervisor.
`repiu_aot_probe` reported `valid=true`, `linear_span_all=true`, all three retired-profile
probe groups and `retired_trap_profile_all=true`, and `coherence_all=true`. The direct
60-second profile is stored under
`build/benchmarks/aot-retired-trap-profile/aot-dbt/20260726-172512`. It reached the loader's
internal timeout with zero AOT legacy fallback. The result EEPROM SHA-256 matched the fixture:
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`. There was no terminal
guest fatal. Five known unimplemented Glide categories remained clearly labeled
`[repiu-fatal] ... action=continue` diagnostics and execution continued.

### Measured result and decision

The run recorded 7,401 retired traps across 61 guest and 146 cache addresses, with no
histogram overflow or metadata misses. The top 16 guest addresses covered 98.24%.
Only 108 traps were relinkable; 7,293 (98.54%) came from entries shorter than five bytes.
Resolver outcomes were `0/107/7293/1/0/0` for
active/generation/quarantine/failure/fallback/trace.

`0x030F4A94` contributed 2,850 events (38.51%) and `0x030F507C` contributed 1,891 (25.55%),
covering 64.06% together; the top five covered 84.96%. Extending the existing in-place
`E9 rel32` relink cannot address most traps. The next performance task should design a side
table or shared dispatch gate that routes one-to-four-byte inactive entries to the newest
generation or guest fallback without raising `INT3`.
