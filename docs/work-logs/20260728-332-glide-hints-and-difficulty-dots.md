# 20260728-332 작업 로그: grHints 구현과 난이도 점 진단 / Work log

설계: [20260728-332-glide-hints-and-difficulty-dots.md](../design/20260728-332-glide-hints-and-difficulty-dots.md)

작업 지시: [20260728-332-glide-hints-and-difficulty-dots.md](../work-orders/20260728-332-glide-hints-and-difficulty-dots.md)

## 한국어

### 결론 요약

**항목 1 완료.** 로그에 남아 있던 유일한 미구현 Glide API `_GRHINTS@8`(호출 298회)을
구현했습니다.

**항목 2는 진단 계측까지 완료했고 원인 확정은 실행 로그를 기다립니다.** 다만 이번
분석으로 **가능한 원인 두 가지가 이미 배제됐습니다.**

### 로그 분석으로 확인된 사실

사용자 제공 `repiu_log.txt`(Release 로더 `v0.0.103`, `aot-dbt`, 01:22:03~01:25:51)를
UTF-16에서 변환해 분석했습니다.

| 확인 | 내용 |
|---|---|
| 미구현 API | `_GRHINTS@8` 1건뿐. 다른 critical 없음 |
| 드로잉 primitive | `grDrawTriangle` 8,706회가 **유일**. point/line/polygon/LFB **0회** |
| 텍스처 | 다운로드 42회, `grTexSource` 3,070회 |
| 자산 | `SPR.RES`에 `level.tga`, `PIU.DAT`에 `LEVEL.PTX`(38,619B) 정상 존재 |

**배제됨 1 — no-op 드로잉 경로.** `grDrawPoint`/`grDrawLine`/`grDrawPolygon`은 현재
"draw request accepted without rendering" no-op이지만 **게임이 한 번도 호출하지
않습니다.** 점이 안 보이는 원인이 될 수 없습니다.

**배제됨 2 — `datas\texture` chdir 실패.** 로그에 `chdir C:\PIU\datas\texture`
실패(0x0003)가 반복되지만, 원본 CHD의 ISO9660 트리에 그 디렉터리가 **없고** 마운트
추출기는 트리를 필터 없이 전부 복사합니다. 실제 기판에서도 없는 경로이므로 정상
폴백입니다. (같은 종류의 오진이 과거에 있었으므로 명시적으로 기록합니다.)

### grHints 구현

hint type별로 상태를 기록합니다. `GrVertex` 레이아웃이 ABI로 고정돼 있어 STWHINT는
구조체를 직접 읽는 렌더러의 결과를 바꾸지 않으므로, **이 backend에서는 상태 기록이
완결된 구현**입니다. `GR_HINT_FPUPRECISION`은 기록만 하고 x87 제어 워드를 바꾸지
않습니다. 호스트가 guest의 부동소수 연산을 대신 수행하지 않으며, 제어 워드를 바꾸면
guest 결과가 달라지기 때문입니다("최적화보다 정확성").

알 수 없는 hint type과 STW 예약 비트는 계속 구현 공백으로 보고합니다.

### 난이도 점 — 남은 세 후보와 판별 계측

정적 분석으로는 다음 셋을 가를 수 없습니다.

* **A** 게임이 그 quad를 아예 제출하지 않음(상위 자산/로딩 문제)
* **B** 제출하지만 텍스처가 바인딩되지 않아 untextured로 그려짐
* **C** 텍스처까지 정상인데 이후 상태나 드로잉이 덮음

B는 코드상 실재하는 가능성입니다. `SourceTexture`가 주소를 못 찾으면
`current_texture_`를 비우고 이후 draw가 텍스처 없이 나가며, `grTexSource`는
`GrTexInfo*`를 쓰지 않고 **주소만으로** 바인딩합니다. 다만 실제로 그 일이 일어나는지는
기존 로그에 없습니다. 기존 `REPIU_GLIDE_TRI_CENSUS`는 combine별 집계와 **최대** 크기만
남겨 작은 quad의 존재 여부를 답하지 못합니다.

그래서 `REPIU_GLIDE_DRAW_CENSUS`를 추가했습니다. 48px 이하 quad를 세고 40개를 개별
표본으로(위치·크기·텍스처 바인딩·텍스처 크기·s/t·정점 색·constant color·combine),
500 draw마다 누계를(작은 quad 수, untextured 수, 저장 텍스처 수, `grTexSource` 미스와
마지막 미스 주소) 남깁니다. 판정 규칙은 설계 4절에 **사전 등록**했습니다.

### 검증 결과

1. Win32 x86 Debug 전체 빌드 통과.
2. `repiu_aot_probe` 전체 통과(exit 0), `repiu_glide_issue_probe` pass.
3. 계측은 전부 env-gated 기본 OFF이며 렌더링 경로는 변경하지 않았습니다.

### 실행 계측 결과 (4회, 사용자 실행)

**`grHints` 검증 완료.** 2회차 실행에서 호출 192회, `[repiu-fatal] ... _GRHINTS@8`
라인이 사라졌습니다. critical 항목 0건입니다.

**원인 후보가 순차적으로 기각됐습니다.**

| 회차 | 관측 | 결론 |
|---|---|---|
| 1 | `small=1,422`, `small-untextured=0`, `missing-sources=0` | A(미제출)·B(미바인딩) 기각 |
| 2 | 32×32 점 스프라이트 BMP가 정확히 디코드(알파 원형 마스크 정상) | 디코드 불량 기각 |
| 3 | 64×256 텍스처 draw의 `st=(0,0)~(64,256)` | 좌표는 **텍스처 자체 텍셀 공간**. 정규화 방식 정상 → 좌표 스케일 가설 기각 |
| 4 | 8px 이하 quad **0건** | 게임은 작은 quad를 제출하지 않음. quad는 정상 크기이고 픽셀 단계에서 사라짐 |

### 근인 확정 및 수정 — `grTexTextureMemRequired`

4회차의 원시 바이트 덤프가 답을 줬습니다.

```
memrequired ptr=0x041CEB9C smallLod=0 largeLod=0 aspect=0 format=0 -> bytes=8192
raw=05 00 00 00 | 05 00 00 00 | 03 00 00 00 | 0A 00 00 00 | 00 00 00 00
```

게스트 메모리에는 **정상적인 5필드 `GrTexInfo`** (smallLod=5, largeLod=5, aspect=3,
format=10)가 있는데 디코드 값은 전부 0이었습니다. **`info`를 게스트에서 읽어오는 코드
자체가 없었습니다.** 포인터는 `IsGuestRangeReadable` 검사에만 쓰이고, 기본 생성된
빈 구조체가 그대로 계산에 들어갔습니다.

그 결과 **모든 텍스처가 8192바이트**로 답해졌습니다(LOD 0 + aspect 8x1 + 1바이트/텍셀
= 256×32×1). 게스트는 이 답으로 자기 TMU 주소 공간을 배치하므로, 실제 128KB인
256×256 맵을 **`0x2000` 간격으로 쌓아 서로 덮어썼습니다.** 네 번의 계측에서 반복
관측된 균일한 `0x2000` 간격은 게임의 성질이 아니라 **우리 출력의 되먹임**이었습니다 —
`docs/kb/glide-texture-lod-and-formats.md`가 경고한 순환 그대로입니다.

수정: 가독성 검사 후 구조체를 실제로 `memcpy`합니다. 계산 실패는 이제 별도 사유로
보고합니다.

**이 수정은 점 증상을 고치지 못했습니다.** 적용은 확인됐지만(8×8 → 128B,
256×256 → 131,072B) 화면은 그대로였습니다. "덮어써진 텍스처" 인과 가설은 **기각**
입니다. 실제 결함을 하나 고친 것은 맞지만 점의 원인은 아니었습니다.

### 전체 프레임 덤프 — 필터링을 포기한 시점

크기 기준(48px 이하), 텍스처 기준(256 미만), 초소형 기준(8px 이하) 세 가지 필터가
모두 실패했습니다. 앞의 둘은 제목 텍스트와 페이드 패널에 표본 예산을 소모했고,
세 번째는 **0건**이었습니다 — 게임은 8px 이하 quad를 제출하지 않습니다.

**교훈:** 무엇이 대상을 구별하는지 모르는 상태에서 필터를 설계하면 계속 빗나갑니다.
`REPIU_GLIDE_FRAME_DUMP`로 **한 프레임의 모든 draw**를 덤프하자 즉시 잡혔습니다.

```
점    bbox=40x40   tex=0x62000 texdim=32x32 st=(0,0)~(256,256) const=0xFE6565FE  (46 draws = 23개)
화살표 bbox=165x165 tex=0x60000 texdim=64x64 st=(0,0)~(512,512) const=0x5CA20000  (8 draws = 4개)
```

전체 다운로드 로그가 마지막 조각이었습니다.

```
addr=0x00062000 thisLod=3 largeLod=3 aspect=3 → 32x32
addr=0x00060000 thisLod=2 largeLod=2 aspect=3 → 64x64
```

### 근인 1 — 텍스처 좌표 정규화 (해결)

**Glide 좌표 공간은 텍셀 단위가 아닙니다.** LOD와 무관하게 긴 축이 항상 256이고
짧은 축만 aspect가 줄입니다. 그래서 32×32 맵도 `st`가 `0..256`으로 옵니다.

픽셀 크기로 정규화하면 긴 변이 256인 맵에서는 값이 우연히 같아 정상 동작하고,
그보다 작은 맵에서만 `256/크기` 배 초과가 되어 CLAMP로 quad의 그 비율만 덮입니다.
32×32는 1/8 → 40px quad에서 5px만 보였습니다. **화면 증상과 정확히 일치합니다.**

수정: `TextureEntry`에 aspect에서 계산한 `s_extent`/`t_extent`를 저장하고 그것으로
정규화합니다. 긴 변이 256인 맵은 extent = 픽셀 크기이므로 **기존 정상 요소는 무변화**
입니다.

### 근인 2 — `GrColor_t` 형식 (해결)

`grSstWinOpen` 인자 `cformat=1 = GR_COLORFORMAT_ABGR`인데 ARGB로 읽고 있었습니다.
점의 `0xFE6565FE`는 ABGR로 빨강, ARGB로 읽으면 파랑 — 관측된 파란 점과 일치합니다.
게임 상수색이 대부분 회색(대칭)이라 그동안 드러나지 않았습니다.

수정: `ConvertGlideColorToArgb`를 HLE 공용 계층에 두고 `grConstantColorValue`와
`grBufferClear`에 적용했습니다.

### 최종 검증 (해결됨)

사용자 실행 화면에서 난이도 점이 **실제 기판과 동일하게 크고 붉은 원**으로 표시됨을
확인했습니다. 256×256 텍스처를 쓰는 요소(폰트·배경·원판)는 변화가 없었습니다.

### 잘못 내렸던 결론 (자기 정정)

**census3에서 "좌표는 텍셀 공간이므로 우리 정규화가 맞다"고 판단해 이 가설을 한 번
기각했습니다. 그 판단이 틀렸습니다.** 근거로 삼은 64×256 표본은 extent(64, 256)와
픽셀 크기(64, 256)가 **같아서 두 규칙을 구분할 수 없는 사례**였습니다. 구분 가능한
표본(32×32)이 나오고서야 갈렸습니다. 판별력 없는 표본으로 가설을 기각한 것이
이번 조사에서 가장 큰 지연 요인이었습니다.

### 미확정 / Unresolved

* 화살표(64×64에 st 0..512 = 2배 반복)가 원본에서도 반복 렌더인지는 미확정입니다.
  현재는 게임이 요청한 그대로 그립니다.
* `grTexTextureMemRequired` 수정으로 게스트 텍스처 메모리 사용이 정확해졌으나
  (8MB에 256×256 기준 64장), 장시간 실행에서 축출 동작이 나타나는지는 관측하지
  않았습니다.
* `grTexSource`가 `GrTexInfo*`를 무시하고 주소만으로 바인딩하는 것은 사실이지만
  `missing-sources=0`이므로 이번 증상과 무관합니다.

---

## English

### Summary

Item 1 is complete: `_GRHINTS@8`, the only unimplemented Glide API in the log at 298 calls, is
implemented. Item 2 has its diagnostic instrumentation in place, and the root cause awaits a run,
but the analysis already eliminated two candidate causes.

### What the log established

Converting the user's UTF-16 `repiu_log.txt` (Release loader v0.0.103, `aot-dbt`) shows one
unimplemented API, `grDrawTriangle` as the only drawing primitive at 8,706 calls with point, line,
polygon, and LFB paths never called, 42 texture downloads against 3,070 `grTexSource` calls, and a
valid `LEVEL.PTX` asset behind the `level.tga` reference in `SPR.RES`.

That eliminates two candidates. The no-op point, line, and polygon paths cannot be the cause
because the game never calls them. The repeated `chdir C:\PIU\datas\texture` failure is normal,
because that directory does not exist in the CHD's ISO9660 tree while the mount extractor copies
the tree unfiltered — recorded explicitly, since this class of misdiagnosis has happened before.

### grHints

State is recorded per hint type. Because the `GrVertex` layout is fixed by the ABI, STWHINT cannot
change the output of a renderer that reads the structure directly, so recording is a complete
implementation for this backend. `GR_HINT_FPUPRECISION` is recorded but not applied, since the
host does not execute the guest's floating point and changing the x87 control word would alter
guest results. Unknown hint types and reserved STW bits still report a gap.

### The dots

Three causes remain — the quads are never submitted, they are submitted without a bound texture,
or they draw and are covered — and static analysis cannot separate them. The second is a real
possibility in code, since `SourceTexture` clears the binding on a miss and `grTexSource` binds by
address without consulting `GrTexInfo`, but whether it happens is not in the log, and the existing
triangle census records only per-combine counts and maximum sizes. The new
`REPIU_GLIDE_DRAW_CENSUS` counts quads of 48px or less, samples forty of them with binding,
dimensions, coordinates, colors, and combine mode, and prints running totals including
`grTexSource` misses every 500 draws, with reading rules pre-registered in the design.

### Instrumented runs and the root cause

Four user-run measurements settled it. `grHints` is verified: 192 calls with the fatal line gone.
The dot hypotheses fell one at a time — 1,422 small quads with zero untextured draws and zero
`grTexSource` misses rejected both "never submitted" and "no texture bound"; the dumped 32x32
sprite decodes into a clean circle with a correct alpha mask, rejecting a decode fault; a draw
bound to a 64x256 texture spanning st (0,0) to (64,256) proved coordinates are in the texture's
own texel space, so the existing normalization is right; and no quad of 8 pixels or less is ever
submitted, so the quads are full size and the pixels disappear later.

The raw-byte dump then gave the answer. Guest memory holds a correct five-field `GrTexInfo`
(smallLod 5, largeLod 5, aspect 3, format 10) while every decoded field read zero, because
`grTexTextureMemRequired` never read the struct: the pointer served only the readability check
and a default-constructed value went into the calculation. Every texture therefore answered 8192
bytes — LOD 0 with aspect 8x1 at one byte per texel — and since the guest sizes its own TMU
address space from that answer, it packed 256x256 maps 0x2000 apart and overwrote them. The
uniform spacing observed across all four runs was our own output feeding back into guest
behavior, the exact circular trap the knowledge base warns about. The fix reads the struct.

That fix was verified as applied — 8x8 answering 128 bytes and 256x256 answering 131,072 — but
the screen was unchanged, so the overwriting hypothesis is rejected. It repaired a real defect
that was not this one.

### Giving up on filters

Three filters failed in turn: quads of 48 pixels or less and draws bound to sub-256 textures both
spent their budget on title text and a fading panel, and quads of 8 pixels or less matched
nothing at all, because the game never submits any. Designing a filter without knowing what
distinguishes the target keeps missing it. Dumping every draw of whole frames found them at once:
46 draws bind a 32x32 texture with st spanning 0 to 256 and 8 bind a 64x64 texture with st
spanning 0 to 512, matching the 23 dots and 4 arrows on screen, and the full download log showed
those textures declared at largeLod 3 and 2 with a square aspect.

### First cause: texture coordinate normalization

Glide texture coordinates are not in texel units and do not follow the LOD. The space is 256
along the longer axis for every texture, with the shorter axis scaled by the aspect ratio, so a
32x32 map is addressed 0..256. Normalizing by the pixel size is therefore correct only when the
longer edge is already 256, and shrinks anything smaller by exactly `256 / size` — an eighth for
the dots, leaving 5 pixels of a 40-pixel quad, precisely the symptom. The fix stores an
aspect-derived extent per texture and normalizes by that, which leaves every 256-edge map
unchanged.

### Second cause: GrColor_t format

`grSstWinOpen` selects `GR_COLORFORMAT_ABGR`, while the renderer read those values as ARGB. The
dots' `0xFE6565FE` is red in ABGR and blue as ARGB, which is what the screen showed. Nearly every
constant the game sets is a grey, which is symmetric and hid the fault. A shared
`ConvertGlideColorToArgb` now converts for `grConstantColorValue` and `grBufferClear`.

### Verification

The user's run shows the difficulty dots as large red circles matching the original hardware
capture, with the font, background, and disc art unchanged. Debug and Release builds,
`repiu_aot_probe` at exit 0, and `repiu_glide_issue_probe` all pass, and every diagnostic
addition is environment-gated and off by default.

### A conclusion that was wrong

The coordinate hypothesis was raised and then **wrongly rejected** on the third run, on the
strength of a 64x256 sample whose extent (64, 256) equals its pixel size (64, 256) — a case that
cannot distinguish the two rules at all. Only a sample from a map smaller than 256 could, and
when one appeared it settled the question immediately. Rejecting a hypothesis on evidence that
lacks the power to discriminate was the single largest delay in this investigation.

### Unresolved

Whether the arrow's 0..512 coordinates over a 64x64 map mean deliberate two-fold repetition on
original hardware is unconfirmed; the renderer now draws what the game asks. Guest texture memory
use is now accurate, about 64 maps of 256x256 within the 8MB limit, but eviction behavior over a
long session was not observed. `grTexSource` ignoring `GrTexInfo` remains true and remains
irrelevant here, since no source ever missed.
