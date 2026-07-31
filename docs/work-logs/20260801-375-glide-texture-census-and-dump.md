# 작업 로그: Glide 텍스처 census와 픽셀 덤프 / Work log: Glide texture census and pixel dump

Task 375. 설계 [20260801-375](../design/20260801-375-glide-texture-census-and-dump.md),
작업 지시 [20260801-375](../work-orders/20260801-375-glide-texture-census-and-dump.md)

## 한국어

### 결론: 텍스처 축을 닫습니다

사용자가 music select의 텍스처를 의심해 속성과 실 데이터를 검사할 도구를
요청했습니다. 만들어 돌린 결과 **텍스처는 결백합니다.**

music select 24.1초 / 793프레임:

```
uploads/distinct/identical-repeats/changed-repeats: 41/29/0/12
decode-failures/last-failed-format/extent-mismatch/palettized-without-palette: 0/0/3/0
format 10: 5   format 12: 36
longer-edge  8: 1   32: 1   64: 1   256: 38
```

| 검사 | 결과 |
|---|---|
| 동일 내용 재업로드 | **0건** — 낭비 없음 |
| 디코드 실패 | **0건** — 화면에서 사라진 텍스처 없음 |
| 팔레트 없는 팔레트 텍스처 | **0건** |
| 8비트 포맷(0/2/3/4/5/14) | **0건** — 전부 16비트 |
| extent 불일치 | 3건 — 8·32·64 LOD, Task 332의 정상 동작 |
| 업로드 빈도 | 프레임당 **0.05회**, 0.4 MB/s |

픽셀도 육안 확인했습니다. 발판 이미지(fmt12)는 빨강·파랑 화살표와 노란 중앙이 제
색으로, 배경 43%가 투명하게 나옵니다. 하트 스프라이트(fmt10)는 빨간 하트에 초록
배경으로 알파 100% 불투명입니다. 64×256 세로 배너는 종횡비 5가 올바르게 세로로
디코드됩니다.

**중간에 세운 오진을 하나 기록해 둡니다.** 자동 장면의 fmt10 텍스처 두 장이 전부
청록색이라 RGB_565 적색 채널 손실을 의심했습니다. 세 가지로 반증했습니다 —
채널 값 분포가 22/53/26으로 5/6/5 비트와 일치하고, R/B를 바꿔 보니 노란색이 되어
그것도 부자연스러웠으며, 결정적으로 **같은 fmt10이 다른 장면에서는 빨강·초록으로
정상 출력**됐습니다. 원본 BGA 아트가 색보정된 것이었습니다.

### 중복 발견과 정리 — 사용자 지적

**이 작업의 덤프는 중복이었습니다.** 사용자가 "이전에도 여러 번 구현한 적이
있다"고 지적해 확인한 결과, `linexe_glide_boundary.cpp`에 `DumpTextureToBmp`가 이미
있었고 **같은 디렉터리 `build/texture_dumps`** 에 쓰고 있었습니다.

| | 기존 (`REPIU_DUMP_TEXTURE_BMP`) | 신규 (`REPIU_GLIDE_TEX_DUMP`) |
|---|---|---|
| 형식 | BMP **24비트** | TGA **32비트** |
| 알파 | **없음** | 있음 |
| 디코드 | **덤프 전용으로 한 번 더** | backend 결과 재사용 |
| manifest | 없음 | CSV |

기존 코드의 주석이 스스로 한계를 인정하고 있었습니다 — *"the BMP is 24-bit, so
alpha never reaches the file. A sprite that decoded into a nearly fully transparent
image looks identical to a correct one there, **which is exactly the case under
investigation**"*.

사용자 결정에 따라 **기존 BMP 텍스처 덤프를 제거**했습니다(56줄). 함께 사라진 것:

* 덤프용 **중복 디코드** — 텍스처마다 `DecodeGlideTextureToRgba8`를 두 번 돌리던 것
* `repiu-tex-alpha` 보정 로그 — BMP의 알파 손실을 메우려던 stderr 출력. TGA가 알파를
  보존하므로 불필요

`DumpTextureToBmp`는 LFB 덤프에서만 쓰이므로 **`DumpLfbSurfaceToBmp`로 개명**해
범위를 정직하게 했습니다.

### 구현

* 신규 `glide_texture_census.{h,cpp}` — 업로드/고유주소/동일내용 재업로드/변경
  재업로드, 디코드 실패와 마지막 실패 포맷, extent 불일치,
  **팔레트 없는 팔레트 텍스처**, 포맷·크기 히스토그램, 총 바이트.
* 내용 해시는 FNV-1a. "같은 바이트인가"만 판정하면 되므로 암호학적 강도가 필요
  없습니다.
* **거부 경로도 기록합니다.** 처음 구현에서는 `IsGlideTextureFormatAcceptable`
  실패와 치수 계산 실패가 census **이전에** return해 보이지 않았습니다. 이것이야말로
  "화면에서 조용히 사라지는 텍스처"라 census가 잡아야 할 대상인데 놓치고 있었습니다.
* TGA 덤프는 opt-in(`REPIU_GLIDE_TEX_DUMP`), 상한 512건
  (`REPIU_GLIDE_TEX_DUMP_LIMIT`). 무압축 32비트, **상단 원점**(Glide 좌표계와 일치 —
  뒤집힌 덤프는 있지도 않은 디코드 버그를 쫓게 만듭니다).

### 검증

* Debug/Release 빌드 성공, `repiu_aot_probe` 양 구성 **exit 0**, 신규 probe **9개 항목
  전부 true**.
* `REPIU_DUMP_TEXTURE_BMP=1`을 켜도 **BMP 0개, `repiu-tex-alpha` 0줄** — 경로가 죽은
  것을 확인.
* 깨끗한 디렉터리에서 두 지정 방식 모두 일관:

| 지정 | uploads | 파일 | manifest |
|---|---:|---:|---:|
| `=build\texdump_clean` | 28 | 28 | 28 |
| `=1` | 28 | 28 | 28 |

중간에 "dump written 0인데 파일 24개" 불일치를 보고했는데, **코드 문제가 아니라
이전 실행이 `manifest.csv`를 잡고 있어 `rm -rf`가 실패한 것**이었습니다. 깨끗한
디렉터리 재현으로 확인했습니다.

### 부수 정리: 커밋된 캡처 로그 제거

Task 374 커밋(`4760c19`)에 `git add -A`로 캡처 로그 3개(약 450 KB)가 딸려
들어간 것을 발견해 추적에서 제외하고 `.gitignore`에
`musicselect*.log`, `tex*.log`, `build/texture_dumps/`를 추가했습니다.

### 남은 것

`REPIU_DUMP_LFB_BMP`는 그대로입니다. LFB 표면 덤프라 관심사가 다르지만 **같은 24비트
알파 손실**을 안고 있으므로, 공용 TGA writer로 합칠 후보로 남깁니다.

축은 이제 이렇게 정리됩니다.

| 축 | 상태 |
|---|---|
| 텍스처 | **닫힘** (본 작업) |
| DOS I/O | 닫힘 (Task 374) |
| Glide gate 전체 | 10.65%, 실제 GL work 2.8% |
| **커널 예외 왕복** | **22.5%** |
| **VEH 핸들러 본체** | **20.3%** |
| 게스트 실행 | 약 57% |

다음은 Task 373(single-step 모집단, 13.3%)입니다.

---

## English

### Verdict: the texture axis closes

The user suspected textures behind the music select frame rate and asked for
attribute and pixel-level inspection. Built and run, the answer is that textures
are clean: 41 uploads over 24.1 seconds and 793 frames, 29 distinct addresses,
**zero identical repeats**, **zero decode failures**, **zero palettized uploads
missing a palette**, and no 8-bit formats at all — everything is RGB_565 or
ARGB_4444. Uploads run at 0.05 per frame and 0.4 MB/s. Three extent mismatches are
the 8, 32, and 64 LOD cases, which is Task 332's documented normal behaviour.

Pixels confirm it visually: the dance pad in ARGB_4444 shows correct red and blue
arrows with a yellow centre over a 43% transparent background, the RGB_565 heart
sprite is red on green at full opacity, and the 64×256 banner decodes tall as
aspect ratio 5 requires.

One misdiagnosis is recorded deliberately. Two RGB_565 textures in the automated
scene were entirely cyan, which looked like a lost red channel. Three things
disproved it: the per-channel value spread of 22/53/26 matches 5/6/5 bits, swapping
red and blue produced an equally unnatural yellow, and decisively the same format
renders correct reds and greens in another scene. The cyan was colour-graded BGA
artwork.

### The dump was a duplicate

The user pointed out that texture dumping had been implemented before, and it had:
`DumpTextureToBmp` already existed in the boundary, writing to the same
`build/texture_dumps` directory. It produced 24-bit BMPs that dropped alpha — a
limitation its own comment identified as "exactly the case under investigation" —
and decoded every texture a second time purely for the dump. On the user's
decision the BMP texture path was removed along with its duplicate decode and the
`repiu-tex-alpha` logging that existed to compensate for the missing alpha. The
writer now serves the LFB surface alone and is named `DumpLfbSurfaceToBmp` to say
so.

### Implementation and verification

The new census records uploads, distinct addresses, identical versus changed
repeats by FNV-1a content hash, decode failures with the last failing format,
extent mismatches, palettized uploads arriving without a palette, format and
dimension histograms, and total bytes. Rejection paths are recorded too: the first
cut returned from `IsGlideTextureFormatAcceptable` and the dimension calculation
before reaching the census, which would have hidden precisely the silently missing
texture the census exists to catch. The dump is opt-in, capped at 512, and written
top-origin because a flipped dump sends the reader hunting a decode bug that is not
there.

Both configurations build, the probe passes all nine assertions, enabling the old
`REPIU_DUMP_TEXTURE_BMP` produces no BMPs and no alpha logging, and a clean
directory yields 28 uploads, 28 files, and 28 manifest rows through either spelling
of the dump variable. An earlier report of "dump written 0 with 24 files" was a
stale directory from a failed cleanup, not a defect.

Separately, three capture logs totalling about 450 KB had been swept into the
Task 374 commit by `git add -A`; they are untracked now and the ignore list covers
that shape of file.
