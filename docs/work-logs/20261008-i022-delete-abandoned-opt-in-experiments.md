# 작업 로그: 버려진 옵트인 실험의 기능과 스위치 삭제 (issue #22)

작업 지시: `docs/work-orders/20261008-i022-delete-abandoned-opt-in-experiments.md`
설계: `docs/design/20261008-i022-delete-abandoned-opt-in-experiments.md`
조사: `docs/analysis/environment-toggle-inventory.md` 2번 묶음

## 한 일

1. **범위 조사**: 13개 변수의 코드·probe·스크립트·문서 흔적을 세 갈래로 나눠 찾았다. 그
   과정에서 간접 디스패치 핸들러만 무장하던 진단 `REPIU_AOT_DBT_CALL_STEP`(Task 285)이
   함께 죽는다는 것을 확인해 범위에 넣었다.
2. **이전 동작 복원 스위치 5개**: `SEGMENT_WRITE_BLOCKS_RESUME`, `QUARANTINE_FIRST_WRITE`,
   `QUARANTINE_ON_GENERATION_FAILURE`, `STRICT_SPANNING_ENTRY`, `PATCH_WIDE_PROTECT`의
   판정 함수와 조건 한 줄씩을 지웠다.
3. **AOT-DBT**
   * post-HLE 번역: 판정 함수 두 개와 cache miss 분기의 opt-in 조건. trace의 단계 이름은
     기본값 그대로(`cache-miss-non-identical`/`cache-miss-gate-disabled`).
   * segment-override hybrid: 빌드 옵션·이미지·배치 필드, 동반 HLE 슬롯 방출과 검증,
     `AotSegmentOverrideSite::dispatch_cache_offset`과 그 패치·rebase·trace 필드, loader
     로그 줄. 방출 바이트는 기본 `je 0x02; popfd; int3` 그대로.
   * 간접 디스패치: 빌드 옵션 3개, `AotDbtIndirectDispatchSite`와 이미지·배치 벡터, miss
     꼬리 방출(기본 `popfd; int3`만 남김), site 해석·rebase·provenance,
     `aot_dbt_indirect_dispatch.*`, miss thunk 네 곳(Win32 naked, Linux x86 `.S`, direct·
     cache 주소 함수), 카운터와 loader 로그 블록. thunk 주석의 개수와 거절 바이트 목록을
     고쳤다(다섯 → 넷, `21` 삭제).
   * call-step 진단: `aot_dbt_call_step_probe.*`, 컨텍스트·보고 구조체 필드, trampoline의
     무장·처리 지점과 환경 변수 읽기, snapshot, loader 로그 블록.
4. **native**
   * region: 진입·이탈·Dr 처리·VEH 블록, `ScanNativeRegionWithZydis`, 상태 필드와 카운터,
     `kBreakpointNativeRegion`. single-step 처리는 `TryEnterNativeFastPath`만 부른다.
   * span 캐시·쓰기·점프: 양성 캐시, 쓰기 횡단 판정, 점프 연결, `NativeLinearSpanOptions`
     와 analyzer의 `options` 인자, 해당 카운터. `write fault-cancel`은 기본 span이 쓰므로
     남겼다.
   * retired reentry: 래퍼, 정책, `HandleAotReentry`의 호출, 카운터(컨텍스트·보고·공유
     telemetry), `ExecutionTimeBucket::kAotReentryNativeSpan`.
5. **출력 형식 변화**(모두 항상 0이던 값):
   * `[repiu-live]`: `region=`, `span_cache=`, `span_jump=`, `retired_span=` 제거,
     `span_write=`는 세 값 → fault-cancel 한 값.
   * loader 최종 로그: span cache, span jump, retired span, 간접 디스패치, call-step,
     segment-override enabled 줄 제거. span write 줄은 `write fault-cancel: N`으로.
     `aot reentry cycles/count/share` 세 줄에서 `native-span` 열 제거.
   * supervisor 줄의 `retired_span=` 제거, 공유 live telemetry 버전 23 → 24.
   * `[repiu-aot-dynamic] stage=image-segment-site` trace의 `dispatch=` 필드 제거.
   * `VehExitSite`의 세 값은 번호를 지키려고 "퇴역" 주석과 함께 남겼다.
6. **probe**: 간접 디스패치 probe와 call-step probe 파일 삭제, long mode probe의
   `ProbeIndirectFallbackStackCleanup` 삭제, selector guard probe의 post-HLE 정책 표와
   hybrid 두 단언을 기본 레이아웃 단언 `segment_override_default_layout` 하나로, span
   probe의 쓰기·점프·캐시·정책 단언 삭제와 retired 블록을 `TryEnterNativeLinearSpan`
   직접 호출(`linear_span_entry_behavior`)로 교체. `repiu_aot_probe` 전체 체인이 #20 때
   앞단에서 멈추던 `dbt_indirect_dispatch` probe는 이번에 삭제되었다.
7. **스크립트**: `task283`~`task287`, `task413`, `benchmark_native_linear_span.ps1` 삭제.
   `task347`에서 `REPIU_AOT_DBT_POST_HLE_TRANSLATE`와 #20 이후 이미 죽은
   `REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE`를 비우는 줄을 뺐다.
8. **문서**: ARCHITECTURE 일곱 곳(span 실험, post-HLE, Stage 4 간접 디스패치, 세대 실패와
   spanning entry — 옛 이름 `CanActivateWin32AotAddressMapEntry`도 바로잡음, reject cache,
   retired span, hybrid segment-override), pumpit3 멈춤 가이드(arena 낙하는 Task 417 이전
   빌드로만 재현), analysis 목록(삭제 기록과 스크립트 참조 정정), TODO.

## 검증

이 환경은 Linux(Ubuntu 26.04.1, RTX 4090, GNOME Wayland — 실행은 x11)라 Win32는 빌드하지
못했다. 기준선은 같은 기계에서 main(`79c33d8`, v0.0.207)을 따로 빌드해 썼다.

* **빌드**: Linux x64(cache)·i386(direct) Release, 모든 기본 타깃 통과. 바뀐 파일에서 새
  경고 없음(원래 있던 `g_repiu_active_thread_context` 경고 하나뿐).
* **core probe**: 두 아키텍처 모두 종료 코드 0. 기준선 출력과 비교해 차이는 삭제한
  `indirect_fallback_call_stack_restored=…` 줄(x64)과 i386의 할당 주소값뿐이다.
  i386의 `shutdown_recovery_policy_wide_pointer=false`는 기준선에도 있는 값이다.
* **변수 읽기**: `src/`·`include/`에 13개 변수와 `REPIU_AOT_DBT_CALL_STEP` 문자열이 없다.
* **롬셋 30초 스모크**(`scripts/survey_romsets.sh`, 실제로 뜨는 16개 롬셋, 나머지 6개는
  CHD 디렉터리가 없어 양쪽 모두 즉시 종료): 양쪽 모두 fault 0, untranslatable 0, 종료
  실패 0. 프레임 수는 양쪽 모두 실행마다 크게 흔들렸다. 낮은 쪽은 전부 fps가 1.0에 붙는
  #6의 "숨겨진 창에서 vsync swap이 막히는" 상태였고(느린 상태 실행 수: x64 기준 7/새 11,
  i386 기준 6/새 6), 어느 롬셋이 빠지는지는 실행마다 달랐다.
* **교대 재실행**(롬셋마다 기준·새 순서를 번갈아, 30초): 스모크에서 갈린 x64 10개
  롬셋은 느린 상태가 양쪽에 한 번씩(기준 pumpipx2, 새 pumpit3a), 나머지 18회는 프레임
  ±5% 이내로 일치. i386 8개 롬셋은 16회 모두 정상이고 ±3% 이내로 일치. x64 스모크의
  7/11 차이는 환경 요인으로 판정했다.
* **pumpitea 90초**(교대): x64 두 쌍은 모두 정상이고 로고 다음 공백(8초 지점)이 기준
  1.18/1.21초, 새 1.24/1.21초, 40초 지점 정지가 기준 1.58초(다른 한 번은 따라잡기 구간),
  새 1.63/1.83초, 프레임 5063·5524 / 5088·5049. i386은 첫 쌍이 양쪽 모두 느린 상태
  (93/94프레임), 둘째 쌍은 새 빌드가 처음 29초 느린 상태였다. 추가 한 쌍은 양쪽 모두
  정상으로 프레임 5134/5166, 40초 지점 정지 1.14/1.10초, 로고 다음 공백은 둘 다 1.03초
  미만. fault는 전부 0. (#18의 1.5~2.7초는 Win32 값이며, Linux에서 이 공백은 원래 짧다.)
* **확인하지 못한 것**: Win32 빌드와 `repiu_aot_probe`(selector guard·native span probe를
  고쳤다), Win32 supervisor와 Win32 thunk 파일. 이 부분은 이 환경에서 컴파일조차 되지
  않으므로 Win32 빌드로 확인해야 한다.

---

# Work log: delete the abandoned opt-in experiments and their switches (issue #22)

**Done.** Deleted the 13 variables' features and switches, plus `REPIU_AOT_DBT_CALL_STEP`
(Task 285), a diagnostic armed only by the indirect dispatch handler. The five rollback
switches lose their function and one condition each. Post-HLE translation loses its opt-in
(trace stage names keep their default values). Segment-override hybrid dispatch loses its
options, companion slot emission, validation, `dispatch_cache_offset` with patch, rebase and
trace field, and its loader line; the slot stays `je 0x02; popfd; int3`. Indirect dispatch
loses its options, site type and vectors, the miss tail (only `popfd; int3` remains), site
resolution, rebase and provenance, its handler files, the four miss thunks and its counters
and loader block; thunk comments now count four and drop `21`. The call-step diagnostic goes
whole. Native region, the span cache, write crossing, jump chaining
(`NativeLinearSpanOptions` and the analyzer's `options` argument with them) and retired-span
reentry go with their counters and the `kAotReentryNativeSpan` bucket; `write fault-cancel`
stays because default spans use it. Output changes, all of always-zero values: `[repiu-live]`
drops `region=`, `span_cache=`, `span_jump=`, `retired_span=` and shrinks `span_write=` to one
value; the final loader log drops the matching lines and the `native-span` column of the
three `aot reentry` lines; the supervisor drops `retired_span=`; shared live telemetry goes to
version 24; the image-segment-site trace drops `dispatch=`. `VehExitSite` keeps its three
unused values as retired slots. Probes: the indirect dispatch and call-step probes and the
long-mode indirect fallback sub-probe go; the selector guard probe keeps a single
`segment_override_default_layout` check; the span probe drops the write, jump, cache and
policy checks and replaces the retired block with a direct `TryEnterNativeLinearSpan` check
(`linear_span_entry_behavior`). The `dbt_indirect_dispatch` probe that stopped the full
`repiu_aot_probe` chain in #20 is gone. Seven A/B scripts were deleted and `task347` lost two
dead variable lines. ARCHITECTURE (seven places, including the stale
`CanActivateWin32AotAddressMapEntry` name), the pumpit3 stall guide, the analysis inventory and
TODO were updated.

**Verified** on Linux (Ubuntu 26.04.1, RTX 4090, GNOME Wayland, runs on x11) against main
(`79c33d8`, v0.0.207) built separately on the same machine; Win32 could not be built here.
Linux x64 (cache) and i386 (direct) Release builds of every default target pass with no new
warnings in changed files. The core probe exits 0 on both; compared with the baseline, the
only differences are the deleted `indirect_fallback_*` line (x64) and i386 allocation
addresses. No string of the 13 variables or `REPIU_AOT_DBT_CALL_STEP` remains in `src/` or
`include/`. A 30 s smoke over the 16 romsets that start (six lack a CHD directory and exit at
once on both sides) shows zero faults, zero untranslatable sites and zero shutdown failures on
both sides; frame counts swing widely on both, the low ones all being #6's hidden-window
state with fps pinned at 1.0 (x64 7 base / 11 new, i386 6 / 6, different romsets each run).
Interleaved 30 s reruns: on x64's ten divergent romsets the slow state hit each side once
and the other 18 runs match within ±5%; on i386's eight, all 16 runs are healthy and match
within ±3% — the x64 7/11 split is environmental. Interleaved 90 s pumpitea: x64 post-logo gap
1.18/1.21 s base vs 1.24/1.21 s new, frames 5063/5524 vs 5088/5049; i386's first two pairs hit
the slow state (both sides in one, the new build's first 29 s in the other), and an extra pair
was healthy on both sides with 5134/5166 frames, the 40 s stall at 1.14/1.10 s and the
post-logo gap under 1.03 s on both. All faults zero. **Not verified:** the Win32 build,
`repiu_aot_probe` (the selector guard and native span probes changed), the Win32 supervisor and
the Win32 thunk file, which do not even compile here and need a Win32 build.
