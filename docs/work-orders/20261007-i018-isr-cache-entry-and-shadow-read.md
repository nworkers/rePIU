# 작업 지시: INT8 ISR의 캐시 진입과 shadow 무조건 읽기 슬롯 (issue #18, 방향 3 3단계)

설계: `docs/design/20261007-i018-isr-cache-entry-and-shadow-read.md`

## 작업 항목

1. 주입 (`src/engine/execution/execution_trampoline.cpp`
   `InjectPendingInterrupts`)
   * direct 모델에서 벡터 주소를 `FindAotCacheAddress`로 조회, 없으면
     `RequestAotDynamicTranslation` 한 번. 성공하면 캐시 주소로 `Eip`.
   * `REPIU_TIMER_HANDLER_CACHE_ENTRY` opt-out, 카운터 세 개
     (`ThreadContext`, telemetry attempt, 로더 요약 한 줄).
2. read 슬롯 (`src/runtime/aot_code_cache.cpp`
   `EmitGuardedSegmentReadSlot`, 검증기의 i386 read 블록, 헤더 주석)
   * `66 8B 05|reg<<3 <shadow>; E9 rel32`, 두 주소 필드 = +3,
     `fallback_offset` = 슬롯 시작.
3. probe (`selector_guard_probe.cpp`) read 레이아웃 단언 갱신.
4. 검증: probe 두 개, pumpitea 교대(기준선 = 2단계 빌드), 스모크 3종.
   분석 문서에 주입 경로 사실과 결과 기록.

## 완료 기준

* 주입 진입 카운터에서 cache(+translated) 진입이 native 진입을
  압도하고, `0xFE2F0`·`0xFE376`의 HLE 진입이 틱·memcpy 수에 비례하지
  않는다.
* 공백·exception census가 2단계 기준선보다 나쁘지 않고 스모크가 정상.

---

# Work order: INT8 ISR cache entry and the unconditional shadow read slot (issue #18, direction 3 phase 3)

Design: `docs/design/20261007-i018-isr-cache-entry-and-shadow-read.md`

Items: (1) in `InjectPendingInterrupts`, on the direct model, look the
vector up with `FindAotCacheAddress`, request one dynamic translation
when unmapped, and enter the cache address on success; opt-out
environment variable and three entry counters through `ThreadContext`,
the telemetry attempt and one loader summary line; (2) re-emit the i386
guarded read slot as `mov r16, [shadow]; jmp` with both address fields
at +3 and `fallback_offset` at the slot start, update the validator and
the header comment; (3) update the probe's read assertions; (4) verify
with both probes, interleaved pumpitea runs against the phase-2 build
and the three-title smoke, and record the injection-path facts in the
analysis topic.

Done when cache (plus translated) entries dominate native entries in the
new counters, HLE entries at `0xFE2F0` and `0xFE376` stop scaling with
ticks and memcpys, and the gap, exception census and smoke are no worse
than the phase-2 baseline.
