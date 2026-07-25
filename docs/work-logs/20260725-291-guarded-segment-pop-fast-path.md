# 20260725-291 작업 로그: guarded segment-pop fast path / Work log: guarded segment-pop fast path

설계: [20260725-291-guarded-segment-pop-fast-path.md](../design/20260725-291-guarded-segment-pop-fast-path.md)

작업 지시: [20260725-291-guarded-segment-pop-fast-path.md](../work-orders/20260725-291-guarded-segment-pop-fast-path.md)

## 한국어

### 구현

planner가 exact plain opcode `07`, `1F`, `0F A1`, `0F A9`를 각각 ES/DS/FS/GS의
`kGuardedSegmentPop`으로 분류하고 fallthrough edge를 보존하도록 했습니다. `POP SS`와
prefixed form은 기존 HLE boundary로 유지합니다.

플랫폼 공용 emitter는 `pushfd; push eax` 뒤 physical selector를 guest 원래 `[ESP]` 및
shadow selector와 비교합니다. 성공하면 counter를 증가시키고 EAX/EFLAGS를 복원한 뒤
`lea esp,[esp+4]`와 cache fallthrough를 실행합니다. 불일치하면 fallback counter 뒤
EAX/EFLAGS를 복원하여 ESP를 포함한 guest 상태가 진입 시점과 동일한 상태로 `INT3`에
도달합니다.

Win32 placement와 dynamic append는 shadow selector 및 success/fallback counter 주소를
patch합니다. 최초 selector table이 없거나 해석 불가하면 slot 시작을 `INT3`로 두고,
live selector 재해석 때 안전한 slot만 복원합니다. breakpoint provenance, live snapshot,
종료 telemetry, whole-CFG HLE coverage 검증과 합성 probe를 함께 확장했습니다.

### 검증

- Win32 x86 Debug 전체 빌드: 성공
- 증분 `repiu_aot_probe`, loader, supervisor 빌드: 성공
- `repiu_aot_probe`: `guarded_segment_pop_*` 8개 항목과
  `selector_guard_all=true`, `linear_span_all=true`, `coherence_all=true`.
  손상된 guard 분기와 누락 fallback도 whole-CFG 검증에서 거부
- 20초 OFF/ON: progress `8,399/8,671`, single-step `59,418/45,175`, AOT boundary
  `59,529/44,142`
- 순서를 뒤집은 55초 OFF/ON: progress `37,606/39,571`, triangle draw `412/468`,
  single-step `252,701/246,644`, AOT boundary `74,724/59,334`
- 55초 ON guarded success/fallback: `21,011/1,593`(92.95%)
- 모든 live 실행: loader exit 0, graceful timeout, fatal 0, AOT legacy fallback 0
- 기본 정책 smoke: 환경 변수 미지정은 ON(`8,714/1,593` success/fallback), 알 수 없는
  값은 fail-closed OFF
- fixture/OFF/ON EEPROM SHA-256:
  `A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`
- 원시 로그: `build/benchmarks/guarded-segment-pop/` 아래 smoke/long/default/invalid 결과

### 판정

두 시간 범위와 반대 실행 순서에서 progress가 개선됐고 장기 Glide draw도 13.59%
증가했습니다. AOT boundary는 20.60% 줄었으며 정확성 gate도 유지됐습니다. 따라서
`aot-dbt`에서 기본 ON으로 승격하고 `REPIU_AOT_GUARDED_SEGMENT_POP=0|off|false`와
알 수 없는 값을 진단/회귀 bisect용 fail-closed opt-out으로 남겼습니다. 다른 backend는
계속 비활성입니다.

## English

### Implementation and verification

The planner classifies exact plain opcodes `07`, `1F`, `0F A1`, and `0F A9` as ES/DS/FS/GS
`kGuardedSegmentPop` records with explicit fallthroughs. `POP SS` and prefixed forms remain
HLE boundaries. The platform-neutral emitter compares the physical selector against both the
original guest-stack word and the shadow selector while preserving EAX/EFLAGS. Success
advances guest ESP by four and jumps to cache fallthrough; mismatch restores exact entry
state before INT3. Static placement and dynamic append patch shadow/counter addresses and
keep unresolved entries fail-closed. Provenance, telemetry, whole-CFG validation, and
synthetic probes were extended together.

The full Win32 x86 Debug build passed. The AOT probe reports every new guarded-pop check,
rejects a corrupted guard branch and a missing fallback, and reports
`selector_guard_all`, `linear_span_all`, and `coherence_all` as true. A 20-second comparison
improved progress from 8,399 to 8,671 and reduced single-step from 59,418 to 45,175. The
reversed-order 55-second OFF/ON comparison improved progress from 37,606 to 39,571 and
triangle draws from 412 to 468, while reducing AOT boundaries from 74,724 to 59,334. ON
recorded 21,011/1,593 guarded success/fallback (92.95%). Every live run exited normally via
graceful timeout with zero fatal and AOT legacy fallback counts, and all EEPROM hashes matched
the fixture. An unset-variable smoke enabled the path and recorded 8,714/1,593
success/fallback; an unknown-value smoke disabled it fail-closed.

The path is promoted to default-on for `aot-dbt`. Setting
`REPIU_AOT_GUARDED_SEGMENT_POP=0|off|false` and unknown values opt out fail-closed for
diagnostics or regression bisects; other backends remain disabled.