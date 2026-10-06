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

## 이어진 조사 (같은 날 오후)

기각 뒤 원인 조사를 계속해 다음을 확인했다(상세·수치는 분석 문서).

1. **뜨거운 breakpoint 지점의 정체**: 리타이어 재진입이 아니라 DOS
   호출이었다. `calibrate()`(게스트 0x101277, AH=2Ch를 실제 1초 동안
   세어 저장)와 `delay(ms)`(0x1012A9, 저장값×ms만큼 AH=2Ch 스핀),
   lseek(AH=42h). delay 호출처는 초기화의 delay(5000)과 I/O 보드 포트
   (0x2A4/0x2AC/0x2DA) 쓰기 뒤의 delay(100~500)들이다.
2. **시간 예산**(`REPIU_EXECUTION_TIME_PROFILE=1`, 기준선 30초): VEH가
   벽시계의 65.22%(283,208회), VEH 내부의 79.81%가 버킷 밖 잔여.
   호스트 심볼 귀속으로 잔여의 주체가 재패치 경로임을 확인
   (ProtectMemory 29.5% 등 합 ~62%).
3. **v3 실험**: 쓰기 동작을 전혀 바꾸지 않고 `ProtectMemory` 범위만
   capacity(16 MiB)→size로 줄여 측정했으나(32.1/44.1초), 직후 수정
   없는 기준선이 40.2초로 측정되어 **판정 불가**로 폐기했다(코드
   되돌림). 오전의 기준선은 15.0~19.6초였으므로 약 2시간 연속 부하
   뒤의 기계 상태(열 스로틀 추정)가 측정을 교란한다.
4. **실행의 양분(모드) 발견**: 같은 바이너리가 flip이 슬롯 폴백으로
   전부 트랩되는 느린 모드(공백 32~61초)와 거의 트랩되지 않는 빠른
   모드(15~19.6초)로 갈리고, 빌드와 기계 상태가 모드 선택에 함께
   개입한다. 이전에 추정했던 "재패치의 prologue 복원(리타이어 복원)"
   가설은 수정했다 — flip 지점의 INT3는 슬롯 자체의 가드 폴백이다.

측정 규율을 문서화했다: 이 공백의 A/B는 식힌 기계에서 기준선을
교대로 끼워서만 판정하고, 장시간 세션 후반의 수치는 버린다.

## 남은 일 (갱신)

1. 모드 분기 규명(최우선): flip 슬롯의 캐시 상주 시점과 빠른 모드의
   처리 경로 추적.
2. v3(ProtectMemory capacity→size)을 식힌 기계에서 교대 측정으로
   재판정.
3. flip 자체 제거(issue 방향 3)와, 게임 자체 대기 시간(하한) 계산.

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

The whole-cache re-patch is load-bearing beyond resolution updates: the
masked variants changed the run's behavior, not just its cost. The
design premise (the re-patch is purely an optimization) is false, so
directions 1 and 2 were rejected and the engine/runtime changes
reverted; only the probe flag remains. Confirmed along the way: the
flip is the guest memcpy idiom (virtual DS 0x0024 vs ES 0x002B),
v0.0.180 was fast because it did not track a virtual DS, and the
loading phase is exception-dispatch bound (~297k exceptions per 30 s).

The same afternoon continued the investigation: the hot breakpoint
sites are the game's `calibrate()`/`delay(ms)` loops on INT 21h AH=2Ch
and the AH=42h lseek wrapper (the delays follow I/O-board port writes);
the time profile puts VEH at 65.22% of wall with a 79.81% in-bucket
residual that host-symbol attribution assigns to the re-patch path
(~62% of sited samples); a write-identical v3 (`ProtectMemory`
capacity→size) measured 32–44 s but an unmodified baseline measured
40.2 s right after (morning baselines: 15.0–19.6 s), so v3 is
inconclusive under machine drift and was dropped pending an interleaved
re-measurement on a cooled machine; and runs are bimodal (flip
slot-path traps against a fast path), with build and machine state both
influencing the mode. The earlier retirement-resurrect inference was
revised accordingly. Remaining work is listed above and in the analysis
topic `docs/analysis/pumpitea-loading-segment-flip.md`.
