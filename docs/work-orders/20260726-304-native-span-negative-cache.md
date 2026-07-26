# 20260726-304 작업 지시: 네이티브 span 음성 캐시 / Work order: native-span negative cache

설계: [20260726-304-native-span-negative-cache.md](../design/20260726-304-native-span-negative-cache.md)

## 한국어

### 목표

기본 `aot-dbt` native linear-span 경로에서 반복되는 정적 거절의 Zydis decode 비용을
줄이되, 게스트 실행 의미와 기존 fail-closed single-step fallback은 유지합니다.

### 작업 범위

- `NativeLinearSpan`에 cacheable rejection byte 길이 추가.
- `NativeFastPathState`에 제한된 음성 캐시와 hit/miss/stale/store/capacity-skip 계수 추가.
- 기본 정적 scan에서만 lookup/store를 수행하는 opt-in 정책 추가.
- 실행 결과 구조, live snapshot, Win32 최종 로그에 계수 연결.
- `repiu_aot_probe`에 cache hit와 byte-change invalidation 검증 추가.
- A/B 검증 후 기본 승격 또는 보류 결정.

### 비대상

- 성공 span의 generation 캐시 정책 변경.
- memory-write/direct-jump 실험 기능의 기본값 변경.
- HLE, quarantine, page retirement 또는 AOT emitter/translator 정책 변경.

### 절차

- [x] scanner가 0~1개 일반 명령 뒤 정적 경계에서 거절한 연속 바이트 길이를 기록.
- [x] 최대 65,536개 entry의 음성 캐시와 snapshot 비교/무효화 구현.
- [x] `REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE` 정책 및 write/jump 모드 우회 구현.
- [x] telemetry와 최종 로그 연결.
- [x] synthetic probe 및 기존 probe 실행.
- [x] Win32 x86 Debug 전체 빌드.
- [x] 동일 fixture OFF/ON 교차 측정과 정확성 게이트 확인.
- [x] 반복 texture milestone 개선에 따라 `aot-dbt` 기본 ON으로 승격.
- [x] `ARCHITECTURE.md`, 관련 analysis, 작업 로그 갱신.
- [x] 변경 커밋.

### 완료 조건

ON 실행에서 실제 cache hit가 발생하고, fatal/legacy fallback/예상 밖 span cancel이
늘지 않으며 EEPROM과 Glide 의미 milestone이 보존되어야 합니다. 반복 측정에서 성능
이득이 확인되지 않으면 기능은 opt-in으로 남깁니다.

## English

### Goal

Reduce repeated Zydis decode cost for static native linear-span rejections on the default
`aot-dbt` path while preserving guest semantics and the existing fail-closed single-step
fallback.

### Scope

Add a cacheable-rejection byte length to `NativeLinearSpan`; add a bounded negative cache and
hit/miss/stale/store/capacity-skip counters to `NativeFastPathState`; use it only for the
default static scan mode behind an initial opt-in; expose the counters in execution results,
live snapshots, and final logging; add synthetic probes; run controlled A/B validation and
decide whether to promote the default.

Successful-span generation caching, write/jump experiment defaults, HLE/quarantine/page
retirement, and AOT translation are out of scope.

### Procedure and completion

Implement scanner metadata, the 65,536-entry snapshot cache, environment policy, telemetry,
and probes. Pass all existing probes and a full Win32 x86 Debug build, then compare identical
OFF/ON fixtures. Promotion requires real hits, preserved EEPROM and Glide milestones, zero
fatal/legacy fallback, no unexpected cancellation regression, and repeatable performance
improvement. Otherwise keep the feature opt-in. Update architecture, analysis, and the work
log, then commit the task.
