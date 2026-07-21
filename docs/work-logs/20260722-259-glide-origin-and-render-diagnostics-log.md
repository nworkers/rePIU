# 작업 로그 — 화면 상하 반전 수정과 배경 미표시 원인 격리 / Work Log — Vertical Flip Fix and Isolating the Missing Background

* 작성일 / Date: 2026-07-22 (Task 259)
* 브랜치 / Branch: `claude/glide-origin-and-render-diagnostics`
* 선행 / Predecessor: Task 258 (GrLOD_t 수정으로 첫 콘텐츠 렌더 달성)

> **절차 예외 기록.** 사용자 보고("화면이 위아래 뒤집힘", "일부 텍스처가 안 나옴")
> 두 건의 **원인 규명**이 목적이었고, 실제 코드 수정은 origin 인자 전달 1건과
> 진단 계측이다. 설계 문서 없이 진행했으며 근거는 본문에 남긴다.

## 1. 문제 1 — 화면 상하 반전 (해결) / Vertical Flip (Fixed)

**근인 (확인됨).** 두 겹이었다.

1. **enum 오독.** `GrOriginLocation_t`는 `GR_ORIGIN_UPPER_LEFT`=0,
   `GR_ORIGIN_LOWER_LEFT`=1이다. Task 254가 관측값 `origin=1`을 UPPER_LEFT로
   기록해, 게임이 요청한 것과 반대 방향의 투영을 적용했다.
2. **인자 미전달 (더 근본적).** `origin`이 `OpenWindowed`로 전달되지 않고
   y 뒤집힌 `glOrtho`가 하드코딩돼 있었다. 게임이 어떤 값을 넘기든 같은 투영이
   걸리므로, enum 해석이 맞았더라도 다른 origin을 쓰는 타이틀에서 재발했을 결함이다.

**수정.** `OpenWindowed(width, height, color_buffers, aux_buffers, origin)`으로
인자를 받아 투영을 선택한다. LFB 블릿 쿼드는 lock origin과 창 투영 방향이 **각각
독립적으로** 텍스처 v를 뒤집으므로 XOR로 합쳤다 — 두 반전이 조용히 상쇄되어
"우연히 맞아 보이는" 상태를 막기 위함이다. `Close()`에서 상태를 리셋한다.

**검증.** 사용자 화면에서 UI 텍스트가 하단에 위치해 참조 화면의 `INSERT COIN(S)`
자리와 일치하며, 글자 모양·글로우·자간이 참조와 사실상 동일하다.

## 2. 문제 2 — 배경 미표시 (원인 격리, Glide 밖) / Missing Background (Isolated, Outside Glide)

참조 화면의 파란 BGA 배경 + PUMP IT UP 로고가 통째로 비어 있다. **전수 계측으로
렌더러 원인을 배제**했다.

| 계측 | 결과 |
|---|---|
| 삼각형 센서스 (4,000 draw 전수) | 단일 조합 `fn=3 other=1 textured=1`, **최대 232×39** |
| 배경 전달 경로 호출 여부 | `grTexDownloadMipMap@16`·`Partial@40`·`grLfbLock/Unlock` **전부 미호출** |
| 텍스처 포맷 센서스 | `RGB_565`·`ARGB_4444`뿐, 전부 지원·저장·디코드 |

**결론.** 640×480 배경도, 배경을 구성할 타일도 존재하지 않는다. **게임이 배경
draw를 발행하지 않는다.** 렌더러는 제출된 것을 정확히 그리며, UI 텍스트가 참조
화면과 일치하는 것이 그 증거다.

**미확정.** 배경은 BGA(배경 동영상)로 보이며 게임이 BGA 자산을 준비하지 못해
렌더를 건너뛰는 것으로 **추정**되나 확인되지 않았다. 다음 작업은 파일 I/O 경로
추적이며 이는 Glide 밖 영역이다.

## 3. 반증된 가설 2건 / Two Disproved Hypotheses

기록으로 남긴다. 둘 다 그럴듯했고 둘 다 데이터로 죽었다.

1. **"미지원 텍스처 포맷 때문"** — 팔레트 포맷(`P_8`/`AP_88`)이 팔레트 없이
   디코드되는 실제 결함이 코드에 있어 유력해 보였으나(`grTexDownloadTable`이
   no-op), **포맷 센서스 결과 해당 포맷이 등장조차 하지 않는다.** 실재하는 결함이
   현재 증상의 원인은 아니었다.
2. **"배경이 다른 combine 모드를 써서 텍스처가 꺼진다"** — `SetTextureCombineEnabled`
   가 `function==3 && other==1`만 켜므로 구조적으로 가능했으나, **4,000 draw 전부가
   그 조합**이었다.

두 가설 모두 원인을 **렌더러 쪽에서 찾으려는 편향**이었다. 유효했던 것은 표본이
아니라 **전수 집계**다 — 앞선 12개 표본 진단은 배경이 UI 뒤에 올 가능성을 배제하지
못했다.

## 4. 추가한 진단 / Diagnostics Added

전부 env-gated이며 출력에 상한이 있다.

| 환경변수 | 용도 |
|---|---|
| `REPIU_GLIDE_CALL_AUDIT` | ordinal별 최초 호출 + 인자 (Task 257) |
| `REPIU_GLIDE_DRAW_DIAG` | 삼각형 디코드값 + 그리기 전후 백버퍼 non-black |
| `REPIU_GLIDE_TEX_CENSUS` | 텍스처 포맷별 개수·크기·수용 여부·저장 실패 사유 |
| `REPIU_GLIDE_TRI_CENSUS` | combine 모드별 draw 히스토그램 + 최대 크기 |

`REPIU_DUMP_TEXTURE_BMP` 덤프 형식도 고쳤다. 기존 **32비트 top-down BI_RGB**는
합법이지만 32비트 BI_RGB의 4번째 바이트가 규격상 "예약됨"이라 뷰어마다 해석이
다르고 음수 `biHeight`도 드물어, **파일은 정상인데 뷰어가 거부해 "덤프가 안 된
것처럼" 보였다.** 가장 호환성 높은 **24비트 bottom-up**으로 바꾸고, 알파가 있는
포맷은 `_alpha.bmp`를 그레이스케일로 함께 남긴다(투명도 때문에 안 보이는 경우를
눈으로 확인하기 위함).

## 5. 관측 재현성 메모 / Reproducibility Note

콘텐츠 화면은 **키 입력이 있어야** 나온다. `port_io_emulator.cpp`가
`GetAsyncKeyState`로 전역 물리 키 상태를 읽으므로 `keybd_event` 합성 입력이 창
포커스와 무관하게 전달된다. 다만 게스트가 입력 포트를 폴링하는 시점이 구동마다
달라, 고정 시각 1회 입력은 **8회 중 1회만 도달**한 적이 있다. 넓은 창에 걸쳐 반복
탭하는 방식으로 바꾼 뒤 71회 도달로 안정됐다.

키 매핑: F1=TEST, F2=SERVICE1, F5=COIN1, Home/PgUp/Numpad5/End/PgDn = 발판 5종.

---

## English Summary

Two user-reported symptoms were investigated; only one was a renderer defect.

**Vertical flip (fixed).** `GrOriginLocation_t` has `GR_ORIGIN_UPPER_LEFT` = 0 and
`GR_ORIGIN_LOWER_LEFT` = 1, so Task 254's reading of the observed `origin=1` as
UPPER_LEFT was backwards — and more fundamentally the argument never reached the
backend, which hardcoded a y-flipped projection, so any origin would have
produced the same result. `OpenWindowed` now takes the origin and selects the
projection; the LFB blit XORs the lock origin against the projection direction so
the two inversions cannot silently cancel.

**Missing background (isolated, outside Glide).** A census over all 4,000 draws
shows a single combine mode (SCALE_OTHER/TEXTURE, textured) and a maximum
triangle of 232x39 — no background-sized geometry or tiles — while every
alternative delivery path is never called and every texture format in use is
supported. The game never issues background draws. Two hypotheses were disproved
first (unsupported formats; an unsupported combine disabling texturing), both
biased toward blaming the renderer; exhaustive counting rather than sampling is
what settled it. Whether the BGA asset path is the blocker remains open and is a
file-I/O question outside Glide.

Also fixed the BMP texture dump, whose 32-bit top-down form was legal but refused
by viewers — the bytes were always correct, which made it look like the dump had
never happened.
