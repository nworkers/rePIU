# 작업 로그: 세그먼트 재해석의 선택적 재패치 시도와 기각 (issue #18)

작업 지시: `docs/work-orders/20261006-i018-selective-segment-repatch.md`
설계: `docs/design/20261006-i018-selective-segment-repatch.md` (기각 판정 포함)
분석: `docs/analysis/pumpitea-loading-segment-flip.md`

## 한 일

1. issue #18의 수정 방향 1(바뀐 세그먼트 레지스터의 지점만 재패치)과
   2(기록되는 페이지 범위만 `ProtectMemory`)를 구현했다.
   * `runtime::PatchAot*Sites` 네 패처에 `segment_mask`(기본 전체) 추가.
   * `ReResolveWin32AotSegmentOverrides`에 override/guarded 마스크와 기록
     범위 사전 패스를 추가해 해당 페이지만 보호·플러시.
   * `ReResolveAotSegmentOverrides`가 레지스터별 해석 비교로 마스크를
     만들고, `ProtectMemory` 실패가 아니면 해석 스냅숏을 갱신.
   * 검증용으로 `repiu_aot_probe --selector-guard` 단독 실행 플래그 추가.
     (전체 probe 체인은 main에서도 `dbt_indirect_dispatch_call_layout/
     placement=false`로 그 앞에서 멈춰 selector guard까지 도달하지 못함 —
     기존 문제로, 이 작업과 무관.)
2. selector guard probe 통과(`selector_guard_all=true`)를 확인하고
   pumpitea 재현 절차(90초, frame-rate 로그)로 공백을 측정했다.
3. 결과가 기대와 반대여서 guarded 마스크 기준을 바꾼 v2(해석 변화 기준)도
   측정했고, 열 스로틀 교란을 배제하기 위해 기준선을 교대로 3회 측정했다.

## 측정 (2026-10-06, 같은 기계, 교대 실행)

| 빌드 | 공백(초) | handled segment load (90초) | guarded load 성공/폴백 |
|---|---|---|---|
| 기준선 main(v0.0.205) ×3 | 18.7 / 15.0 / 19.6 | 42,556~47,318 | 47,547~68,421 / 5,964~7,297 |
| 마스크 v1 ×3 | 35.1 / 45.5 / 45.2 | 106,262~117,847 | 12,512 / 40,761 |
| 마스크 v2 ×2 | 52.6 / 60.8 | 73,490~77,161 | 25,714~29,851 / 23,410~25,505 |

기준선 3차는 v2 직후 측정(열 영향 배제). 재패치당 지점 기록은 1/5~1/12로
줄었지만(selector guard 합산 210M→45M) 공백은 2~3배 늘었다.

## 결론

캐시 전량 재패치는 해석 갱신만 하는 것이 아니라, 게스트 페이지 리타이어가
INT3로 닫은 슬롯의 prologue를 flip마다 되살리는 부수효과를 갖고 있었다
(기각 근거와 메커니즘 추정은 분석 문서). "재패치는 가드가 자기 검증하므로
순수 최적화"라는 설계 전제가 거짓이어서 **방향 1·2를 기각**하고 엔진·런타임
변경을 되돌렸다. 남긴 코드는 `--selector-guard` 플래그뿐이다.

함께 확정한 사실(분석 문서에 누적):

* flip의 정체는 게스트 memcpy 관용구(`push es; mov ax,ds; mov es,ax;
  rep movsd; mov es,[saved]`), 가상 DS=0x0024 / ES=0x002B.
* v0.0.180이 빨랐던 이유: DS를 가상으로 추적하지 않아 `mov es,ax`가
  no-op 로드였고 가드가 네이티브로 성공했다. 가상 추적이 실기에 충실하다.
* 로딩 구간은 예외 디스패치(30초에 ~29.7만 건)가 지배한다.

## 남은 일 (issue #18 계속)

1. 리타이어된 엔트리로의 스테일 유입을 건전하게 다시 잇는 경로 설계
   (전량 재패치의 우연한 복원을 명시적 메커니즘으로 대체).
2. flip 자체 제거(issue 방향 3): memcpy 관용구의 shadow 간 세그먼트 복사
   네이티브 처리 또는 지점별 두 selector 기억.
3. 뜨거운 재진입 지점(`0x04101297`, `0x04101BAE`, `0x041012D3`) 규명.

## 검증

* `scripts/build_win32_x86_release.ps1 -Target repiu,repiu_aot_probe` 성공.
* 되돌린 상태에서 `repiu_aot_probe --selector-guard` 통과.
* pumpitea 90초 실행 3+3+2회, 30초 census 1회(로그는 세션 스크래치에 보관,
  수치는 위 표와 분석 문서에 기록).

---

# Work log: selective segment re-patch attempt and rejection (issue #18)

## What was done

1. Implemented issue #18's fix directions 1 (re-patch only the changed
   register's sites) and 2 (`ProtectMemory` over only the written page
   range): `segment_mask` on the four runtime patchers, masks plus a
   written-span pre-pass in `ReResolveWin32AotSegmentOverrides`, and
   per-register change masks in `ReResolveAotSegmentOverrides`. Added a
   standalone `repiu_aot_probe --selector-guard` flag for verification
   (the full probe chain already stops earlier on main at
   `dbt_indirect_dispatch_*=false`, unrelated to this task).
2. Verified `selector_guard_all=true`, then measured the pumpitea gap
   per the issue's reproduction (90 s, frame-rate log).
3. The result inverted expectations, so a v2 (guarded mask by changed
   resolution) was measured too, with three interleaved baselines to
   exclude thermal drift.

## Measurements: baselines 18.7 / 15.0 / 19.6 s; masked v1
35.1 / 45.5 / 45.2 s; masked v2 52.6 / 60.8 s (table above). Per-re-patch
site writes dropped to 1/5–1/12, yet the gap grew 2–3x.

## Conclusion

The whole-cache re-patch is load-bearing beyond resolution updates: its
prologue restore keeps resurrecting slots that guest-page retirement
closed with INT3. The design premise (the re-patch is purely an
optimization) is false, so directions 1 and 2 were rejected and the
engine/runtime changes reverted; only the probe flag remains. Confirmed
along the way: the flip is the guest memcpy idiom (virtual DS 0x0024 vs
ES 0x002B), v0.0.180 was fast because it did not track a virtual DS, and
the loading phase is exception-dispatch bound (~297k exceptions per
30 s). Remaining work is listed above and in the analysis topic
`docs/analysis/pumpitea-loading-segment-flip.md`.
