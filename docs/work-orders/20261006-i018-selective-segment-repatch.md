# 작업 지시: 세그먼트 재해석의 선택적 재패치 (issue #18) — 수행 결과: 기각

> 구현과 측정까지 수행했고, 결과가 완료 기준(공백 축소)을 반증해 코드는
> 되돌렸다. 남긴 것: `repiu_aot_probe --selector-guard` 단독 실행 플래그,
> 분석 문서(`docs/analysis/pumpitea-loading-segment-flip.md`), 작업 로그.
>
> Implemented and measured; the result refuted the done-criteria (the gap
> grew), so the code was reverted. Kept: the `--selector-guard` probe
> flag, the analysis topic, and the work log.

설계: `docs/design/20261006-i018-selective-segment-repatch.md`

## 작업 항목

1. `include/repiu/runtime/aot_segment_patch.h`,
   `src/runtime/aot_segment_patch.cpp`
   * 네 패처(`PatchAotSegmentOverrideSites`, guarded load/pop/read)에
     `segment_mask` 매개변수(기본 `0x3F`)를 더하고, 마스크 밖의 지점은
     건너뛰며 `processed`에 세지 않는다.
2. `include/repiu/engine/aot_code_cache.h`, `src/engine/aot_code_cache.cpp`
   * `AotSegmentPatchStats`에 `protect_failure_count`를 더한다.
   * `ReResolveWin32AotSegmentOverrides`에 `override_segment_mask`,
     `guarded_segment_mask`(기본 `0x3F`)를 더한다. 마스크에 걸린 지점의
     기록 범위를 사전 패스로 구해 그 페이지 범위에만
     `ProtectMemory`·`FlushInstructionCacheRange`를 건다. 걸린 지점이
     없으면 아무것도 하지 않는다.
3. `src/engine/aot/aot_runtime_dispatch.cpp`
   * `ReResolveAotSegmentOverrides`가 해석 비교로 `override_segment_mask`,
     shadow 주소 비교로 `guarded_segment_mask`를 만들어 넘기고,
     `ProtectMemory` 실패가 아니면 해석 스냅숏을 갱신한다.
4. 검증
   * Win32 Release 빌드, `repiu_aot_probe` selector guard probe 통과.
   * issue #18 재현 절차로 pumpitea 공백 측정, 결과를 작업 로그와 issue에
     남긴다.

## 완료 기준

* 기본 마스크 경로(배치 초기 패치, probe)의 동작이 그대로다.
* ES 단독 변화에서 guarded 지점 재기록과 캐시 용량 전체 보호가 사라진다.
* pumpitea 로고 다음 공백이 v0.0.180 수준으로 돌아오거나, 돌아오지 않으면
  근본 해결(방향 3)의 필요를 측정으로 보인다.

---

# Work order: selective re-patch on segment re-resolution (issue #18)

Design: `docs/design/20261006-i018-selective-segment-repatch.md`

## 추가 항목 (같은 날 저녁, 역설 판별 후)

5. `repiu_aot_probe --segment-restore`: VEH 재개(NtContinue)가 재개
   컨텍스트의 `SegEs`에 담긴 게스트 selector(0x0024 등)를 물리
   레지스터에 실제로 어떻게 복원하는지 확정하는 probe. INT3 핸들러가
   `SegEs`를 바꿔 재개하고, 재개 지점에서 물리 ES를 읽어 보고한다.
   이것이 guarded 슬롯의 "가드 성공" 물리 경위와, 재패치 속도-가드
   성공률 결합(역설)의 뿌리 판별이다.

## Additional item (same evening, after the paradox discrimination)

5. `repiu_aot_probe --segment-restore`: a probe that establishes what
   the physical segment register actually holds after a VEH resume
   (NtContinue) whose context carries a guest selector (0x0024 and
   friends) in `SegEs`. The INT3 handler rewrites `SegEs` and resumes;
   the resume point reads the physical ES and reports it. This settles
   the physical path of a guarded-slot guard success and roots the
   re-patch-speed/guard-success coupling.

## Items

1. `include/repiu/runtime/aot_segment_patch.h`,
   `src/runtime/aot_segment_patch.cpp`
   * Add a `segment_mask` parameter (default `0x3F`) to the four patchers
     (`PatchAotSegmentOverrideSites`, guarded load/pop/read); skip unmasked
     sites without counting them in `processed`.
2. `include/repiu/engine/aot_code_cache.h`, `src/engine/aot_code_cache.cpp`
   * Add `protect_failure_count` to `AotSegmentPatchStats`.
   * Add `override_segment_mask` and `guarded_segment_mask` (default
     `0x3F`) to `ReResolveWin32AotSegmentOverrides`; compute the written
     byte span of the masked sites in a pre-pass and apply `ProtectMemory`
     and `FlushInstructionCacheRange` to that page range only. Do nothing
     when no site is masked in.
3. `src/engine/aot/aot_runtime_dispatch.cpp`
   * `ReResolveAotSegmentOverrides` builds `override_segment_mask` from the
     resolution comparison and `guarded_segment_mask` from the shadow
     address comparison, passes them down, and updates the resolution
     snapshot unless `ProtectMemory` failed.
4. Verification
   * Win32 Release build; the selector guard probe of `repiu_aot_probe`
     passes.
   * Measure the pumpitea gap with issue #18's reproduction and record the
     result in the work log and the issue.

## Done when

* The default-mask paths (placement-time patch, probes) behave as before.
* An ES-only change no longer rewrites guarded sites nor protects the whole
  cache capacity.
* The post-logo gap returns to the v0.0.180 level, or the measurement shows
  the root fix (direction 3) is required.
