# 20260730-360 Glide LFB 565 색 채널 순서 작업 로그 / Work Log

* 설계 / Design:
  [20260730-360-glide-lfb-565-color-order.md](../design/20260730-360-glide-lfb-565-color-order.md)
* 작업 지시 / Work order:
  [20260730-360-glide-lfb-565-color-order.md](../work-orders/20260730-360-glide-lfb-565-color-order.md)

## 한국어

### 결과

일반 Glide 텍스처나 shader/material/light를 변경하지 않고 `grLfbLock`의 565 채널
순서만 보정했습니다.

* `DecodeGlideLfb565ToRgba8`와 `EncodeRgba8ToGlideLfb565`가
  `GrColorFormat_t`를 받습니다.
* ARGB/RGBA는 RGB565, ABGR/BGRA는 BGR565로 변환합니다.
* `grSstWinOpen` 시 LFB write color format을 window color format으로
  초기화하고, 이후 `grLfbWriteColorFormat` 상태를 write lock에 사용합니다.
* framebuffer→staging seed와 write unlock→RGBA presentation이 같은 형식을
  사용하므로 부분 기록 전후에도 Red/Blue가 왕복 보존됩니다.
* `grLfbWriteRegion`의 명시적 RGB565 source 경로에는 RGB 순서를 전달했습니다.
* render probe에 RGB/BGR pure red/blue와 ABGR Cyan `0xFFE0` 왕복 검사를
  추가했습니다.

### 원인 증거

PIU는 `grSstWinOpen`과 `grLfbWriteColorFormat`에 모두
`GR_COLORFORMAT_ABGR(1)`을 전달하고, 실제 문제 장면은
`GR_LFBWRITEMODE_565` write-only lock으로 기록합니다. Glide 2.4 Programming
Guide Table 11.2에서 이 조합은 상위 5비트가 Blue이고 하위 5비트가 Red인
BGR565입니다. 기존 구현은 이를 항상 RGB565로 읽어 Blue를 Red로 바꿨습니다.

### 검증

다음 빌드와 probe가 통과했습니다.

```text
cmake --build build\win32_x86_debug --config Debug \
  --target repiu_loader_win32 repiu_glide_render_probe
repiu_glide_render_probe=pass
repiu_glide_render_probe --opengl-lfb=pass
repiu_glide_issue_probe=pass
```

빌드에는 기존 C4819 코드 페이지 경고만 있었고 신규 컴파일/링크 오류는 없습니다.

`REPIU_EXECUTION_BACKEND=aot-dbt`, LFB dump 활성화 상태로 `pumpit1`을 35초
supervisor 구동했습니다. `grLfbWriteColorFormat(1)` 뒤 write-only 565
lock/unlock이 반복됐고 fatal 또는 Glide backend failure 없이 supervisor 제한으로
종료됐습니다.

수정 후 LFB dump 245의 전체 307,200픽셀 통계:

| 항목 | 값 |
|---|---:|
| 평균 R/G/B | `11.45 / 74.36 / 82.47` |
| Blue/Cyan 우세 픽셀 | `115,200` |
| Yellow 우세 픽셀 | `0` |
| 대표색 | `(33,251,255)` |

실행 로그와 검증 이미지는 `build/task360-runtime/` 및
`build/texture_dumps/`에 남겼습니다. 실행 종료 중 작성되던 마지막 dump 303은
불완전하여 통계에서 제외했습니다.

---

## English

### Result

The defect was fixed only in the 565 `grLfbLock` conversion path; regular Glide
textures, shaders, materials, and lighting were not changed.

The platform-neutral encode/decode helpers now accept `GrColorFormat_t`, use
RGB565 for ARGB/RGBA and BGR565 for ABGR/BGRA, and preserve the same effective
format through framebuffer seeding and write-unlock presentation.
`grSstWinOpen` initializes the LFB write format and
`grLfbWriteColorFormat` can override it. The explicit RGB565 source contract
of `grLfbWriteRegion` remains RGB ordered. The render probe covers pure
red/blue and the ABGR cyan value `0xFFE0`.

### Evidence and verification

PIU selects ABGR for both the window and LFB write state and writes the
affected scene through write-only 565 locks. Glide 2.4 Table 11.2 defines this
as BGR565, while the old HLE always treated it as RGB565.

The Win32 x86 loader/render-probe build completed with only pre-existing C4819
warnings. The render probe, its OpenGL LFB mode, and the existing issue probe
all passed. A 35-second `aot-dbt` game smoke repeated format-1 565 locks and
unlocks without a fatal or Glide backend failure.

Corrected LFB dump 245 has mean RGB `11.45/74.36/82.47`, 115,200
blue/cyan-dominant pixels, zero yellow-dominant pixels, and dominant color
`(33,251,255)`. Runtime evidence is under `build/task360-runtime/` and
`build/texture_dumps/`; the incomplete final dump 303 was excluded.
