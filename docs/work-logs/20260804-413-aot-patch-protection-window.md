# Task 413 작업 로그 — AOT patch 보호 구간 좁히기 (효과 있음, 원인은 아님)

설계: [20260804-413](../design/20260804-413-aot-patch-protection-window.md) ·
작업 지시: [20260804-413](../work-orders/20260804-413-aot-patch-protection-window.md)

## 1. 한 줄 결과

보호 구간을 16 MB에서 **쓰는 페이지**로 좁혔습니다. **멈춤은 사라지지 않았습니다**
(healthy 0/3 대 0/3). 설계 §5에 등록한 대로, 이 결과는 **patch 1회의 가격이 아니라
다른 것이 지배 항목**이라는 뜻입니다.

## 2. 변경

`src/platform/win32/aot_code_cache_win32.cpp` 한 파일입니다. inline-cache patch 경로의
`VirtualProtect` 3곳(진입 RW, rel32 초과 조기 복귀, 정상 복귀)이 **쓰는 14바이트를
덮는 페이지 창**을 씁니다. 창 계산이 이상하면 전체 캐시로 물러섭니다.
`REPIU_AOT_PATCH_WIDE_PROTECT=1`이면 예전 동작이며, **A/B가 한 바이너리 안에서**
가능합니다.

## 3. 측정 — 사전 등록한 1차 종점은 통과하지 못했습니다

같은 빌드·같은 세션·census OFF·EEPROM 실행별 격리, wide/narrow 교대 3회씩 60초.

| run | 조건 | 판정 | frames | traces | publishes | breakpoint 예외 |
|---:|---|---|---:|---:|---:|---:|
| 1 | wide | 멈춤 | 1 | 6 | 100 | 39,644 |
| 2 | narrow | 멈춤 | 0 | 6 | 66 | 53,826 |
| 3 | wide | 멈춤 | 1 | 6 | 100 | 42,912 |
| 4 | narrow | 멈춤 | 0 | 6 | 69 | 48,362 |
| 5 | wide | 느림 | 1 | 7 | 100 | 46,953 |
| 6 | narrow | 멈춤 | 0 | 6 | 67 | 50,046 |

**1차 종점(frames ≥ 100)은 양쪽 다 0/3입니다.** 두 조건 모두 `icache patch #` 진단이
12,288까지 이어졌고 예외 census에 새 코드는 없습니다(정확성 확인).

**부수 관측 — 같은 시간에 breakpoint 예외가 약 15% 늘었습니다**(wide 39,644~46,953 대
narrow 48,362~53,826). patch 1회가 싸졌으니 같은 시간에 더 많이 처리한 것으로 읽히지만,
**3회씩으로는 단정할 수 없습니다** — 이 세션의 실행은 격리 모드(예외 총수 72만)와
비격리 모드(7만~30만)가 섞여 분산이 지배적입니다.

## 4. 그래도 변경을 유지하는 이유

1. **정확성에 손해가 없습니다.** 쓰는 바이트·순서·결과 코드가 같고, 실패 시 예전
   동작으로 물러섭니다.
2. **RW 창이 작아집니다.** 예전에는 patch 동안 캐시 **전체**가 쓰기 가능해졌고, 그
   순간 다른 스레드가 캐시 어디를 실행하든 실행 권한이 없었습니다. 지금은 관련 없는
   페이지가 RX로 남습니다.
3. **가격 자체는 실제로 컸습니다.** 같은 호스트에서 16 MB `VirtualProtect` 쌍은
   **4,225 µs / 약 11.5 M cycle**로 측정됐습니다(Task 412 부수 측정).

## 5. 무엇이 틀렸나 — 가설의 산술이 과했습니다

"patch 12,288회 × 11.5 M cycle = 141 G cycle"은 실행 전체 예산(120초 실행에서
guest-run 126 G)을 넘습니다. **넘는 값이 나왔으면 그 자리에서 가설을 의심했어야
합니다.** 실제로는 실행 중 patch 경로가 그만큼 돌지 않거나(진단 counter는 하한),
`VirtualProtect` 가격이 벤치마크와 다릅니다(벤치마크는 매 반복 다른 페이지를 dirty로
만들었습니다). Task 412의 실측은 그 자리를 **여러 경로의 합**으로 채웁니다 —
세그먼트 override 재해석 약 15%, JAMMA 스냅샷 약 10%, `WriteGuestBytes` 약 14%,
`FindAotCacheAddress` 약 13%, inline-cache patch 요청 약 7%.

## 6. 다음

* **miss 횟수 축.** 4-entry inline cache로 정적 호출처 259곳의 return을 담을 수
  없습니다([AOT worker inline cache](../analysis/aot-worker-inline-cache.md) 참조).
* **세그먼트 override 재해석**이 host 표본 최대 인구입니다. 같은 경로도 전체 캐시
  보호를 쓰므로, 좁히기 + 호출 빈도 측정이 다음 후보입니다.
* 멈춤 자체의 기전은 여전히 미확정입니다(Task 412 §4).

---

# Task 413 Work Log — narrowing the AOT patch protection window (real, but not the cause)

## 1. Result in one line

The protection window went from 16 MB to **the pages actually written**, and **the stall
did not go away** (0/3 healthy in both conditions). Per the design's registered branch, that
means the dominant cost is **not the price of one patch**.

## 2. Change

One file, `src/platform/win32/aot_code_cache_win32.cpp`: the inline-cache patch path's three
`VirtualProtect` calls take a page window covering the fourteen bytes written, falling back
to the whole cache when the range is unusable, with `REPIU_AOT_PATCH_WIDE_PROTECT=1`
restoring the old behaviour so **the A/B lives in one binary**.

## 3. Measurement — the primary endpoint failed

Three wide and three narrow 60-second runs, alternating, census off, EEPROM isolated per
run: frames were 1/0/1/0/1/0 and path traces 6/6/6/6/7/6, so **frames ≥ 100 was 0/3 in both
conditions**. Both kept issuing `icache patch #` diagnostics to 12,288 with no new exception
class. Breakpoint exceptions rose about 15% in the same wall time (39,644-46,953 wide
against 48,362-53,826 narrow), which reads as more patches processed once each got cheaper —
but **three runs per condition cannot carry that claim**, because this session mixes
quarantined runs (720,000 exceptions) with unquarantined ones (72,000-307,000) and the
variance dominates.

## 4. Why the change stays

Accuracy is unchanged — same bytes, same order, same result codes, with a fallback to the
old behaviour. The writable window shrinks: previously the **entire** cache became writable
during a patch, so any thread executing cache code at that instant had no execute
permission; unrelated pages now stay RX. And the price was real: the 16 MB `VirtualProtect`
pair measures **4,225 µs (about 11.5 M cycles)** on this host.

## 5. What was wrong — the arithmetic overshot

"12,288 patches times 11.5 M cycles" is 141 G cycles, which exceeds the whole guest-run
budget (126 G in the 120-second run). **A figure that exceeds the budget should have been
challenged on the spot.** Either the patch path does not run that often (the diagnostic
counter is a lower bound) or `VirtualProtect` costs less in situ than in a benchmark that
dirtied a different page every iteration. Task 412's measurement fills the slot with **a sum
of paths** instead: segment-override re-resolution about 15%, the JAMMA snapshot about 10%,
`WriteGuestBytes` about 14%, `FindAotCacheAddress` about 13%, and the inline-cache patch
request about 7%.

## 6. Next

The **miss-count** axis, since four inline-cache entries cannot cover a return site with 259
static call sites; **segment-override re-resolution**, the largest host population, which
uses the same whole-cache protection and whose call frequency is unmeasured; and the stall
mechanism itself, still unresolved (Task 412 section 4).
