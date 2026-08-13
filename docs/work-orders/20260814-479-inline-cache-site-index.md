# inline-cache 패치 site 조회 인덱스 작업 지시

1. `include/repiu/platform/win32/aot_inline_cache_site_index.h`를 추가합니다.
   `Win32AotInlineCacheSiteIndex`(`buckets`, `next_in_bucket`, `indexed_site_count`,
   `lookup_count`, `fallback_scan_count`, `rebuild_count`)와
   `AotInlineCacheSiteLookup`(`usable`, `found`, `site_index`), 그리고
   `RebuildAotInlineCacheSiteIndex`, `EnsureAotInlineCacheSiteIndex`,
   `InvalidateAotInlineCacheSiteIndex`, `LookupAotInlineCacheSiteIndex`를 선언합니다.
2. `src/platform/win32/aot_inline_cache_site_index.cpp`를 추가합니다. 해시는 기존
   `aot_cache_address_index.cpp`와 같은 Knuth 승산 해시를 쓰고, chain 순회는
   `indexed_site_count`로 상한을 둡니다. 조회는 `miss_offset`과 `miss_offset - 1` 두
   키의 chain을 모두 훑어 **가장 작은 site index**를 돌려줍니다.
3. `Win32AotCodeCachePlacement`에 `inline_cache_site_index` 멤버를 추가합니다.
4. `PatchWin32AotIndirectInlineCache`의 선형 탐색을 다음으로 바꿉니다.
   - 진입부에서 `EnsureAotInlineCacheSiteIndex`.
   - 인덱스가 `usable && found`이고 돌려준 site가 키(`miss_cache_offset` 또는 `+1`)와
     실제로 일치하면 그 site를 씁니다.
   - 그 외에는 기존 선형 탐색을 그대로 실행합니다. 인덱스가 "없음"이라고 답한 경우도
     탐색으로 확인한 뒤에만 기존 실패 경로로 갑니다.
   - 폴백이 실행되면 `fallback_scan_count`를 증가시킵니다.
4'. `IsAotInlineCacheMiss`(`aot/aot_runtime_dispatch.cpp`)의 같은 선형 탐색도 같은
   인덱스로 바꿉니다. 이 지점에서는 "없음"을 그대로 신뢰합니다 — "없음"이 흔한 답이라
   탐색으로 확인하면 제거하려는 비용이 돌아옵니다. usable하지 않을 때 탐색으로
   내려가는 것은 패치 경로와 같습니다.
5. `live_telemetry_snapshot.cpp`에서 인덱스 카운터를 attempt 스냅샷으로 옮기고,
   `execution_trampoline.h`에 대응 필드를 추가한 뒤 `main.cpp`에 요약 한 줄을
   `Win32 AOT inline-cache site index sites/indexed/scans/rebuilds`로 남깁니다.
6. `src/tools/aot_probe/aot_inline_cache_site_index_probe.{h,cpp}`를 추가하고
   `main.cpp`(probe)에 등록합니다. 검증 항목은 설계 문서 §검증 1과 같습니다.
7. `CMakeLists.txt`에 새 소스 2개를 등록합니다.
8. Win32 x86 Debug 빌드를 수행하고 `aot_probe`를 실행합니다. `inline_cache_*`와
   새 `inline_cache_site_index_*` 항목이 모두 `true`여야 합니다.
9. 설계와 `docs/analysis/current-execution-frontier.md`를 갱신하고 작업 로그를 남긴 뒤
   하나의 작업 커밋으로 정리합니다.
10. 사용자 구동 A/B로 Task 478 + 479를 함께 판정합니다. `pumpit8` 동일 장면 3회,
    vsync OFF. 프레임당 패치 수와 primitive 수가 3% 이내로 일치하고 cycle당 swap과
    cycle당 primitive가 같은 방향일 때만 fps 비교를 인정합니다.

# Inline-cache patch site index work order

1. Add `include/repiu/platform/win32/aot_inline_cache_site_index.h` declaring
   `Win32AotInlineCacheSiteIndex` (`buckets`, `next_in_bucket`,
   `indexed_site_count`, `lookup_count`, `fallback_scan_count`, `rebuild_count`),
   `AotInlineCacheSiteLookup` (`usable`, `found`, `site_index`), and
   `RebuildAotInlineCacheSiteIndex`, `EnsureAotInlineCacheSiteIndex`,
   `InvalidateAotInlineCacheSiteIndex`, `LookupAotInlineCacheSiteIndex`.
2. Add `src/platform/win32/aot_inline_cache_site_index.cpp` using the same Knuth
   multiplicative hash as `aot_cache_address_index.cpp`, bounding chain traversal
   by `indexed_site_count`. A lookup walks the chains for both `miss_offset` and
   `miss_offset - 1` and returns the **lowest** matching site index.
3. Add an `inline_cache_site_index` member to `Win32AotCodeCachePlacement`.
4. Replace the linear scan in `PatchWin32AotIndirectInlineCache`:
   call `EnsureAotInlineCacheSiteIndex` on entry; use the indexed site when the
   lookup is usable, found, and the returned site really matches the key
   (`miss_cache_offset` or `+1`); otherwise run the original scan unchanged,
   including confirming a "not found" answer before taking the existing failure
   path; and count each fallback in `fallback_scan_count`.
4b. Index the same scan in `IsAotInlineCacheMiss`
   (`aot/aot_runtime_dispatch.cpp`), which runs before every patch attempt on both
   the indirect and return dispatch paths. There a "not found" is trusted rather
   than confirmed — "no" is the common answer, so confirming it would restore the
   cost being removed — while an unusable index falls through to the scan exactly
   as in the patch path.
5. Snapshot the index counters in `live_telemetry_snapshot.cpp`, add the matching
   fields in `execution_trampoline.h`, and log one summary line in `main.cpp` as
   `Win32 AOT inline-cache site index sites/indexed/scans/rebuilds`.
6. Add `src/tools/aot_probe/aot_inline_cache_site_index_probe.{h,cpp}` and
   register it in the probe `main.cpp`, covering design §Verification item 1.
7. Register the two new sources in `CMakeLists.txt`.
8. Run the Win32 x86 Debug build and `aot_probe`; `inline_cache_*` and the new
   `inline_cache_site_index_*` items must all report `true`.
9. Update the design and `docs/analysis/current-execution-frontier.md`, leave a
   work log, and land one task commit.
10. Judge Tasks 478 and 479 together from the user's A/B: `pumpit8`, same section,
    three runs, vsync off. The fps comparison counts only when per-frame patches
    and primitives agree within 3% and swaps per cycle and primitives per cycle
    move in the same direction.
