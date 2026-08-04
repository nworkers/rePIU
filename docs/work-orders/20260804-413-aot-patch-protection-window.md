# Task 413 작업 지시 — AOT patch 보호 구간 좁히기

설계: [20260804-413](../design/20260804-413-aot-patch-protection-window.md)

## 변경 파일

| # | 파일 | 변경 |
|---|---|---|
| 1 | `src/platform/win32/aot_code_cache_win32.cpp` | 익명 namespace에 `AotCachePatchWindow`·`ComputeAotCachePatchWindow`·`AotPatchWideProtectEnabled`·`AotPatchPageSize` 추가. inline-cache patch 경로의 `VirtualProtect` 3곳을 창 기반으로 교체 |

**이 과제는 파일 하나입니다.** dynamic append와 segment patch 경로는 건드리지
않습니다(설계 §4).

## 구현 규칙

* 쓰는 바이트·순서·결과 코드를 바꾸지 않습니다. 바뀌는 것은 보호 창의 크기뿐입니다.
* 창 계산이 이상하면 **전체 캐시로 물러섭니다.** 좁게 잘못 잡히는 경로가 없어야 합니다.
* 조기 복귀 경로에서도 반드시 같은 창으로 RX를 복원합니다.
* `REPIU_AOT_PATCH_WIDE_PROTECT`로 예전 동작을 되살릴 수 있어야 합니다(A/B 필수).

## 검증

1. `cl /Zs` 문법 검사 후 Release 빌드 오류 0.
2. **A/B, census 끔, 같은 세션, EEPROM 실행별 격리** — wide 3회, narrow 3회,
   각 60초. 판정은 frames와 `DOS path trace #`
   ([재현 가이드](../guides/pumpit3-stall-reproduction.md)).
3. 정확성 확인: 두 조건 모두 `icache patch #` 진단이 이어지고, 예외 census에 새 코드가
   없어야 합니다.
4. 결과에 따라 설계 §5의 사전 등록 분기를 따릅니다.

## 산출물

작업 로그, [AOT worker inline cache](../analysis/aot-worker-inline-cache.md) 갱신,
[pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md) 갱신, frontier 갱신.

---

# Task 413 Work Order — narrow the AOT patch protection window

Design: [20260804-413](../design/20260804-413-aot-patch-protection-window.md)

## Files

One file: `src/platform/win32/aot_code_cache_win32.cpp` gains
`AotCachePatchWindow`, `ComputeAotCachePatchWindow`, `AotPatchWideProtectEnabled`, and
`AotPatchPageSize` in its anonymous namespace, and the inline-cache patch path's three
`VirtualProtect` calls take the window. The dynamic-append and segment-patch paths are left
alone (design section 4).

## Implementation rules

The bytes written, their order, and the result codes stay the same; only the window size
changes. An unusable range falls back to the whole cache, so no path can end up narrower
than what it writes. Early returns restore RX through the same window.
`REPIU_AOT_PATCH_WIDE_PROTECT` must restore the old behaviour, since the A/B depends on it.

## Verification

Syntax-check, then a zero-error Release build; then A/B with the census off in one session
with the EEPROM isolated per run — three wide and three narrow runs of 60 seconds, judged on
frames and `DOS path trace #` counts. Both conditions must keep issuing `icache patch #`
diagnostics with no new exception class. Follow the design's pre-registered branch on the
result.

## Deliverables

The work log plus updates to the AOT worker inline-cache analysis, the pumpit3 stall
analysis, and the frontier.
