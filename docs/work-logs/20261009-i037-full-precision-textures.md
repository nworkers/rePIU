# 작업 로그: 게임 내장 Mesa fx의 4444 절단을 우회하는 원본 정밀도 텍스처 (issue #37)

분석: `docs/analysis/mesa-fx-texture-path.md`
설계: `docs/design/20261009-i037-full-precision-textures.md`
작업 지시: `docs/work-orders/20261009-i037-full-precision-textures.md`

## 한 일

* **분석**: `repiu_aot_probe --dump-image <디렉터리>`(재배치된 오브젝트를 `obj<N>_<base>.bin`으로 저장)를
  더하고 capstone으로 읽었다. PIU는 OpenGL로 그리고 정적 링크된 Mesa 3.x fx 드라이버가 Glide로 옮긴다.
  `fxTexGetFormat`이 RGBA 계열을 ARGB4444, RGB 계열을 RGB565로 고르고, `fxTexBuildImageMap`이
  `&0xF0`(565는 `&0xF8/0xFC`)로 잘라 넘긴다. 원본은 `gl_texture_image +0x34`에 남아 있다. 세 서명이
  19개 롬셋 모두에서 한 번씩 나와 같은 빌드다.
* **원본 찾기(`repiu::hle::BuildMesaFxFullPrecisionTexture`)**: ESI(ti)·EBP(level)·`[ESP+0x34]`(tObj)에서
  `[tObj+0x484] == ti`, `ti.mipmapLevel[level].data == 게이트 data`를 확인하고, `tObj.Image[level]`의
  원본(4444는 RGBA, 565는 RGB에 알파 255)을 정수 배 nearest로 Glide 크기에 맞춘 뒤, 다시 잘라 게임
  데이터와 바이트 단위로 같을 때만 쓴다. 게스트 메모리는 읽기 가능 검사 뒤 읽기만 한다.
* **게이트**: `grTexDownloadMipMapLevel`에서 format 12/10일 때 위를 불러 `StoreTexture`에 넘긴다.
* **백엔드**: `TextureEntry`가 게임 RGBA와 원본 정밀도 RGBA를 함께 보관하고 옵션대로 올린다.
  `ApplyTextureFullPrecision()`이 이벤트 펌프 시작에서 옵션 변화를 보고 `glTexSubImage2D`로 다시 올린 뒤
  현재 바인딩을 되돌린다(`[repiu-glide] full-precision textures on/off: N re-uploaded`).
* **옵션**: OSD 체크박스 "Full-precision textures (8-bit)"(툴팁 포함), 환경 변수
  `REPIU_GLIDE_TEXTURE_FULL_PRECISION`(기본 켬), 런처 설정 `[Video] texture_full_precision`과 체크박스.
* **집계**: census에 결과별 수, 최종 로그 `Glide texture full precision used/not-applicable/unreadable/
  link-mismatch/data-mismatch/verify-mismatch`.
* **probe**: `mesa_fx_texture_source`(4444·565·정수 배 확대·실패 경로 다섯·홀수 크기), 런처 probe(왕복,
  잘못된 값 경고, 게시 순서, 환경 변수 우선).

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Release·Linux x64 Release 빌드 | 오류 0 |
| `repiu_aot_probe --mesa-fx-texture-source` | 4444·565·upscale·fallbacks·odd_size 모두 true |
| 런처 probe, core probe | Win32·Linux x64 실패 0 |
| pumpitea 실행(사용자, 2026-10-09) | 업로드 61개(4444 48, 565 13), `used=61`, 실패 0. OSD 토글마다 18개 다시 올림. 사용자가 화질 변화를 확인. 정상 종료(`exit-requested`) |
| pumpit8 실행(사용자, 2026-10-09, 약 65초) | 업로드 130개(4444 88, 565 42), `used=130`, 실패 0. 토글 41회, 회당 15~39개 다시 올림. 정상 종료 |

추정이던 `[ESP+0x34]` = tObj 연결은 이 실행으로 확인됐다.

## 확인하지 않은 것

* pumpitea·pumpit8 외 롬셋의 실행(서명은 같다).
* 크기 변환 경로가 실제로 쓰이는지(따로 집계하지 않음, 검증 실패 시 4444로 돌아감).
* Linux i386 빌드.

## 후속 후보: 메모리

검증된 텍스처마다 호스트 메모리에 RGBA8 두 벌(`game_rgba8`, `full_precision_rgba8`)을 더 들고 있다.
텍셀당 8바이트로, pumpitea 토글 시점 18개 항목은 많아야 약 9MB, 게임에 알려 주는 텍스처 메모리
8MB(텍셀 약 400만 개)를 기준으로 한 상한은 약 32MB다. 시작 주소가 다른 겹친 업로드는 예전 항목을 바로
지우지 않으므로 실제 상한은 이보다 클 수 있다. GPU 메모리는 변경 전과 같다.

`game_rgba8`은 없어도 된다. 원본을 쓰는 조건이 "원본을 다시 4444/565로 자르면 게임 데이터와 바이트
단위로 같다"이므로, 끌 때 원본을 다시 잘라 만들면 같은 이미지가 나온다. 그러면 추가분이 절반(상한
약 16MB)이 되고, 토글하는 순간 변환 계산이 든다. 사용자 결정(2026-10-09)으로 지금은 두 벌을 유지한다.

---

# Work log: full-precision textures bypassing the embedded Mesa fx 4444 truncation (issue #37)

**Done.** Added `repiu_aot_probe --dump-image <dir>` (each relocated object as
`obj<N>_<base>.bin`) and read pumpitea with capstone: PIU draws through OpenGL and its statically
linked Mesa 3.x fx driver translates to Glide; `fxTexGetFormat` picks ARGB4444 for RGBA formats and
RGB565 for RGB, and `fxTexBuildImageMap` cuts with `&0xF0` (`&0xF8/0xFC` for 565), leaving the
original at `gl_texture_image +0x34`; three signatures occur once in each of the 19 ROM sets.
`repiu::hle::BuildMesaFxFullPrecisionTexture` takes ESI (ti), EBP (level) and `[ESP+0x34]` (tObj),
checks `[tObj+0x484] == ti` and the level data pointer, reads `tObj.Image[level]` (RGBA for 4444,
RGB with alpha 255 for 565), scales it to the Glide size by Mesa's whole-factor nearest rule, and
uses it only if re-truncating it equals the game's data byte for byte, reading guest memory only
after a readability check. The `grTexDownloadMipMapLevel` gate calls it for formats 12 and 10 and
passes the result to `StoreTexture`; `TextureEntry` keeps both images and uploads the one the option
selects, and `ApplyTextureFullPrecision()` re-uploads with `glTexSubImage2D` at the start of the event
pump after a change and restores the binding. The option is an OSD checkbox, the
`REPIU_GLIDE_TEXTURE_FULL_PRECISION` variable (on by default) and the launcher's
`[Video] texture_full_precision`; the census and final log count each outcome. Probes: a new
`mesa_fx_texture_source` and an extended launcher probe.

**Verified.** Win32 and Linux x64 Release builds without errors; the new probe, the launcher probe
and the core probes pass on both. The user's pumpitea run on 2026-10-09 uploaded 61 textures (48
4444, 13 565), all used at full precision with no fallbacks, re-uploaded 18 per OSD toggle, showed
the quality change and exited normally — confirming the inferred `[ESP+0x34]` link; a following
pumpit8 run of about 65 seconds used all 130 uploads (88 4444, 42 565) with no fallbacks across 41
toggles of 15 to 39 re-uploads each.

**Not checked.** Runs on ROM sets other than pumpitea and pumpit8 (their signatures match), whether the resizing path occurs in
practice, and the Linux i386 build.

**Follow-up candidate: memory.** Each verified texture keeps two extra RGBA8 copies in host memory
(`game_rgba8`, `full_precision_rgba8`), 8 bytes per texel: at most about 9 MB for the 18 entries live
at pumpitea's toggle, about 32 MB bounded by the 8 MB texture memory the game is told (about 4 M
texels) — possibly more, since an overlapping upload at another start address does not drop the old
entry at once. GPU memory is unchanged. `game_rgba8` is redundant: an original is used only when
re-truncating it equals the game's data byte for byte, so it can be rebuilt from the original on
switching off, halving the extra memory (about 16 MB bound) at the cost of a conversion per toggle.
Both copies stay for now by the user's decision on 2026-10-09.
