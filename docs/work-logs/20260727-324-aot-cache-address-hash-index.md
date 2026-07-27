# 20260727-324 작업 로그: AOT cache 주소 해시 색인 / Work log

설계: [20260727-324-aot-cache-address-hash-index.md](../design/20260727-324-aot-cache-address-hash-index.md)

작업 지시: [20260727-324-aot-cache-address-hash-index.md](../work-orders/20260727-324-aot-cache-address-hash-index.md)

## 한국어

### 결론 요약

`FindAotCacheAddress`의 선형 탐색을 버킷 체인 해시 색인으로 교체했습니다. 호출당
비용은 `1,047,784 tick`에서 `6,866 tick`으로 **99.3% 감소**했고, 같은 60초 동안
guest heartbeat는 `79,640 → 331,913`(**4.17배**), progress는 `8,199 → 21,843`
(**2.66배**)로 증가했습니다.

동시에 설계가 세운 가설 하나가 **기각**됐습니다. VEH 내부이면서 single-step handler
밖인 구간은 73.76%에서 74.34%로 **줄지 않았습니다.** 그 구간은 `FindAotCacheAddress`와
다른 원인을 가지며 별도 계측이 필요합니다.

### 구현 개요

1. 신규 모듈 `aot_cache_address_index.{h,cpp}`
   - `Win32AotCacheAddressIndex`: `indexed_entry_count`, 2의 거듭제곱 `buckets`,
     `address_map`과 평행한 `next_in_bucket`.
   - 체인은 최신을 head로 연결합니다. 따라서 head부터 첫 active 일치가 "최신 active",
     체인 끝까지의 마지막 일치가 "최초 삽입"이 되어 두 규칙이 한 순회로 처리됩니다.
   - 버킷에는 충돌로 다른 guest 주소가 섞이므로 순회 중 `guest_address` 비교를
     유지합니다.
2. `Win32AotCodeCachePlacement`에 `cache_address_index` 필드 추가.
3. `FindAotCacheAddress`는 색인 유효 시 색인 경로, 무효 시 **기존 선형 탐색을 원문
   그대로** 실행합니다.
4. 갱신 훅
   - `InitializeWin32AotPageCoherence`: 시작 시 무효화, 종료 시 `Ensure`.
   - `RegisterWin32AotAddressMap`: `AppendAotCacheAddressIndexEntry`.
   - 동적 append 루프 뒤: `Ensure` 안전망.

### 설계에서 조정한 점

초안은 비증분 append에서 전체 재구축을 하도록 했으나, `InitializeWin32AotPageCoherence`가
`RegisterWin32AotAddressMap`을 항목마다 호출하므로 초기화가 O(n^2)가 됩니다(26,710
항목 기준 약 7억 연산). 비증분 호출은 재구축 대신 **무효화**하고, 첫 증분 append가
`address_map`의 현재 크기로 버킷을 잡도록 바꿨습니다. 그 결과 초기화 루프가 재구축
없이 O(n)으로 색인을 완성합니다.

### 의미 보존 검증

이 작업의 성패는 성능이 아니라 조회 의미 보존입니다. 잘못된 세대로 점프하면 guest
실행이 조용히 오염됩니다. 신규 차등 probe는 **교체 이전 구현을 oracle로 그대로
보관**하고, 모든 guest 주소와 그 이웃 주소·미스 주소에 대해 두 구현의 결과가
완전히 일치하는지 확인합니다.

| 검증 항목 | 결과 |
|---|---|
| retired 목록이 빈 placement (최신 중복이 있어도 최초 항목) | `true` |
| retired 주소와 비retired 주소 혼재 | `true` |
| 최신 항목 inactive, 이전 항목 active | `true` |
| 모든 세대 inactive (양쪽 모두 미스) | `true` |
| 강제 해시 충돌 주소 쌍 | `true` |
| 동적 append 300회 + 버킷 성장 + retired 추가 | `true` |
| 색인 무효 상태 fallback | `true` |
| 훅을 거치지 않은 placement | `true` |

### 검증 결과

1. Win32 x86 Debug 전체 빌드 통과.
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과.
   `aot_cache_address_index_all=true` 포함 모든 `*_all=true`.
3. 60초 `aot-dbt` A/B. 두 실행 모두 정상 timeout, AOT legacy fallback 0,
   malformed dispatch 0, EEPROM SHA-256 `A1FC1D...52570` 일치.
   `[repiu-fatal]`은 기존 정책대로 `action=continue`인 Glide hint 진단 1건뿐입니다.

기준선은 Task 323의 `on3` 실행이며 동일 profile, 동일 설정, 이 변경 외 동일 바이너리입니다.

| 지표 | 교체 전 | 교체 후 | 변화 |
|---|---:|---:|---:|
| **progress** | 8,199 | 21,843 | **+166%** |
| **heartbeat** | 79,640 | 331,913 | **+317%** |
| single-step | 29,958 | 155,342 | +418% |
| AOT boundary | 30,099 | 51,360 | +71% |
| `kCacheLookup` tick | 11,395,704,478 | 148,029,142 | **-98.7%** |
| `kCacheLookup` 호출당 | 1,047,784 | 6,866 | **-99.3%** |
| `kAotResume` tick | 12,987,145,872 | 2,220,773,822 | -82.9% |
| single-step handler 합계 | 20,559,155,309 | 12,500,460,702 | -39.2% |
| VEH 합계 | 140,669,834,536 | 133,777,126,498 | -4.9% |
| VEH 비율 | 86.38% | 81.98% | -4.40%p |
| **VEH − single-step handler** | **73.76%** | **74.34%** | **+0.58%p** |

### 판정

설계가 등록한 가설 검증 결과입니다.

* **성립:** `kCacheLookup`은 87.75%에서 사실상 소멸했습니다(`kAotResume`의 6.7%,
  전체 wall-clock의 0.09%).
* **기각:** VEH 내부 미귀속 73.76%는 **함께 줄지 않았습니다.** `ResolveAotTransferTarget`이
  같은 `FindAotCacheAddress`를 호출하므로 같은 원인일 것이라는 가설은 이 A/B로
  기각됩니다. 다음 작업은 그 구간의 자체 계측입니다.

교체 후 `kAotResume` 내부 순위는 `kSpanSafety` 34.8%, residual 33.1%,
`kSegmentWriteProbe` 15.5%, `kQuarantineCheck` 9.9%, `kCacheLookup` 6.7%로 바뀌었습니다.
다만 `kAotResume` 자체가 전체의 1.36%이므로 현재 우선순위가 아닙니다.

### 미확정 / 해석 주의

* **워크로드가 이동했습니다.** 실행이 빨라지면서 guest가 이전에 도달하지 못한 구간
  (texture 로딩, Glide texture gate)까지 진행했고 phase 표시도 `3 → 2`로 달라졌습니다.
  따라서 교체 전후의 stage 구성비를 같은 작업량에 대한 비교로 해석하면 안 됩니다.
  다만 progress, heartbeat, single-step이 2.7~5.2배로 함께 증가했으므로 처리량 개선
  자체는 phase 이동의 부산물이 아니라 실제입니다.
* **Debug 빌드 수치입니다.** MSVC Debug의 iterator debug check가 `std::vector` 순회를
  크게 늦추므로 Release에서 이 교체의 상대 이득은 더 작습니다. 이 A/B 수치를 Release
  이득으로 인용하지 않습니다. O(n) → O(1)이라는 점근 개선만 빌드 구성과 무관합니다.
* VEH 내부 74.34%의 내역은 여전히 미귀속입니다. AOT boundary 51,360회에 대해 VEH
  진입은 196,009회이므로, 경계 1회당 비용을 단순 나눗셈으로 추정하지 않았습니다.

---

## English

### Summary

Replacing the linear `FindAotCacheAddress` scan with a bucket-chain hash index cut per-call
cost from `1,047,784` to `6,866` ticks (-99.3%). Over the same 60 seconds, guest heartbeat
rose from 79,640 to 331,913 (4.17x) and progress from 8,199 to 21,843 (2.66x). The A/B also
rejected a design hypothesis: the share of wall clock inside the VEH but outside the
single-step handler did not fall, moving from 73.76% to 74.34%, so that region has a
different cause and needs its own instrumentation.

### Implementation

A new `aot_cache_address_index` module holds `indexed_entry_count`, a power-of-two `buckets`
array, and a `next_in_bucket` array parallel to `address_map`. Chains link newest-first, so
the first active chain match is the newest active entry and the last chain match is the
oldest entry, reproducing both lookup rules in a single traversal; the `guest_address`
comparison is retained because collisions mix addresses. `FindAotCacheAddress` uses the index
when valid and otherwise runs the original scan verbatim. The index is maintained by
invalidation plus `Ensure` around `InitializeWin32AotPageCoherence`, an incremental append in
`RegisterWin32AotAddressMap`, and an `Ensure` safety net after the dynamic append loop.

The draft design rebuilt on any non-incremental append, which would have made initialization
O(n^2) because `InitializeWin32AotPageCoherence` calls `RegisterWin32AotAddressMap` per entry
(about 700 million operations at 26,710 entries). Non-incremental calls now invalidate
instead, and the first incremental append sizes buckets for the map as it already stands, so
the init loop builds the index in O(n) with no rebuild.

### Semantic verification

Because a wrong answer silently jumps guest execution to the wrong translation generation,
equivalence matters more than speed. The new differential probe keeps the pre-change
implementation verbatim as an oracle and asserts identical results for every mapped guest
address plus neighbours and misses, across empty retired lists, mixed retired and
non-retired addresses, a newest-inactive entry behind an older active one, all-inactive
generations, forced hash collisions, 300 dynamic appends with bucket growth and retired
additions, an invalidated index, and a placement built without the hooks. All eight cases
pass.

### Verification

The full Win32 x86 Debug build and `repiu_aot_probe` passed, including
`aot_cache_address_index_all=true`. Both 60-second `aot-dbt` runs reached their timeout with
zero AOT legacy fallback, zero malformed dispatch, and a matching EEPROM SHA-256, with the
only `[repiu-fatal]` being the pre-existing `action=continue` Glide hint diagnostic. The
baseline is the Task 323 `on3` run under identical profiles and settings.

### Caveats

The workload moved: faster execution carried the guest into content it had not previously
reached (texture loading and Glide texture gates) and the phase marker changed from 3 to 2,
so before/after stage shares are not a comparison over identical work. Progress, heartbeat,
and single-step counts all rose together by 2.7x to 5.2x, so the throughput gain itself is
real rather than an artifact of that shift. These are Debug-build numbers, where MSVC
iterator checks inflate `std::vector` traversal, so the relative gain is smaller in Release
and these figures are not quoted as Release gains; only the O(n) to O(1) change is
build-independent. The composition of the remaining 74.34% inside the VEH is still
unattributed, and per-boundary cost was not estimated by dividing it across the 51,360 AOT
boundaries given 196,009 VEH entries.
