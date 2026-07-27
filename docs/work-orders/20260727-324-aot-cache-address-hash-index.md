# 20260727-324 작업 지시: AOT cache 주소 해시 색인 / Work order

설계: [docs/design/20260727-324-aot-cache-address-hash-index.md](../design/20260727-324-aot-cache-address-hash-index.md)

## 한국어

### 목표

`FindAotCacheAddress`의 `address_map` 선형 탐색을 버킷 체인 해시 색인으로 교체합니다.
Task 323이 측정한 `kAotResume`의 87.75%를 제거하고, VEH 내부 미귀속 73.76%가 같은
원인인지 A/B로 검증합니다. **조회 의미는 완전히 보존해야 합니다.**

### 구현 항목

1. `include/repiu/platform/win32/aot_cache_address_index.h`,
   `src/platform/win32/aot_cache_address_index.cpp` 신규 모듈
   - `Win32AotCacheAddressIndex` 구조체(`indexed_entry_count`, `buckets`,
     `next_in_bucket`).
   - `RebuildAotCacheAddressIndex(placement)` 전체 재구축.
   - `AppendAotCacheAddressIndexEntry(placement, map_index)` 증분 append,
     load factor 초과 시 버킷 배증 후 재구축.
   - `LookupAotCacheAddressIndex(placement, guest_address, newest_active,
     map_index*)` 조회. 색인 무효면 `false`를 반환해 호출자가 fallback하게 합니다.
2. `aot_code_cache_win32.h`
   - `Win32AotCodeCachePlacement`에 `Win32AotCacheAddressIndex cache_address_index`
     필드 추가.
3. `aot_code_cache_win32.cpp`
   - `FindAotCacheAddress`가 색인 유효 시 색인 경로, 무효 시 기존 선형 탐색을
     **원문 그대로** 실행하도록 분기.
4. `aot_page_coherence_win32.cpp`
   - `InitializeWin32AotPageCoherence`에서 전체 재구축 호출.
   - `RegisterWin32AotAddressMap`에서 증분 append 호출.
5. `src/tools/aot_probe/aot_cache_address_index_probe.{h,cpp}` 신규 차등 probe.
6. CMake Win32 source 목록에 신규 파일 두 개 추가, `aot_probe/main.cpp`에 등록.

### 보존해야 할 조회 규칙

| 조건 | 반환 항목 |
|---|---|
| guest 주소가 `retired_guest_addresses`에 있음 | 가장 **최신의 active** 항목 |
| 그 외 (retired 목록이 비었거나 해당 주소가 없음) | 가장 **오래된** 항목, active 무관 |

### 안전 조건

- 위 두 규칙에서 벗어나는 결과를 절대 반환하지 않습니다. 잘못된 세대로의 점프는
  guest 실행을 조용히 오염시킵니다.
- 색인은 캐시입니다. `indexed_entry_count != address_map.size()`이면 기존 선형
  탐색으로 fail-safe합니다.
- 해시 충돌로 다른 guest 주소가 같은 체인에 섞이므로 순회 중 `guest_address`
  비교를 생략하지 않습니다.
- `address_map_states[].active` 변경 시 색인을 갱신하지 않습니다(구조 불변).
- 색인 쓰기는 `address_map` 쓰기와 동일 지점에서만 수행해 새 경합을 만들지 않습니다.
- 기존 `FindAotCacheAddress` 시그니처와 모든 호출 지점을 바꾸지 않습니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과.
   신규 차등 probe가 다음을 모두 검증합니다.
   - retired 목록이 빈 placement
   - retired 세대가 있는 주소와 없는 주소 혼재
   - 최신 항목이 inactive, 이전 항목이 active
   - 해시 충돌 강제 주소 쌍
   - 색인 무효 상태의 fallback 동등성
   - 동적 append 후에도 동등성 유지
3. `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1 REPIU_EXECUTION_TIME_PROFILE=1`
   60초 `aot-dbt` A/B(교체 전 = Task 323 on3 실행, 교체 후 = 신규 실행).
4. EEPROM hash 일치, fatal 0, legacy fallback 0, malformed 0.

Debug 빌드 A/B 수치를 Release 이득으로 인용하지 않습니다.

---

## English

### Goal

Replace the linear `address_map` scan in `FindAotCacheAddress` with a bucket-chain hash
index, removing the 87.75% of `kAotResume` measured in Task 323 and testing by A/B whether
the unattributed 73.76% inside the VEH shares that cause. Lookup semantics must be preserved
exactly.

### Implementation

Add an `aot_cache_address_index` module providing `Win32AotCacheAddressIndex` plus rebuild,
incremental append, and lookup entry points; add the index to
`Win32AotCodeCachePlacement`; branch `FindAotCacheAddress` to the index when valid and to the
original linear scan verbatim when not; call the rebuild from
`InitializeWin32AotPageCoherence` and the append from `RegisterWin32AotAddressMap`; add a
differential probe; and register both new sources in CMake and the probe main.

### Preserved lookup rules

When the guest address appears in `retired_guest_addresses`, return the newest active entry.
Otherwise return the oldest entry regardless of its active flag.

### Safety

Never return a result outside those two rules, since jumping to the wrong generation silently
corrupts guest execution. The index is a cache: mismatched `indexed_entry_count` falls back to
the linear scan. Retain the `guest_address` comparison during chain traversal because
collisions mix addresses. Do not update the index on `active` changes, which leave structure
unchanged. Write the index only where `address_map` is already written, introducing no new
race. Leave the `FindAotCacheAddress` signature and all call sites unchanged.

### Verification

Run the full Win32 x86 Debug build and `repiu_aot_probe`, with the new differential probe
covering empty retired lists, mixed retired and non-retired addresses, a newest-inactive with
older-active case, forced hash collisions, invalidated-index fallback, and equivalence after
dynamic append. Then run a 60-second `aot-dbt` A/B with both Task 323 profiles enabled against
the Task 323 baseline, confirming a matching EEPROM hash with zero fatal, legacy fallback, and
malformed dispatch. Debug A/B numbers are not quoted as Release gains.
