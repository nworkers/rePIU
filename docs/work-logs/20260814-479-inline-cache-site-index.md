# inline-cache 패치 site 조회 인덱스 작업 로그

## 요약

Task 478이 남긴 1순위 — inline-cache 패치 단가 — 를 다뤘습니다. site 조회의 선형
탐색을 `miss_cache_offset` 해시 인덱스로 바꿨고, **같은 탐색이 있던 두 번째 지점
`IsAotInlineCacheMiss`도 함께** 바꿨습니다.

관련: [설계](../design/20260814-479-inline-cache-site-index.md),
[작업 지시](../work-orders/20260814-479-inline-cache-site-index.md),
[current-execution-frontier](../analysis/current-execution-frontier.md)

## 근거 (Task 478 측정, 2026-08-13, vsync OFF)

| 항목 | 값 |
|---|---:|
| 패치 호출 수 | 1,203,695 (프레임당 1,742) |
| 패치 회당 | 약 75,100 cycle (약 20 µs) |
| `guest-run` 대비 | 약 28.7% |
| site 수 | 8,019 |

`AotIndirectInlineCacheSite`는 32비트 빌드에서 약 44~48바이트이므로 배열이 약
353~385 KB이고, 첫 일치에서 멈추는 탐색은 패치마다 평균 절반(약 176~192 KB)을
스트리밍합니다. 캐시 라인 약 3,000개이며 회당 75,100 cycle과 자릿수가 맞습니다.

## 구현 중 확인한 사실 — 같은 탐색이 한 곳 더 있었습니다

`IsAotInlineCacheMiss`([`aot_runtime_dispatch.cpp:853`](../../src/platform/win32/aot/aot_runtime_dispatch.cpp#L853))가
**키도 배열도 같은 선형 탐색**이었고, indirect dispatch(`:1310`)와 return
dispatch(`:1529`) 양쪽에서 패치를 시도하기 직전에 호출됩니다.

| | 패치 경로 | `IsAotInlineCacheMiss` |
|---|---|---|
| 호출 조건 | miss로 판정된 경우만 | **모든 indirect·return dispatch** |
| 불일치 시 | — | **8,019개 전부 순회(약 353 KB)** |

즉 패치 1회에 이 탐색이 최소 2회 들어가고, 패치로 이어지지 않는 dispatch마다
**최악의 경우**가 한 번씩 더 실행되고 있었습니다. Task 478이 "나머지 약 28.7%,
대부분 IC 패치"로 적은 버킷에는 이 호출도 함께 들어 있습니다. 한쪽만 고쳤다면 같은
배열을 도는 비용이 그대로 남았을 것입니다.

## 한 일

| # | 변경 | 파일 |
|---|---|---|
| 1 | `miss_cache_offset` 해시 인덱스 모듈 신설 | `platform/win32/aot_inline_cache_site_index.{h,cpp}` (신규) |
| 2 | placement에 `inline_cache_site_index` 멤버 추가 | `platform/win32/aot_code_cache_win32.h` |
| 3 | 패치 경로 조회를 인덱스로, 실패 시 기존 탐색으로 폴백 | `platform/win32/aot_code_cache_win32.cpp` |
| 4 | `IsAotInlineCacheMiss` 조회를 인덱스로 | `platform/win32/aot/aot_runtime_dispatch.cpp` |
| 5 | 조회·폴백·재구축 카운터 스냅샷과 요약 줄 | `telemetry/live_telemetry_snapshot.cpp`, `execution_trampoline.h`, `host/win32/main.cpp` |
| 6 | 등가성 probe 11항목 신설 | `tools/aot_probe/aot_inline_cache_site_index_probe.{h,cpp}` (신규), `tools/aot_probe/main.cpp` |
| 7 | 소스 등록 | `CMakeLists.txt` |

설계 결정 중 기록해 둘 것 둘:

* **인덱스는 캐시입니다.** `indexed_site_count != sites.size()`면 조회가 스스로
  "사용 불가"를 보고하고 호출자는 기존 탐색을 그대로 돌립니다. 인덱스가 돌려준 site도
  키와 다시 대조하므로, 개수가 유지된 채 offset만 바뀌는 변경이 나중에 생겨도 잘못된
  site를 패치하지 않습니다. 실패 모드가 **느려지는 것**이지 틀리는 것이 아닙니다.
* **"없음" 처리는 두 지점이 다릅니다.** 패치 경로는 탐색으로 한 번 더 확인한 뒤에만
  실패로 보고합니다(희귀 + 로그 경로라 비용이 없습니다). `IsAotInlineCacheMiss`는
  "없음"이 흔한 답이므로 그대로 신뢰합니다 — 확인하면 없애려는 비용이 돌아옵니다.

증분 append 링크(Task 324가 쓴 방식)는 도입하지 않았습니다. 같은 프로파일에서 dynamic
translate는 263회, 패치는 1,203,695회이므로 append 배치마다 O(n) 재구축 한 번은
무시할 수 있고, 훅이 줄면 append 경로의 실수 여지도 줄어듭니다.

## 검증

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug 빌드 (`repiu_aot_probe`) | 통과 (exit 0, 기존 C4819 경고만) |
| Win32 x86 Debug 빌드 (`repiu`) | 통과 (exit 0) |
| `aot_probe` (pumpit8 `PIU.EXE`) | **exit 0**, 전 항목 통과 |
| 기존 `inline_cache_*` 11항목 | 전부 `true` (실제 캐시 메모리에 패치·재패치·retire) |
| 신설 `inline_cache_site_index_*` 11항목 | 전부 `true` |

새 probe는 변경 전 조회를 oracle로 그대로 옮겨 두고 인덱스 답과 대조합니다.

| 항목 | 무엇을 막는가 |
|---|---|
| `spaced` | 정상 배치에서 두 키가 각각 옳은 site로 |
| `adjacent` | miss offset이 1 차이인 두 site — 배열 순서상 앞선 site가 이겨야 함(양쪽 순서 모두) |
| `duplicate` | 같은 키를 가진 site 둘 — 낮은 index가 이겨야 함 |
| `collisions` | 해시 충돌 chain에서 키 재비교 |
| `offset_wrap` | offset 0과 `0xFFFFFFFF`의 부호 없는 wrap이 기존 `+ 1U`와 같게 |
| `append` / `rebuild` | append 직후에는 조회가 거절하고, `Ensure` 뒤에는 다시 답하는지 (300회) |
| `invalidated_fallback` | 무효화 상태에서 답하지 않는지 |
| `empty` / `unindexed` | site가 없거나 인덱스를 만든 적 없는 placement에서 답하지 않는지 |

### 사용자 구동 A/B 결과 (2026-08-14)

Task 478·479를 함께 적용한 동일 구간 3회는 판정 조건을 충족했습니다. 기준 대비
patch/swap은 평균 **+0.02%**, primitive/swap은 **+2.07%**로 모두 3% 이내였고,
cycle/swap과 cycle/primitive는 세 실행 모두 같은 방향으로 감소했습니다.

| 지표 | 기존 | 개선 후 3회 평균 | 변화 |
|---|---:|---:|---:|
| cycle/swap | 54.916M | 42.454M | **−22.69%** |
| cycle/primitive | — | — | **−24.26%** |
| swap/wall-second | 66.81 | 86.28 | **+29.14%** |

세 실행 모두 Task 479 index `scans=0`, residency `0`, inline-cache patch 성공률 100%,
dynamic translation `266/266`, return fallback 0이었습니다. Task 478 단독 대조군이
없으므로 Task 479 단독 효과는 분리해 확정하지 않습니다.

### 사용자 구동 A/B 절차 (재현용)

1. `pumpit8`, vsync OFF(`REPIU_GLIDE_SWAP_INTERVAL=0`),
   `REPIU_EXECUTION_TIME_PROFILE=1`.
2. **같은 구간**을 3회 재현합니다.
3. 판정 조건: 프레임당 패치 수와 primitive 수가 3% 이내로 일치하고, cycle당 swap과
   cycle당 primitive가 같은 방향일 것.
4. 새 요약 줄 `Win32 AOT inline-cache site index sites/indexed/scans/rebuilds`에서
   `scans`가 0에 가까워야 합니다. 크면 인덱스가 무효화된 채 돌고 있다는 뜻이고 이번
   변경의 효과도 그만큼 없습니다.
5. `pumpit1` 회귀 확인.

## 회고

* **같은 결함이 두 곳에 있는 패턴이 또 나왔습니다.** Task 324가 `FindAotCacheAddress`를
  고치고 Task 334가 같은 모양의 `FindAotGuestAddress`를 뒤늦게 고쳤는데, 이번에도
  패치 경로와 `IsAotInlineCacheMiss`가 같은 탐색이었습니다. **선형 탐색 하나를 고칠
  때는 같은 배열을 같은 키로 도는 다른 호출자를 먼저 세어 보는 편이 낫습니다.**
* **"핫 경로 여부"는 호출 횟수만이 아니라 실패 시 비용으로도 봐야 합니다.**
  `IsAotInlineCacheMiss`는 대부분 `false`를 돌려주는 조용한 질의처럼 보이지만, 바로
  그 `false`가 배열 전체를 도는 최악의 경우였습니다.
* **계측을 함께 넣었습니다.** 인덱스가 조용히 무효화되면 성능만 원래대로 돌아가고
  아무 신호도 남지 않습니다. `scans` 카운터가 그 상태를 로그 한 줄로 드러냅니다.

---

# Inline-cache patch site index work log

## Summary

Took up the first item Task 478 left — the inline-cache patch's unit price. The
site lookup's linear scan is now a `miss_cache_offset` hash index, applied at
**both** places that carried that scan: the patch itself and
`IsAotInlineCacheMiss`.

## Evidence (Task 478 measurement, 2026-08-13, vsync off)

| Item | Value |
|---|---:|
| Patch calls | 1,203,695 (1,742 per frame) |
| Per patch | ≈75,100 cycles (≈20 µs) |
| Share of `guest-run` | ≈28.7% |
| Sites | 8,019 |

`AotIndirectInlineCacheSite` is about 44-48 bytes in a 32-bit build, so the array
is ≈353-385 KB and a scan stopping at the first match streams half of it — about
176-192 KB, some 3,000 cache lines — per patch. That is the right order of
magnitude for 75,100 cycles.

## Found while implementing — the same scan existed in a second place

`IsAotInlineCacheMiss`
([`aot_runtime_dispatch.cpp:853`](../../src/platform/win32/aot/aot_runtime_dispatch.cpp#L853))
was the **same scan over the same array with the same key**, called just before
every patch attempt from both indirect dispatch (`:1310`) and return dispatch
(`:1529`). It is called on *every* dispatch, not only on the ones that patch, and
its "no" answer walks all 8,019 sites — the worst case. One patch therefore
carried at least two of these scans, and every non-patching dispatch paid a full
one. The bucket Task 478 recorded as "the remainder, ≈28.7%, mostly the patch"
contains this call too; fixing only the patch would have left the same array walk
in place.

## What changed

A new `platform/win32/aot_inline_cache_site_index.{h,cpp}` holds the index; the
placement gains an `inline_cache_site_index` member; both lookups use it and fall
back to the original scan; the index's lookup, fallback, and rebuild counters are
snapshotted and logged; a new probe with eleven checks compares the index against
the pre-change lookup; and `CMakeLists.txt` registers both sources.

Two design points worth keeping:

* **The index is a cache.** When `indexed_site_count` does not equal the site
  count the lookup declares itself unusable and the caller runs the original scan,
  and the site it returns is re-checked against the key, so a later
  size-preserving mutation cannot cause a wrong patch. The failure mode is *slow*,
  not *wrong*.
* **"Not found" is treated differently in the two places.** The patch path
  confirms it with the scan before reporting failure — that path is rare and
  already logs, so the confirmation is free. `IsAotInlineCacheMiss` trusts it,
  because "no" is the common answer there and confirming it would restore exactly
  the cost being removed.

Task 324's incremental append linking was deliberately not reproduced: the same
profile ran 263 dynamic translations against 1,203,695 patches, so one O(n)
rebuild per append batch is negligible and fewer hooks means fewer ways for the
append path to get it wrong.

## Verification

| Item | Result |
|---|---|
| Win32 x86 Debug build (`repiu_aot_probe`) | Passed (exit 0, only pre-existing C4819 warnings) |
| Win32 x86 Debug build (`repiu`) | Passed (exit 0) |
| `aot_probe` (pumpit8 `PIU.EXE`) | **exit 0**, every item passed |
| Existing `inline_cache_*` (11 items) | All `true` — real patches, repatches, and retirement against live cache memory |
| New `inline_cache_site_index_*` (11 items) | All `true` |

The new probe keeps the pre-change lookup verbatim as an oracle and compares:
ordinary spacing; two sites whose miss offsets are one apart, in both array orders
(the earlier site must win); duplicate keys (lowest index wins); a forced hash
collision (the chain must re-compare the key); offset zero and the `0xFFFFFFFF`
unsigned wrap, matching the scan's `+ 1U`; 300 appends, where the lookup must
decline before `Ensure` and answer after it; an invalidated index; and placements
that are empty or were never indexed.

**User-driven A/B result (2026-08-14).** Three matching runs with Tasks 478 and
479 together met the acceptance rule: patches per swap differed by **+0.02%**
and primitives per swap by **+2.07%**, while cycles per swap and per primitive
fell in every run. Mean cycles per swap fell **22.69%**, cycles per primitive
fell **24.26%**, and swaps per wall-second rose **29.14%**. All three runs had
Task 479 `scans=0`, residency zero, 100% patch success, dynamic translation
`266/266`, and zero return fallback. There is no Task-478-only control, so the
Task 479 contribution is not isolated.

## Retrospective

* **The same defect in two places, again.** Task 324 fixed
  `FindAotCacheAddress` and Task 334 later fixed the identically shaped
  `FindAotGuestAddress`; here the patch path and `IsAotInlineCacheMiss` were the
  same scan. **When fixing one linear scan, count the other callers walking the
  same array with the same key first.**
* **"Hot" is not only call count — it is also the cost of the negative answer.**
  `IsAotInlineCacheMiss` looks like a quiet predicate that usually returns
  `false`, and that `false` was precisely the full-array walk.
* **Instrumentation shipped with the fix.** A silently invalidated index would
  restore the old cost with no other symptom; the `scans` counter makes that state
  one line in the log.
