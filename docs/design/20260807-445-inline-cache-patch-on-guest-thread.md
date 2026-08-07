# Task 445 설계 — 인라인 캐시 패치를 게스트 스레드에서 직접

선행: [190 worker 기반 inline cache](20260712-190-aot-worker-inline-cache.md) ·
[440 crash report](20260807-440-glide-async-present.md) · frontier 항목 5

## 1. 측정 — pumpit2 게스트 스레드의 34%가 이 대기입니다

`REPIU_GUEST_POSITION_CENSUS_DUMP`로 얻은 표본 5,226개 중 **arena·code cache 표본이
0개**입니다. 게스트 스레드는 게스트 코드를 실행하고 있지 않았습니다.

| 표본 비중 | 심볼 |
|---:|---|
| 40.58% | `GlideOpenGlBackend::InvokeOnHostThread+0x25A` |
| **23.98%** | **`RequestAotInlineCachePatch+0x75`** |
| 9.13% | `InvokeOnHostThread+0x28F` |
| **7.84%** | **`RequestAotInlineCachePatch+0x63`** |
| **2.28%** | **`RequestAotInlineCachePatch+0x2E`** |

**`RequestAotInlineCachePatch` 합계 34.1%.** 같은 실행의 워커 카운터가 규모를
확정합니다 — `translate 390 / other 1,721,010`. 번역은 390회뿐이고 나머지는 전부
인라인 캐시 패치이며, **프레임당 385회**입니다.

## 2. 그 함수가 하는 일

```
ResetEvent(complete) → 파라미터 3개 저장 → SetEvent(request)
→ WaitForSingleObject(complete, INFINITE)        ← 게스트 정지
```

워커가 깨어나서 하는 일은 `PatchWin32AotIndirectInlineCache` **한 번**입니다. 그
함수는 사이트를 찾아 `VirtualProtect(RW)` → 14바이트 쓰기 → `VirtualProtect(RX)` →
`FlushInstructionCache`를 합니다. 게스트 스레드가 못 할 일이 하나도 없습니다.

## 3. 원래 설계가 워커를 쓴 두 이유는 지금 성립하지 않습니다

Task 190의 근거는 두 가지였습니다.

**(a) "guest thread는 worker 완료 event를 기다리므로 반쯤 patch된 슬롯을 실행하지
않는다."** — 게스트가 **직접** 패치하면 이 성질은 자동으로 성립합니다. 패치하는 동안
게스트는 캐시를 실행하고 있지 않기 때문입니다. 이 이유는 워커를 **요구하지 않습니다**.

**(b) "translation/patch worker만 `VirtualProtect`를 한다. RWX는 쓰지 않는다."** —
W^X 규율입니다. 이것은 **두 스레드가 동시에 캐시를 만질 때만** 의미가 있는데, 실측하면
그런 일이 없습니다: 워커 요청 지점 셋(`0x246`·`0x2EE`·`0x349`) **모두** `SetEvent`
직후 `WaitForSingleObject(INFINITE)`로 게스트를 세웁니다. **워커는 게스트가 기다리는
동안에만 동작합니다.** 즉 지금의 상호배제는 핸드셰이크가 제공하고 있고, 패치를 게스트가
직접 해도 배제는 그대로입니다 — 여전히 한 번에 한 스레드만 캐시를 만집니다.

덧붙여 `inline_cache_probe`는 이미 이 패치 함수를 **워커 없이 직접** 호출해 검증하고
있습니다. 함수 자체가 워커에 묶여 있지 않다는 증거입니다.

## 4. 변경

`RequestAotInlineCachePatch`가 스위치에 따라 워커 왕복 대신 **같은 함수를 그 자리에서
호출**합니다. 결과 구조체와 반환값 의미는 동일합니다.

| 설정 | 동작 |
|---|---|
| 미설정 | 지금과 동일(워커 왕복) |
| `REPIU_AOT_INLINE_CACHE_PATCH_INLINE=1` | 게스트 스레드에서 직접 패치 |

번역·retire 요청은 **건드리지 않습니다.** 그쪽은 실행 빈도가 390회이고, 워커에 두는
편이 스택·수명 면에서 안전합니다.

## 5. 기대와 한계

패치 1회의 왕복 비용은 표본 비중으로 환산해 약 **6만 cycle**입니다. 직접 호출은
`VirtualProtect` 두 번과 flush가 남으므로 **수천 cycle** 수준으로 봅니다. 1,721,010회에
적용하면 **guest-run의 30% 안팎**이 후보입니다.

| 한계 | 판단 |
|---|---|
| `VirtualProtect` 두 번이 남습니다 | 시스템 호출이므로 공짜가 아닙니다. 남는 비용은 측정으로 확인하고, 필요하면 보호 전환을 묶는 후속 작업으로 |
| 패치 **횟수** 자체는 그대로 | 4-entry 캐시의 thrash(frontier 5)는 별개 축입니다. 이번 작업과 곱해집니다 |
| pumpit1 효과는 미측정 | 같은 구조이므로 방향은 같지만 비중은 다를 수 있습니다 |

## 6. 검증

1. probe — 정책 해석, 그리고 기존 `inline_cache_probe`의 패치 단정이 그대로 통과.
2. pumpit2 스모크 A/B — `aot worker timing other`가 1.7M → 거의 0, position census에서
   `RequestAotInlineCachePatch` 표본 소멸, 프레임 수 비교.
3. 구현 공백 0과 크래시 없음(Task 441 리포터가 지켜봄).

---

# Task 445 Design — patch the inline cache on the guest thread

## 1. Measurement: 34% of pumpit2's guest thread sits in this wait

Of 5,226 position samples, **none** landed in the arena or the code cache — the guest thread was
not running guest code. `RequestAotInlineCachePatch` accounts for **34.1%** of samples across three
sites, and the worker counters give the scale: **390 translations against 1,721,010 other
operations**, all of them inline-cache patches, **385 per frame**.

## 2. What that function does

It resets an event, stores three parameters, signals the worker and blocks on
`WaitForSingleObject(INFINITE)`. The worker then makes **one call** to
`PatchWin32AotIndirectInlineCache`, which locates the site, flips protection to read-write, writes
fourteen bytes, restores execute-read and flushes the instruction cache. Nothing there is beyond
the guest thread.

## 3. Task 190's two reasons no longer hold

**(a) "the guest waits for the worker's completion event, so it never executes a half-patched
slot."** If the guest does the patch itself, that property holds automatically — it is not
executing the cache while it patches. The reason does not require a worker.

**(b) "only the translation/patch worker calls `VirtualProtect`; no RWX state."** That W^X
discipline matters only if two threads can touch the cache at once, and measurement says they
cannot: **all three** worker request sites signal and then block the guest on
`WaitForSingleObject(INFINITE)`. The worker only ever runs while the guest is parked, so the
handshake *is* the mutual exclusion — and it survives moving the patch to the guest, because there
is still exactly one thread touching the cache at a time.

The existing `inline_cache_probe` already calls the patch function **directly, with no worker**,
which is evidence the function was never worker-bound.

## 4. The change

Behind `REPIU_AOT_INLINE_CACHE_PATCH_INLINE`, `RequestAotInlineCachePatch` calls the same function
in place instead of handing it to the worker. The result structure and return value are unchanged.
Translation and retire requests are untouched: at 390 occurrences they are rare, and the worker is
the safer home for their stack and lifetime.

## 5. Expectation and limits

The round trip costs roughly **60,000 cycles** per patch by sample share; the direct call leaves
two `VirtualProtect` calls and a flush, which should be **thousands**. Over 1,721,010 patches that
puts **around 30% of guest-run** in play. What remains: the protection flips are still system
calls, so the residue must be measured and may deserve a follow-up that batches them; the patch
**count** is untouched, since the four-entry thrash is frontier item 5 and a separate axis that
multiplies with this one; and pumpit1's share is unmeasured.

## 6. Verification

The probe covers the policy and keeps the existing patch assertions; a pumpit2 A/B must show
`aot worker timing other` falling from 1.7M to nearly zero with `RequestAotInlineCachePatch`
disappearing from the position census; and the run must stay free of implementation gaps and
crashes, with Task 441's reporter watching.
