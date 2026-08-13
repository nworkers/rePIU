# return dispatch site 조회 인덱스 작업 로그

## 요약

Task 479 이후에도 `guest-run`의 약 45%를 차지한 return handler에서 별도 선형 탐색을
찾아 제거했습니다. `ResolveAotDbtReturnMissFrame`은 return miss마다
`dbt_return_dispatch_sites`를 순회했으며, pumpit8 초기 plan의 return 1,036개에 대해
실행당 약 210만 회 호출됐습니다.

## 변경

* `aot_return_dispatch_site_index.{h,cpp}`에 `miss_cache_offset` exact hash index를
  추가했습니다.
* placement가 index를 소유하며 site count가 달라지면 전체 rebuild합니다.
* return adapter는 usable index를 우선 사용하고, stale 또는 검증 실패 시 기존 scan을
  그대로 실행합니다.
* `sites/lookups/scans/rebuilds` 종료 요약을 추가했습니다.
* 기존 scan을 oracle로 두고 정상 키, 중복, collision, stale/rebuild, invalidate, 빈
  배열을 검증하는 probe를 추가했습니다.

## 검증

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug `repiu_aot_probe` 빌드 | 통과, 기존 C4819/LNK4217 경고만 있음 |
| Win32 x86 Debug `repiu` 빌드 | 통과, 기존 C4819 경고만 있음 |
| pumpit8 `aot_probe` | exit 0 |
| 신규 `return_dispatch_site_index_*` 8항목 | 전부 `true` |
| 기존 `inline_cache_*`, `dbt_return_*`, Task 479 index probe | 전부 통과 |

전체 빌드 스크립트는 구성과 광범위한 header 재컴파일 때문에 첫 60초 실행 두 번이 시간
제한에 걸렸지만, 같은 생성 트리에서 대상 빌드를 끝까지 계속해 exit 0을 확인했습니다.

## 남은 측정과 다음 축

사용자 동일 장면 로그에서 새 요약의 `scans`가 0에 가깝고 return fallback이 계속 0인지
확인해야 합니다. 이번 작업은 lookup 단가만 줄이며, return dispatch와 inline-cache patch
횟수는 바꾸지 않습니다. 다음 구조적 최적화는 hot return site를 megamorphic으로 판정해
4-entry PIC를 매번 다시 쓰는 비용을 피하는 정책입니다.

---

# Return Dispatch Site Lookup Index Work Log

## Summary

Removed a separate linear scan from the return handler that still occupied about
45% of `guest-run` after Task 479. `ResolveAotDbtReturnMissFrame` scanned
`dbt_return_dispatch_sites` on every miss: roughly 2.1 million calls per run
against 1,036 returns in the initial pumpit8 plan.

## Changes

Added an exact `miss_cache_offset` hash index owned by the placement; rebuild on
site-count mismatch; index-first lookup with the old scan retained for stale or
failed validation; final `sites/lookups/scans/rebuilds` telemetry; and an oracle
probe covering ordinary keys, duplicates, collisions, stale/rebuilt state,
invalidation, and an empty array.

## Verification

Win32 x86 Debug `repiu_aot_probe` and `repiu` built successfully with only the
existing C4819/LNK4217 warnings. The pumpit8 probe exited 0, all eight new
`return_dispatch_site_index_*` checks passed, and the existing inline-cache,
DBT-return, and Task 479 index probes remained green.

Two initial 60-second build attempts timed out during configuration and broad
header recompilation; continuing the same generated build tree completed with
exit 0.

## Remaining measurement and next axis

A matching user scene must confirm near-zero scans and continued zero return
fallback. This task lowers lookup price only. The next structural optimization
is an explicit megamorphic-return policy that avoids rewriting the four-entry PIC
on every dispatch.
