# Megamorphic return patch 우회 작업 로그

## 요약

Task 480에서 조회 단가는 감소했지만 실행당 수백만 return miss가 거의 같은 수의 PIC
patch를 계속 발생시켰습니다. DBT host-stack return site별로 miss와 고유 target을 관찰하고,
4-entry PIC가 수렴할 수 없다는 반복 증거가 있는 site의 재패치만 생략하도록 구현했습니다.

## 변경

* `aot_return_patch_policy.{h,cpp}`에 site별 miss 수, 최대 8개 고유 target과
  megamorphic 상태를 추가했습니다.
* initial placement와 dynamic append가 정책 배열을 return dispatch site 수에 맞춥니다.
* return site lookup이 정확한 site index를 공용 return resolver에 전달합니다.
* 16 miss와 8개 고유 target을 모두 만족한 호출부터 patch를 생략합니다. 기존 PIC entry,
  target resolution, stack/레지스터 결과와 fallback은 변경하지 않았습니다.
* 종료 요약에 `observations/megamorphic/bypasses`를 추가했습니다.
* 단일/4-way/8-way, 임계값, 지속 우회, site 독립성과 append 보존 probe를 추가했습니다.

## 검증

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug `repiu_aot_probe`, `repiu` 빌드 | exit 0 |
| Win32 x86 Release `repiu_aot_probe`, `repiu` 빌드 | exit 0 |
| pumpit8 Debug/Release `aot_probe` | 모두 exit 0 |
| 신규 `return_patch_policy_*` 9항목 | 전부 `true` |
| 기존 `inline_cache_all` | `true` |
| 기존 `dbt_return_fallback_all` | `true` |
| Task 479/480 site-index probe | 전부 `true` |

공용 placement header 변경으로 전체 의존 소스가 재컴파일됐습니다. 60초 제한의 앞선 세
시도는 오류 없이 중단됐고, 프로세스를 유지한 최종 빌드는 203.4초에 완료됐습니다. 기존
C4819와 LNK4217 경고만 남았습니다.
Release 전체 재컴파일도 216.7초에 exit 0으로 완료됐습니다.

## 사용자 측정 결과

| 지표 | Task 480 평균 | Task 481 평균 | 변화 |
|---|---:|---:|---:|
| patch / return | 100.01% | 2.39% | **-97.61%** |
| patch bypass | 0% | 97.61% | 활성화 |
| return당 handler cycle | 19,652 | 1,275 | **-93.51%** |
| return handler / guest-run | 54.36% | 17.13% | -37.23%p |

세 실행에서 megamorphic site는 38~39개였고 return 성공률 100%, fallback 0,
return index scan 0, dynamic translation `266/266`, fatal 0과 thread exit 0을 유지했습니다.
`observations = bypasses + return patches` 회계도 일치하며 전체 patch에 추가로 남은
165~166건은 indirect patch입니다.

return/swap은 Task 480보다 55.29% 높고 primitive/swap은 31.85% 낮아 장면 동등성은
성립하지 않았습니다. 따라서 FPS와 전체 cycle/swap 변화는 Task 481에 귀속하지 않습니다.
직접 정책 지표인 patch/return과 return당 cycle은 모든 Task 481 실행이 Task 480 범위보다
크게 낮아 최적화 성공으로 판정합니다.

---

# Megamorphic Return Patch Bypass Work Log

## Summary

Task 480 reduced lookup cost but left millions of return misses producing nearly
as many PIC patches. The implementation now observes misses and distinct targets
per DBT host-stack return site and skips only repatching where repeated evidence
shows that the four-entry PIC cannot converge.

## Changes

Added per-site miss count, up to eight distinct targets, and megamorphic state;
synchronized policy storage at initial placement and dynamic append; passed the
exact return site index into the shared resolver; bypassed patching from the call
that reaches both sixteen misses and eight targets; reported
`observations/megamorphic/bypasses`; and added probes for one-, four-, and
eight-way behavior, threshold, persistent bypass, isolation, and append
preservation. Existing entries, resolution, guest-visible stack/register state,
and fallback are unchanged.

## Verification

The Win32 x86 Debug and Release probes and applications built with exit 0,
retaining only the existing C4819 and LNK4217 warnings. Both pumpit8 full probes
exited 0; all nine new policy checks, `inline_cache_all`,
`dbt_return_fallback_all`, and the Task 479/480 site-index probes passed. Three
initial attempts were stopped by the 60-second limit during broad recompilation;
the uninterrupted Debug build completed in 203.4 seconds and Release in 216.7
seconds.

## User measurement result

Across three runs, patch/return fell from 100.01% to 2.39% (-97.61%), mean
bypass reached 97.61%, and handler cycles per return fell from 19,652 to 1,275
(-93.51%). Return-handler share fell from 54.36% to 17.13%. The runs classified
38-39 sites and retained 100% return success, zero fallback, zero return-index
scans, dynamic translation `266/266`, zero fatal state, and thread exit zero.
`observations = bypasses + return patches`; the extra 165-166 total patches are
indirect patches.

Scene equivalence did not hold: returns per swap rose 55.29% and primitives per
swap fell 31.85%. FPS and whole-run cycles per swap are therefore not attributed
to Task 481. The direct policy metrics and non-overlapping cycles-per-return
ranges establish the optimization as successful.
