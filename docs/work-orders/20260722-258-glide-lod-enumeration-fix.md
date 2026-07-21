# 작업 지시 — GrLOD_t 열거값 해석 정정 / Work Order — Correct GrLOD_t Enumeration Handling

* 작성일 / Date: 2026-07-22 (Task 258)
* 브랜치 / Branch: `claude/glide-api-call-audit` (커밋 `9bfde75`)
* 선행 / Predecessor: Task 257 검증 중 발견

> **절차 예외 기록.** AGENTS.md는 설계 → 작업 지시 → 구현 순서를 요구하나, 이 건은
> Task 257 검증 도중 발견된 **명백한 규격 위반 결함**이고 수정 범위가 계산식 3곳으로
> 한정돼 선수정 후 문서화했다. 규격 근거는 §2에 남긴다.

## 1. 문제 / Problem

게임이 정상 좌표의 콘텐츠 지오메트리(15×31 등 글자 사각형)를 제출하는데도 화면에
아무것도 렌더되지 않았다(삼각형별 백버퍼 리드백 `nonblack 0 → 0`).

## 2. 근인 (확인됨) / Root Cause

Glide의 `GrLOD_t`는 크기의 log2가 아니라 **열거값**이며 **0이 가장 큰 256**이다.

```
GR_LOD_256=0  GR_LOD_128=1  GR_LOD_64=2  GR_LOD_32=3  GR_LOD_16=4
GR_LOD_8=5    GR_LOD_4=6    GR_LOD_2=7   GR_LOD_1=8
```

즉 LOD의 **긴 변** = `256 >> lod` (log2 = `8 - lod`). 그런데 두 계산식이 LOD 값을
지수로 직접 사용했다.

```cpp
width_log2 = large_lod + aspect_ratio - 3;   // lod=0, aspect=3 → 2^0 = 1x1
```

관측된 `largeLod=0, aspect=3(1x1)` 다운로드는 **256×256**인데 **1×1**로 생성됐다.
정확히 뒤집혀, 진짜 1×1(`lod=8`)이면 256×256이 된다.

**결정적 증거.** 게임이 제출하는 텍스처 좌표가 `st=(193,156)`, `(226,156)`,
`(244,·)` — 1×1 텍스처에는 존재할 수 없는 값이며 256폭 텍스처를 가리킨다.

**2차 피해 (게스트 오염).** 같은 계산이 `grTexTextureMemRequired`에도 있고, 게임은
그 반환값으로 **자기 TMU 할당을 결정**한다. 256×256 텍스처에 "8바이트"를 보고하자
게임이 텍스처를 8바이트 간격으로 배치했다. `docs/analysis/glide2x-ovl-and-opengl-hle.md`
의 Task 255 항목은 그 8바이트 간격을 **1×1의 확증으로 기록**했는데, 이는 우리 버그가
만든 증거로 그 버그를 정당화한 순환 논리였다 — 해당 결론은 반증됐다.

참조: [3Dfx Glide 2.4 Reference Manual](https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf) — `GrLOD_t`, `GrAspectRatio_t`, `grTexTextureMemRequired`.

## 3. 작업 항목 / Tasks

### T1. `CalculateGlideTextureDimensions` (`src/hle/glide_texture_decode.cpp`)

* `edge_log2 = 8 - large_lod` 도입.
* aspect ratio를 **긴 변이 아니라 짧은 변**에 적용한다.
  `GR_ASPECT_8x1(0)`~`1x1(3)`은 가로가 길고, `1x2(4)`~`1x8(6)`은 세로가 길다.
  ```
  width_log2  = edge_log2 - max(aspect - 3, 0)
  height_log2 = edge_log2 - max(3 - aspect, 0)
  ```

### T2. `CalculateGlideTextureMemoryRequired` (`src/hle/glide_hle.cpp`)

* 동일한 LOD→변 길이 변환을 적용한다(게스트 할당 오염 차단).
* mipmap 순회 방향 정정: `largeLod`가 숫자로 더 작으므로 `lod = large_lod`에서
  `small_lod`까지 증가시킨다.

### T3. LOD 유효성 검사 정정

* `info.small_lod > info.large_lod`를 실패로 보던 검사를 뒤집는다. 올바른 불변식은
  **`large_lod <= small_lod`** 이다.

## 4. 검증 절차 / Verification

관측 한계: 콘텐츠 화면은 **키 입력이 있어야** 나온다(v0.0.77/78의 JAMMA 입력 수정
이후 정상 동작). `port_io_emulator.cpp`는 `GetAsyncKeyState`로 전역 물리 키 상태를
읽으므로 `keybd_event` 합성 입력이 창 포커스와 무관하게 전달된다.

1. 빌드: `cmake --build build\win32_x86_debug --config Debug --target repiu_loader_win32`
2. 구동: `REPIU_EXECUTION_BACKEND=aot-dynamic`, `REPIU_GLIDE_PIXEL_DIAG=1`,
   `REPIU_GLIDE_DRAW_DIAG=1`, `REPIU_GLIDE_TEX_DIAG=1`
3. 45초 후 F1(TEST)/F5(COIN)/발판 5키 주입.
4. 판정:
   * `StoreTexture`가 **256×256**으로 저장.
   * 게임의 텍스처 주소 간격이 8바이트가 아닌 정상 간격으로 변화.
   * 삼각형별 `nonblack` 누적 증가, 프레임 비검정 픽셀 > 0.
   * 거부·미처리 게이트 0 유지.

## 5. 문서 갱신 / Documentation Updates

* `docs/work-logs/20260722-258-glide-lod-enumeration-fix-log.md` 신규.
* `docs/kb/glide-texture-lod-and-formats.md` 신규 + `docs/kb/README.md` 색인.
* `docs/analysis/glide2x-ovl-and-opengl-hle.md` — Task 255의 "1×1 텍스처" 결론 정정.
* `docs/analysis/current-execution-frontier.md` — Task 257/258 항목.

---

## English Summary

`GrLOD_t` is an enumeration where `GR_LOD_256` is 0 and `GR_LOD_1` is 8, so a
LOD's larger edge is `256 >> lod`. Both texture paths used the LOD value directly
as a log2 exponent, inverting every texture: the observed `largeLod=0`/`1x1`
download is 256x256 but was built as 1x1, which the game's texture coordinates
(up to 244) contradict outright. Because `grTexTextureMemRequired` shares the
math and the guest sizes its own TMU allocations from that answer, the error
propagated into the game, which then packed textures 8 bytes apart — and the
Task 255 analysis recorded that spacing as proof the textures were 1x1, closing
a circular argument. Fix the LOD-to-edge conversion in both places, apply the
aspect ratio to the shorter edge, and invert the validity check to
`large_lod <= small_lod`. Verify with synthesized JAMMA key input driving the
guest to a content screen.
