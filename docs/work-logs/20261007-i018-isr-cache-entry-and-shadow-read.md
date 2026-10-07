# 작업 로그: INT8 ISR의 캐시 진입과 shadow 무조건 읽기 슬롯 (issue #18, 방향 3 3단계)

작업 지시: `docs/work-orders/20261007-i018-isr-cache-entry-and-shadow-read.md`
설계: `docs/design/20261007-i018-isr-cache-entry-and-shadow-read.md`
분석: `docs/analysis/pumpitea-loading-segment-flip.md`

## 한 일

1. **남은 트랩의 경로 확정.** 2단계 빌드의 30초 세그먼트 트레이스와 90초
   breakpoint census로 memcpy `mov ax, ds`(read 슬롯 폴백 33k/30초)와 ISR
   진입 헬퍼 `0xFE2F0`(캐시 밖 네이티브 폴트 6.4k/30초)을 확정했다. 후자는
   `InjectPendingInterrupts`가 벡터의 원 게스트 주소로 ISR에 들어가기
   때문이었다(INT3 census에 지점이 없던 이유).
2. **ISR 주입의 캐시 진입**(`execution_trampoline.cpp`): direct 모델에서
   벡터 주소를 `FindAotCacheAddress`로 조회하고, 없으면
   `RequestAotDynamicTranslation`을 한 번 호출해 그 엔트리로 들어간다.
   프레임·return pad·플래그는 그대로. `REPIU_TIMER_HANDLER_CACHE_ENTRY`
   opt-out, 카운터 `INT8 handler entry cache/translated/native` 요약 추가.
3. **i386 read 슬롯**(`EmitGuardedSegmentReadSlot`)을 가드 없는 12바이트
   `mov r16, [shadow]; jmp`로 교체. 두 주소 필드 = +3, `fallback_offset` =
   슬롯 시작. 검증기·헤더 주석·probe 단언 갱신. long-mode는 그대로.

## 측정 (2026-10-07 01:45~01:58, 교대, 기준선 = 2단계 빌드 사본 `repiu_p2.exe`)

| 순서 | 빌드 | 로고 뒤 공백 | handled segment load | 예외 census (single-step/INT3/AV/privileged/합) | INT8 진입 cache/translated/native |
|---|---|---|---|---|---|
| 워밍업 | 3단계 | 1.65초 | 5,056 | 31.8k/434.8k/12.6k/41.5k/**520.6k** | 20,729/1/0 |
| 1 | 2단계 | 2.13초 | 25,861 | 18.6k/406.8k/46.5k/234.9k/**706.8k** | (없음) |
| 2 | 3단계 | **1.70초** | 5,056 | 31.6k/461.1k/12.5k/41.5k/**546.8k** | 20,729/1/0 |
| 3 | 2단계 | 2.07초 | 25,898 | 18.3k/623.4k/46.7k/236.7k/**925.2k** | (없음) |
| 4 | 3단계 | **1.69초** | 5,056 | 31.8k/436.8k/12.5k/41.5k/**522.7k** | 20,761/1/0 |

* ISR은 첫 틱에 한 번 번역된 뒤 모든 틱이 캐시로 진입했다(native 0).
  handled segment load 25.9k → **5,056**(런 간 동일), privileged 예외
  235k → 41.5k, AV 46.5k → 12.5k. INT3는 약간 늘었다(ISR의 경계가 캐시
  INT3로 옮겨 옴).
* 로고 뒤 공백 2.07~2.13초 → **1.65~1.70초**. 2단계 런3에서 41.5초의
  두 번째 정지(4.42초)가 다시 보였고 3단계 런4에서도 1.57초로 보였다 —
  양쪽 공통인 기존 현상(미확정 유지).
* `--selector-guard`(read 레이아웃·patch 단언 갱신) 전 항목 통과,
  `--segment-restore` 통과.
* 스모크 30초: pumpit1/pumpit2a/pumpit3a 정상(프레임 1,335~1,398, 게스트
  예외 0, return pad pushed=returned·unmatched 0, INT8 진입 전부 cache).

## 남은 것

* far strcmp 그룹(`0xFCF6D/6F`, guarded load 폴백 2,530/90초, HLE 재진입
  2,533)은 그대로다. DS가 0x0024/0x0080/0x002B 세 값을 돌아 수용 쌍 한
  칸을 밀어내는 구조이므로, 쌍을 집합으로 늘리는 것이 다음 후보다.
* 41초 부근의 두 번째 정지는 원인 미확정.

---

# Work log: INT8 ISR cache entry and the unconditional shadow read slot (issue #18, direction 3 phase 3)

Settled the two remaining paths on the phase-2 build: memcpy's `mov ax,
ds` fell back in the guarded read slot (33k per 30 s) and the ISR entry
helper `0xFE2F0` faulted natively outside the cache (6.4k per 30 s)
because `InjectPendingInterrupts` entered the handler at the raw guest
vector. The injection now enters the handler's cache block on the direct
model, translating it once on first use (`FindAotCacheAddress`, then one
`RequestAotDynamicTranslation`), with an opt-out variable and an entry
counter line; the i386 read slot is an unguarded 12-byte `mov r16,
[shadow]; jmp`.

Interleaved 90 s pumpitea runs against the phase-2 build: post-logo gap
2.13/2.07 s → **1.65/1.70/1.69 s**; handled segment loads 25.9k →
**5,056** (run-invariant); privileged exceptions 235k → 41.5k, AVs 46.5k
→ 12.5k, total exceptions 707k–925k → 521k–547k (INT3s rose slightly as
the ISR's boundaries moved into the cache); INT8 entries 20.7k cache / 1
translated / 0 native. The second stall near 41 s recurred in both builds
(unresolved). Both probes pass; the three-title smoke is clean (zero
guest exceptions, return pad pushed = returned, all INT8 entries through
the cache). Remaining: the far-strcmp group (2,530 fallbacks per 90 s,
accepted-pair thrash between 0x0024 and 0x0080) and the 41 s stall.
