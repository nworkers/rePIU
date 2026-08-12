# Glide LFB region 전송 작업 로그

관련 문서: [설계](../design/20260813-476-glide-lfb-region-transfer.md),
[작업 지시](../work-orders/20260813-476-glide-lfb-region-transfer.md)

## 시작점

사용자가 제공한 `pumpit8` 실행 로그에서 `GLIDE_UNIMPLEMENTED_FUNCTION`을 확인했습니다.
전체 로그에 남은 구현 공백은 두 종류이며 둘 다 LFB region 전송이었습니다.

| 종류 | ordinal | 이름 | reason | 호출 횟수 |
|---|---|---|---|---|
| `GLIDE_UNIMPLEMENTED_FUNCTION` | 98 | `_GRLFBREADREGION@28` | `lfb-read-region-noop` | 1440 |
| `GLIDE_UNSUPPORTED_ARGUMENT` | 99 | `_GRLFBWRITEREGION@32` | `lfb-write-region-unsupported` | 1440 |

인자를 보면 두 호출이 한 쌍으로 움직입니다. `y = 0x1DF`(479)에서 1씩 내려가고,
`width = 640`, `height = 1`, `stride = 0`, buffer = BACK. `1440 = 3 x 480`이므로
**전면 화면 read-modify-write 합성 3회**입니다. 읽기가 no-op이라 게스트는 자기 버퍼의
이전 내용을 계산에 쓰고 있었으며 되쓰기는 통째로 버려지고 있었습니다.

> **처음에 잘못 읽었던 것.** `repiu-fatal` 기록 건수가 정확히 128이라 "하단 128행"으로
> 정리했으나, 128은 `kGlideImplementationIssueRecordCapacity`입니다. 서로 다른 인자
> 조합이 테이블 용량에서 잘린 것을 행 수로 오해했습니다. 호출 횟수는 Glide call
> trace의 `count`에 있고 그 값은 1440입니다.

## 확인한 것

1. **`kGlideLfbSrcFmt565 = 1`은 사양 위반이었습니다.** Glide 2.4에서 `565`는 `0x00`
   이고 `0x01`은 555입니다. 게임이 쓰는 `5`(`GR_LFB_SRC_FMT_8888`)뿐 아니라 **진짜
   565 요청도 거부**되고 있었습니다. 이 상수는 work order 002에서 검증 없이 도입된
   값이었습니다.
2. **`stride = 0`이 "빈 영역"으로 해석되고 있었습니다.** 원본 크기를
   `height * |stride|`로 구했기 때문입니다. `height == 1`에서 stride는 쓰이지 않는
   자리이며 `width * bpp`가 유일한 해석입니다.
3. **포맷만 고쳤다면 화면이 파괴되었을 것입니다.** 기존 write 경로는 region을 staging
   surface에 쓴 뒤 **surface 전체**를 present하는데, 그 surface는 프레임 버퍼로
   seed되지 않았습니다. 한 행만 새 값인 640x480 이미지를 프레임당 128회 덮어쓰는
   셈입니다. `grLfbLock` 경로는 이 문제를 이미 인지해 매 lock마다 seed하고 있었으나
   region 경로에는 같은 보정이 없었습니다. 이 결함은 로그로는 보이지 않았고 코드를
   읽어야만 드러났습니다.

## 한 일

**LFB region shadow.** 두 region gate가 기존 `glide_lfb_surface`를 프레임 버퍼의
shadow로 공유합니다. `valid`/`dirty` 두 플래그와 ensure/flush/invalidate 세 연산이
전부이며, region이 아닌 gate 진입 전에 flush 후 invalidate합니다. 결과적으로 관측된
128쌍 연속 호출이 **burst당 readback 1회 + present 1회**로 끝납니다. 행마다 처리하면
프레임당 640x480 왕복 128회이므로 정확성 이전에 실행이 불가능합니다.

**모듈 분리.** 포맷 표, stride 규칙, 클립, 픽셀 변환을 `glide_lfb_region.{h,cpp}`로
분리했습니다. boundary에는 ABI 해석, guest 범위 검사, shadow 수명만 남았습니다.
`WriteRegionToGlideLfb565`는 `WriteGlideLfbRegion`이 상위 집합이므로 제거했습니다.
`Expand5`/`Expand6`/`UsesBgrColorOrder`는 두 번째 사용처가 생겼으므로 anonymous
namespace에서 `glide_lfb.h`의 공용 inline 함수로 올렸습니다.

**구현 범위.** 색 포맷 565/555/1555/888/8888과 stride 0을 지원합니다. 깊이 계열,
`RLE16`, 음수 stride, `AUX`/`DEPTH` 버퍼, lock이 걸려 있는 동안의 region 요청은
`GLIDE_UNSUPPORTED_ARGUMENT`로 기록하고 거부합니다. 반환값은 두 gate 모두 기존 정책인
`FXTRUE`를 유지했습니다 — work order 002가 LFB 계열의 `FXFALSE` 반환이 guest를 멈추게
하는 것을 관측했기 때문입니다.

## 검증

**`glide_lfb_region_probe` 7개 항목 전부 통과.** `repiu_aot_probe`가 exit 0으로
끝나므로 기존 probe 회귀도 없습니다.

```
glide_lfb_region_format_table=true
glide_lfb_region_stride_rule=true
glide_lfb_region_conversion=true
glide_lfb_region_color_order=true
glide_lfb_region_clipping=true
glide_lfb_region_round_trip=true
glide_lfb_region_row_mapping=true
glide_lfb_region_all=true
```

Win32 x86 Debug 전체 빌드 성공. 새 경고는 없고 기존 C4819(코드 페이지) 경고만
남았습니다.

## 사용자 구동 결과 — 설계 가정 2건 반증

사용자가 구동한 결과 **화면이 나오기 시작했습니다.** region 경로가 실제로 동작한다는
뜻이며, 동시에 처음 세운 가정 두 개가 틀렸음이 드러났습니다. 증상은 **전면 화면 상하
반전 + red/blue 교환**이었습니다.

같은 로그에서 근거가 되는 상태값을 확인했습니다.

| 호출 | 인자 | 값 |
|---|---|---|
| `grSstWinOpen` | `cFormat` | 1 = ABGR |
| `grSstWinOpen` | `origin` | 1 = `GR_ORIGIN_LOWER_LEFT` |
| `grLfbWriteColorFormat` | `format` | 1 = ABGR |

**반증 1 — region 좌표는 origin 상대가 아닙니다.** lock과 같이 `LOWER_LEFT`에서 행을
`height - 1 - y`로 뒤집었더니 화면이 상하 반전됐습니다. `grLfbLock`은
`GrOriginLocation_t`를 명시적으로 받지만 region 전송은 받지 않는데, 이는 인자를 생략한
것이 아니라 프레임 버퍼를 **native 배치(행 0 = 화면 위)** 로 주소지정하기 때문입니다.
행 반전을 제거했습니다.

**반증 2 — source word의 채널 순서는 `grLfbWriteColorFormat`을 따릅니다.** design
360의 "`GR_LFB_SRC_FMT_565` source는 명시적 RGB565" 서술을 따라 8888을 `B,G,R,A`(ARGB
word)로 읽었더니 red와 blue가 교환됐습니다. `GrLfbSrcFmt_t`는 픽셀 **크기**만 정하고
채널 순서는 색 형식이 정합니다 — `GrColor_t`와 같은 규칙이고, `grLfbWriteColorFormat`이
존재하는 이유입니다. 변환을 "source 색 형식으로 풀고 목적지 색 형식으로 싼다"로
바꿨습니다. PIU는 양쪽 다 ABGR이므로 무변환 통과가 됩니다.

design 360의 그 서술은 당시 검증이 **불가능한** 추정이었습니다. `grLfbWriteRegion`은
이번 작업 전까지 한 번도 성공한 적이 없기 때문입니다. lock write mode에 대한 design
360의 결론 자체는 그대로 유효합니다. `docs/kb/`와 `docs/analysis/`를 함께 정정했습니다.

수정 후 probe 7개 항목(색 순서·행 대응 항목 추가) 전부 통과, 전체 빌드 성공.

**미수행:** 수정본의 실물 구동 확인은 사용자 몫으로 남습니다.

## 남은 것

* 이 전면 화면 합성이 무엇을 그리는지는 확인됐습니다 — 사용자 화면의 BGA 정지화면
  입니다. read 결과를 어떻게 쓰는지(알파 합성인지 단순 덮어쓰기인지)는 미확인입니다.
* burst 사이에 region이 아닌 gate가 끼면 그만큼 readback/present가 늘어납니다. 실제
  호출 순서는 아직 관측하지 않았으므로, `glide_lfb_region_seed_count`와
  `glide_lfb_region_flush_count`를 구동 로그에서 확인하면 됩니다.
* 이번 작업은 로그에 남은 Glide 구현 공백만 다뤘습니다. 같은 로그의 종료 원인인
  `0x040E5D0D` 접근 위반은 Task 475 범위이며 이 작업과 무관합니다.

# Glide LFB Region Transfer Work Log

## Starting point

The `GLIDE_UNIMPLEMENTED_FUNCTION` entries the user asked about turned out to be one
of only two implementation gaps in the whole `pumpit8` log, and both were LFB region
transfers: `grLfbReadRegion` (ordinal 98) as a no-op and `grLfbWriteRegion` (ordinal
99) as an unsupported argument, **1440 calls each**. Their arguments move as a pair —
`y` walking down from 479 over `640x1` rows with `stride = 0` on the back buffer —
and `1440 = 3 x 480` makes it three full-screen read-modify-write composites, whose
read half returned stale guest memory and whose write half was discarded. (The
`repiu-fatal` record truncated distinct argument sets at 128, which is
`kGlideImplementationIssueRecordCapacity`; an early note misread that as a row count.
Call counts come from the Glide call trace.)

## What the investigation confirmed

`kGlideLfbSrcFmt565 = 1` did not match the specification, where `565` is `0x00` and
`0x01` is 555, so the test rejected both the game's `5` (8888) and genuine 565; the
constant had been introduced without verification in work order 002. A zero stride
was read as an empty region because the source size came from `height * |stride|`,
although a single-row transfer never consults a stride. Most importantly, fixing only
the format would have destroyed the picture: the write path presented the *entire*
staging surface while never seeding it from the frame buffer, so each of the 128
per-row writes would have blitted a mostly-stale 640x480 image over the screen. The
`grLfbLock` path already carried that correction; the region path did not. This
defect was invisible in the log and only showed up on reading the code.

## What was done

Both region gates now share the staging surface as a frame buffer shadow with
`valid`/`dirty` flags and ensure/flush/invalidate operations, flushed and invalidated
before every non-region gate, so a burst costs one readback and one present instead
of 128 of each. The format table, stride rule, clipping, and pixel conversion moved
into a dedicated `glide_lfb_region` module, leaving ABI decoding, guest-range checks,
and shadow lifetime in the boundary; `WriteRegionToGlideLfb565` was superseded and
removed, and the channel-expansion helpers were promoted from an anonymous namespace
into shared inline functions now that they have a second caller. Color source formats
565/555/1555/888/8888 and a zero stride are supported; depth formats, `RLE16`,
negative strides, the `AUX`/`DEPTH` buffers, and region requests made while a lock is
outstanding are declined and recorded. Both gates keep returning `FXTRUE`, because
work order 002 observed that an `FXFALSE` from the LFB family stalls the guest.

## What the user's run refuted

The screen started rendering, which proves the region path works — and showed two
initial assumptions to be wrong at once: the full-screen composite came out **upside
down with red and blue swapped**. The same log pins `grSstWinOpen` to `cFormat = 1`
(ABGR) and `origin = 1` (`GR_ORIGIN_LOWER_LEFT`), plus `grLfbWriteColorFormat(1)`.

**Region coordinates are not origin-relative.** Mirroring rows to
`height - 1 - y` for the lower-left window inverted the screen. `grLfbLock` takes an
explicit `GrOriginLocation_t` and the region entry points take none — not an omission,
but because they address the frame buffer in its native layout with row 0 at the top.
The flip is gone.

**The source word's channel order follows `grLfbWriteColorFormat`.** Following design
360's inference that a `GR_LFB_SRC_FMT_565` source is explicitly RGB-ordered, 8888 was
read as `B,G,R,A`, and red and blue swapped on screen. `GrLfbSrcFmt_t` fixes the pixel
*size*; the color format fixes the channel order, the same rule `GrColor_t` obeys and
the reason `grLfbWriteColorFormat` exists. Conversion now unpacks in the source format
and packs in the destination one, a pass-through for PIU where both are ABGR. Design
360 could not have tested that inference — `grLfbWriteRegion` never once succeeded
before this task — and its conclusions about lock write modes still stand. The kb and
analysis topics were corrected alongside.

## Verification

All seven `glide_lfb_region_probe` checks pass — color order and row mapping were
added after the refutations — and `repiu_aot_probe` exits 0, so no existing probe
regressed. The full Win32 x86 Debug build succeeds with no new warnings. Confirming
the corrected build on screen is left to the user's next run.

## Remaining

What the pass draws is now known — it is the BGA still image on the user's screen —
but how the read result is consumed (alpha composite or plain overwrite) is not. Any
non-region gate
interleaved into a burst adds a readback and a present; the actual call order has not
been observed, and `glide_lfb_region_seed_count` and `glide_lfb_region_flush_count`
answer it from a run log. This task covered only the Glide gaps in that log; the
`0x040E5D0D` access violation that ended the run belongs to Task 475 and is unrelated.
