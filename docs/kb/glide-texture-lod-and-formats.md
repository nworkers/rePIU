# Glide 텍스처 LOD, aspect ratio, 포맷 / Glide Texture LOD, Aspect Ratio, and Formats

Glide 2.x의 텍스처 크기 지정 규약을 정리한다. 이 규약을 잘못 읽으면 텍스처가 뒤집힌
크기로 생성되고, `grTexTextureMemRequired`를 통해 **게스트의 메모리 할당까지 오염**
되므로 정확히 다룬다. 프로젝트에서 실제로 발생한 사례는
`docs/work-logs/20260722-258-glide-lod-enumeration-fix-log.md` 참조.

## GrLOD_t — 열거값이지 log2 크기가 아니다

가장 흔한 오해다. `GrLOD_t`는 **크기가 클수록 값이 작은 열거값**이다.

| 상수 | 값 | 변 길이 |
|---|---:|---:|
| `GR_LOD_256` | 0 | 256 |
| `GR_LOD_128` | 1 | 128 |
| `GR_LOD_64` | 2 | 64 |
| `GR_LOD_32` | 3 | 32 |
| `GR_LOD_16` | 4 | 16 |
| `GR_LOD_8` | 5 | 8 |
| `GR_LOD_4` | 6 | 4 |
| `GR_LOD_2` | 7 | 2 |
| `GR_LOD_1` | 8 | 1 |

```
긴 변 = 256 >> lod          (log2 기준: 8 - lod)
```

LOD 값을 그대로 `1 << lod`의 지수로 쓰면 **크기가 정확히 반전**된다(`lod=0`이
1×1이 되고 `lod=8`이 256×256이 된다).

`GrTexInfo`의 `smallLod`/`largeLod`도 이 방향을 따른다. `largeLod`는 가장 **큰**
밉맵이므로 **숫자로는 더 작다.** 따라서 유효 불변식은 다음과 같다.

```
large_lod <= small_lod
```

밉맵 체인을 순회할 때도 `large_lod`에서 `small_lod` 방향으로 증가시킨다.

## GrAspectRatio_t — 짧은 변을 줄인다

LOD는 **긴 변**을 지정하고, aspect ratio가 **짧은 변**을 축소한다.

| 상수 | 값 | 의미 |
|---|---:|---|
| `GR_ASPECT_8x1` | 0 | 가로가 세로의 8배 |
| `GR_ASPECT_4x1` | 1 | 4배 |
| `GR_ASPECT_2x1` | 2 | 2배 |
| `GR_ASPECT_1x1` | 3 | 정사각 |
| `GR_ASPECT_1x2` | 4 | 세로가 가로의 2배 |
| `GR_ASPECT_1x4` | 5 | 4배 |
| `GR_ASPECT_1x8` | 6 | 8배 |

```
edge_log2   = 8 - lod
width_log2  = edge_log2 - max(aspect - 3, 0)
height_log2 = edge_log2 - max(3 - aspect, 0)
```

예: `lod=GR_LOD_256(0)`, `aspect=GR_ASPECT_4x1(1)` → 256×64.

## 텍스처 좌표 공간 — 텍셀 단위가 아니다

**이것이 이 문서에서 가장 오해하기 쉬운 항목이다.** `GrVertex.tmuvtx[].sow/tow`의
좌표는 텍스처의 픽셀 크기와 **무관하다.** 좌표 공간은 **긴 축이 항상 256**이고,
짧은 축만 aspect ratio가 줄인 만큼 작아진다. LOD는 좌표 공간에 영향을 주지 않는다.

```
s_extent = 256 >> max(aspect - 3, 0)
t_extent = 256 >> max(3 - aspect, 0)
```

| 텍스처 | LOD | aspect | 픽셀 크기 | 좌표 extent |
|---|---:|---:|---|---|
| 정사각 큰 맵 | `GR_LOD_256` | `1x1` | 256×256 | 256 × 256 |
| 정사각 작은 맵 | `GR_LOD_32` | `1x1` | **32×32** | **256 × 256** |
| 가로로 긴 맵 | `GR_LOD_256` | `4x1` | 256×64 | 256 × 64 |
| 세로로 긴 맵 | `GR_LOD_256` | `1x4` | 64×256 | 64 × 256 |

즉 32×32 맵도 `s`, `t`가 `0..256`으로 온다. 정규화를 **픽셀 크기로 하면** 긴 변이
256인 맵에서는 우연히 값이 같아 정상 동작하고, 그보다 작은 맵에서만 `256/크기` 배
만큼 좌표가 커져 스프라이트가 그 비율로 축소된다(Task 332에서 32×32 스프라이트가
1/8로 그려진 실제 사례).

**진단 함정:** 긴 변이 256인 맵(예: 64×256)은 extent와 픽셀 크기가 같으므로 두 규칙을
**구분하지 못한다.** 규칙을 검증하려면 긴 변이 256보다 작은 맵의 표본이 필요하다.

## GrColor_t — 형식은 grSstWinOpen이 정한다

`grConstantColorValue`, `grBufferClear`, `grFogColorValue`, `grChromakeyValue`가 받는
`GrColor_t`의 바이트 배치는 `grSstWinOpen`의 `GrColorFormat_t` 인자가 결정한다.

| 값 | 형식 | 바이트 (MSB→LSB) |
|---:|---|---|
| 0 | `GR_COLORFORMAT_ARGB` | A R G B |
| 1 | `GR_COLORFORMAT_ABGR` | A B G R |
| 2 | `GR_COLORFORMAT_RGBA` | R G B A |
| 3 | `GR_COLORFORMAT_BGRA` | B G R A |

PIU는 **`ABGR`(1)** 을 선택한다. 변환 없이 ARGB로 읽으면 빨강과 파랑이 뒤바뀐다.
회색·흰색 상수는 대칭이라 증상이 드러나지 않으므로, **무채색만 관측하고 "색은
정상"이라고 판단하면 안 된다.** 텍스처 포맷(`GrTextureFormat_t`)과는 별개다.

## LFB 565 — 같은 565라도 cFormat에 따라 RGB/BGR이 바뀐다

`GR_LFBWRITEMODE_565`는 언제나 고정된 RGB565가 아닙니다. Glide 2.4 Programming
Guide Table 11.2에 따르면 `grSstWinOpen`의 `cFormat`이 ARGB/RGBA이면 상위 5비트가
Red이고, ABGR/BGRA이면 상위 5비트가 Blue입니다.

| color format | bits 15..11 | bits 10..5 | bits 4..0 |
|---|---|---|---|
| ARGB/RGBA | Red | Green | Blue |
| ABGR/BGRA | Blue | Green | Red |

따라서 PIU의 `ABGR(1)` write lock은 **BGR565**입니다. 이를 texture의
`GR_TEXFMT_RGB_565`와 같은 방식으로 디코드하면 Blue와 Red가 교환되어 청록색이
노란색으로 보입니다.

`grLfbWriteRegion`의 source image도 같은 규칙을 따릅니다. 아래 `GrLfbSrcFmt_t` 항목을
보십시오.

## GrLfbSrcFmt_t — GrLfbWriteMode_t와 값이 다르다

`grLfbWriteRegion`의 `src_format`은 `GrLfbSrcFmt_t`이고, lock의
`GrLfbWriteMode_t`와는 **다른 열거형**입니다. 두 열거형의 앞부분 값이 우연히
겹치므로 같은 것으로 취급하기 쉽지만, 픽셀당 바이트 수와 깊이 계열의 존재가 다릅니다.

| `GR_LFB_SRC_FMT_*` | 값 | 픽셀당 바이트 | 배치 |
|---|---|---|---|
| `565` | `0x00` | 2 | RGB565 |
| `555` | `0x01` | 2 | RGB555, 최상위 비트 무시 |
| `1555` | `0x02` | 2 | ARGB1555 |
| `888` | `0x04` | 4 | 32비트 정렬 `0RGB` |
| `8888` | `0x05` | 4 | ARGB8888 |
| `565_DEPTH` | `0x0C` | 4 | 색 16비트 + 깊이 16비트 |
| `555_DEPTH` | `0x0D` | 4 | 같음 |
| `1555_DEPTH` | `0x0E` | 4 | 같음 |
| `ZA16` | `0x0F` | 2 | 깊이 전용 |
| `RLE16` | `0x80` | 가변 | 런렝스 압축 |

**`565`는 0이지 1이 아닙니다.** 1로 잘못 정의하면 실제로는 555를 뜻하게 되어 진짜
565와 8888 요청이 모두 거부됩니다(Task 476에서 실제로 발생).

**`GrLfbSrcFmt_t`는 픽셀의 크기와 비트 폭만 정하고 채널 순서는 정하지 않습니다.**
채널 순서는 `grLfbWriteColorFormat`이 정하며, 호출하지 않았다면 `grSstWinOpen`의
`cFormat`이 기본값입니다. 즉 `GrColor_t`와 같은 규칙입니다. 따라서 `ABGR`을 선언한
게스트가 보내는 8888 word는 `A<<24|B<<16|G<<8|R`이고 메모리 배치는 `R,G,B,A`입니다.
이를 `ARGB`로 읽으면(`B,G,R,A`) 화면에서 Red와 Blue가 교환됩니다 — Task 476에서 실제로
관측된 증상입니다.

`GR_LFB_SRC_FMT_565`를 "명시적 RGB565"로 읽는 해석은 **반증되었습니다**. 변환은
"source 색 형식으로 풀고 목적지 색 형식으로 싼다"이며, 두 형식이 같으면 무변환
통과입니다.

`grLfbReadRegion`에는 포맷 인자가 없습니다. 프레임 버퍼의 native 형식, 즉 색 버퍼면
버퍼 cFormat 순서의 565를 그대로 돌려줍니다.

**region 전송의 `y`는 origin 상대가 아닙니다.** `grLfbLock`은 `GrOriginLocation_t`를
명시적으로 받지만 `grLfbWriteRegion`/`grLfbReadRegion`은 받지 않습니다. 인자가 없는
이유는 프레임 버퍼를 native 배치(행 0 = 화면 위)로 주소지정하기 때문입니다.
`GR_ORIGIN_LOWER_LEFT` 창에서 lock처럼 행을 뒤집으면 화면이 상하 반전됩니다.

## grTexTextureMemRequired — 게스트가 이 답으로 할당한다

이 함수는 단순 질의가 아니다. 게임은 반환값으로 **자기 TMU 주소 공간을 배치**하므로,
틀린 값을 돌려주면 게스트가 텍스처를 잘못된 간격으로 쌓는다. 그 결과로 관측되는
"텍스처 주소 간격"은 **우리 출력의 함수**이지 원본 게임의 성질이 아니다 — 이를 외부
사실로 취급하면 순환 논리에 빠진다.

필요 바이트는 요청된 밉맵 범위와 even/odd 마스크에 해당하는 LOD들의 합이며, 텍스처
시작 주소는 8바이트 정렬을 따른다.

## GrTextureFormat_t 요약

| 값 | 포맷 | 텍셀당 바이트 |
|---:|---|---:|
| 0 | `GR_TEXFMT_RGB_332` | 1 |
| 1 | `GR_TEXFMT_YIQ_422` | 1 |
| 2 | `GR_TEXFMT_ALPHA_8` | 1 |
| 3 | `GR_TEXFMT_INTENSITY_8` | 1 |
| 4 | `GR_TEXFMT_ALPHA_INTENSITY_44` | 1 |
| 5 | `GR_TEXFMT_P_8` (팔레트) | 1 |
| 8 | `GR_TEXFMT_ARGB_8332` | 2 |
| 9 | `GR_TEXFMT_AYIQ_8422` | 2 |
| 10 | `GR_TEXFMT_RGB_565` | 2 |
| 11 | `GR_TEXFMT_ARGB_1555` | 2 |
| 12 | `GR_TEXFMT_ARGB_4444` | 2 |
| 13 | `GR_TEXFMT_ALPHA_INTENSITY_88` | 2 |
| 14 | `GR_TEXFMT_AP_88` (팔레트+알파) | 2 |

값 0~5는 1바이트, 8 이상은 2바이트다. 팔레트 포맷(`P_8`, `AP_88`)은
`grTexDownloadTable`로 내려온 팔레트가 있어야 색을 복원할 수 있다.

### 표준 palette의 상위 바이트는 alpha가 아니다

표준 `GuTexPalette` entry는 32비트 `0xAARRGGBB` 모양으로 저장되지만 실제 palette는
8-bit RGB이며 상위 8비트는 무시됩니다. 따라서 P_8을 RGBA로 확장할 때 alpha는 항상
255여야 합니다. AP_88은 palette에서 RGB만 가져오고 texel의 상위 8비트에서 alpha를
가져옵니다. 상위 palette 바이트를 alpha로 사용하면 예약/미정 값이 혼합에 들어가 색과
투명도가 손상됩니다.

근거: [3Dfx Glide 2.4 Reference Manual](https://bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf),
[3Dfx Glide 3.0 Programming Guide](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_3.0_199806.pdf).

### The standard palette high byte is not alpha

A standard `GuTexPalette` entry has a 32-bit `0xAARRGGBB`-shaped storage word,
but the palette contains 8-bit RGB and ignores the high byte. P_8 must therefore
expand with alpha 255. AP_88 takes RGB from the palette and alpha from the
texel's high byte. Treating the ignored palette byte as alpha feeds reserved or
undefined values into blending and corrupts colour and transparency.

Sources: [3Dfx Glide 2.4 Reference Manual](https://bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf),
[3Dfx Glide 3.0 Programming Guide](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_3.0_199806.pdf).

### texture와 palette의 수명은 분리된다

`grTexDownloadMipMapLevel`은 P_8/AP_88 texel의 palette index를 texture memory에
저장하고, `grTexDownloadTable`은 별도의 TMU palette를 바꿉니다. 따라서 texture를 먼저
다운로드하고 palette를 나중에 바꾸는 순서도 유효하며, palette 변경은 기존 indexed
texture에 적용돼야 합니다. HLE가 texture download 시점에 RGBA로 미리 확장한다면 원본
index texel을 보존하고 palette 변경 때 다시 확장해야 동일한 의미를 유지할 수 있습니다.

### Texture and palette lifetimes are separate

`grTexDownloadMipMapLevel` stores P_8/AP_88 palette indices in texture memory,
while `grTexDownloadTable` changes a separate TMU palette. Downloading texture
first and changing the palette later is therefore valid, and the new palette
must affect existing indexed textures. An HLE that expands to RGBA at texture
download time must retain the original indices and expand them again after a
palette change.

CPU에서 미리 확장하는 backend는 palette 변경 시 모든 보존 texture를 즉시 확장할
필요는 없습니다. 각 texture가 적용한 palette generation을 기록하고 실제 sampling 전에
오래된 texture만 갱신하면 observable Glide 의미는 같습니다. 같은 palette byte를 다시
내려보낸 경우 generation을 유지할 수 있습니다. GPU allocation 크기가 같다면
`glTexSubImage2D`처럼 내용을 갱신하는 연산을 사용해 재할당도 피할 수 있습니다.

A CPU-expanding backend need not eagerly expand every retained texture when the
palette changes. Recording the applied palette generation per texture and refreshing
only a stale texture before sampling preserves the same observable Glide semantics.
Re-downloading byte-identical palette data can retain the generation. When the GPU
allocation dimensions are unchanged, a content update such as `glTexSubImage2D` also
avoids reallocating storage.

## 채널 확장 주의

5·6비트 채널을 8비트로 늘릴 때 단순 시프트(`v << 3`)를 쓰면 최댓값이 255가 아니라
248이 되어 흰색이 어두워진다. 상위 비트를 하위로 복제하거나 반올림 스케일을 쓴다.

```
expand5(v) = (v << 3) | (v >> 2)
expand6(v) = (v << 2) | (v >> 4)
```

## `sow/tow/oow` 원근 보정

Glide의 `sow`와 `tow`는 이미 나눗셈이 끝난 일반 UV가 아니라 각각 `s/w`, `t/w`인
분자입니다. rasterizer는 정점 사이에서 이 두 값과 `oow = 1/w`를 보간하고 픽셀마다
`s = sow/oow`, `t = tow/oow`를 복원합니다. 따라서 화면 공간 삼각형을 직교 투영하는
host backend라도 `sow/tow`를 정점에서 미리 나누거나 `oow`를 버리면 perspective가
사라집니다.

공식 구조는 texture mapping 종류에 따라 공용 또는 TMU별 reciprocal-w를 기술할 수
있지만, PIU의 현재 확인된 60-byte producer layout에서는 dword 8이 공용 `oow`, dword
9/10이 TMU0 `sow/tow`이고 dword 11..14는 가변·미확정입니다. 관측된 non-projected
경로는 공용 `oow`를 texture와 table fog에 함께 사용합니다. 미확정 필드는 실제
projected-texture producer 증거가 나오기 전까지 per-TMU `oow`로 간주하지 않습니다.

## Perspective correction with `sow/tow/oow`

Glide `sow` and `tow` are the numerators `s/w` and `t/w`, not final UVs. The
rasterizer interpolates them together with `oow = 1/w` and reconstructs
`s = sow/oow` and `t = tow/oow` per pixel. A host backend using an orthographic
screen-space projection must still retain that divide; discarding `oow` removes
perspective correction.

The official layout can describe shared or per-TMU reciprocal-w depending on
the texture path. In PIU's currently confirmed 60-byte producer, dword 8 is
shared `oow`, dwords 9/10 are TMU0 `sow/tow`, and dwords 11--14 are variable and
unconfirmed. The observed non-projected path shares dword 8 between texture
correction and table fog. Unconfirmed fields must not be labeled per-TMU `oow`
until a projected-texture producer trace establishes that contract.

## 외부 근거 / References

* [3Dfx Glide 2.4 Reference Manual](https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf) — `GrLOD_t`, `GrAspectRatio_t`, `GrTextureFormat_t`, `grTexTextureMemRequired`, `grTexDownloadMipMapLevel`
* [3Dfx Glide 2.4 Programming Guide](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_2.4_199707.pdf) — 텍스처 메모리 관리와 밉맵 배치
* [3Dfx Glide 2.0 API Reference](https://www.gamers.org/dEngine/xf3D/glide/glideref.htm)

---

## English Summary

Glide 2.x sizes textures through `GrLOD_t`, an enumeration in which
`GR_LOD_256` is 0 and `GR_LOD_1` is 8 — **not** a log2 size. A LOD's longer edge
is `256 >> lod`, so using the LOD value directly as an exponent inverts every
texture. Because `largeLod` names the biggest mipmap, it is numerically smaller
than `smallLod`, making `large_lod <= small_lod` the valid invariant.

`GrAspectRatio_t` shrinks the shorter edge: `width_log2 = edge_log2 -
max(aspect - 3, 0)` and `height_log2 = edge_log2 - max(3 - aspect, 0)`.

Texture coordinates are **not** in texel units and do not follow the LOD at all.
The space is 256 along the longer axis for every texture, with the shorter axis
scaled by the aspect ratio, so `s_extent = 256 >> max(aspect - 3, 0)` and
`t_extent = 256 >> max(3 - aspect, 0)`. A 32x32 map is therefore addressed with
s and t running 0..256, and normalizing by the pixel size shrinks any map whose
longer edge is under 256 by exactly `256 / size` — the defect behind Task 332's
eighth-size sprites. Note that a map whose longer edge is already 256, such as
64x256, has an extent equal to its size and so cannot distinguish the two rules;
verifying the convention requires a sample from a smaller map.

`GrColor_t` byte order is chosen by the `GrColorFormat_t` argument to
`grSstWinOpen`: 0 is ARGB, 1 ABGR, 2 RGBA, 3 BGRA. PIU selects ABGR, so reading
those values as ARGB swaps red and blue. Grey and white constants are symmetric
and hide the fault, so observing only achromatic values proves nothing about it.
This is independent of `GrTextureFormat_t`.

LFB 565 packing is also controlled by the color format. ARGB/RGBA places Red
in bits 15..11 and Blue in bits 4..0, while ABGR/BGRA places Blue high and Red
low. PIU's ABGR write locks are therefore BGR565; treating them like
`GR_TEXFMT_RGB_565` swaps blue and red and turns cyan into yellow.

`GrLfbSrcFmt_t` is a different enumeration from `GrLfbWriteMode_t` even where
their low values coincide: `565` is `0x00`, `555` `0x01`, `1555` `0x02`, `888`
`0x04` (four bytes, 32-bit aligned `0RGB`), `8888` `0x05`, the depth-carrying
formats `0x0C`-`0x0E`, `ZA16` `0x0F`, and `RLE16` `0x80`. Defining `565` as `1`
actually names 555 and rejects both genuine 565 and 8888, which is what Task 476
found in this repository.

`GrLfbSrcFmt_t` fixes only the pixel's size and bit widths — **the channel order
comes from `grLfbWriteColorFormat`**, defaulting to the window `cFormat`, the
same rule `GrColor_t` follows. A guest that declared ABGR sends 8888 words as
`A<<24|B<<16|G<<8|R`, whose bytes are `R,G,B,A`; reading them as ARGB
(`B,G,R,A`) swaps red and blue on screen, as Task 476 observed. Reading
`GR_LFB_SRC_FMT_565` as an explicitly RGB-ordered image is **refuted**:
conversion unpacks in the source format and packs in the destination format,
passing through unchanged when the two agree. `grLfbReadRegion` takes no format
argument at all: it returns the frame buffer's native form, meaning 565 in the
buffer's cFormat order for a color buffer.

Region `y` is not origin-relative. `grLfbLock` takes an explicit
`GrOriginLocation_t` while `grLfbWriteRegion` and `grLfbReadRegion` take none,
because they address the frame buffer in its native layout with row 0 at the
top. Mirroring rows the way a lock would renders a `GR_ORIGIN_LOWER_LEFT`
window's LFB output upside down.

`grTexTextureMemRequired` is not a passive query — the guest lays out its own TMU
address space from the answer, so a wrong result propagates into guest behavior
and any "observed" texture spacing becomes a function of our own output rather
than evidence about the original game.

Texture formats 0-5 are one byte per texel and 8-14 are two; palette formats need
the table uploaded by `grTexDownloadTable`. Expand 5- and 6-bit channels by bit
replication so the maximum encoded value maps to 255 rather than 248.
