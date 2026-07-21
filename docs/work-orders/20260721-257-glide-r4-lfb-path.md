# 작업 지시 — Glide R4 LFB 경로 구현 / Work Order — Glide R4 LFB Path Implementation

* 작성일 / Date: 2026-07-21 (Task 257)
* 설계 / Design: `docs/design/20260721-257-glide-r4-lfb-path.md`
* 브랜치 / Branch: `claude/glide-api-call-audit`

## 1. 목적 / Goal

`grLfbLock@24`(112)과 `grLfbUnlock@8`(113)을 실동작시켜, 게스트가 백버퍼에 직접 쓴
565 픽셀이 화면에 도달하게 한다. 함께 관측된 `grConstantColorValue@4`(92)의 상태를
보존한다.

Make the LFB lock/unlock pair functional so guest-written 565 pixels reach the
screen, and retain the constant color observed alongside it.

## 2. 작업 항목 / Tasks

### T1. ordinal 감사 진단 정식화 / Formalize the audit diagnostic

* 대상: `src/platform/win32/boundary/linexe_glide_boundary.cpp`
* 이미 추가된 env-gated `REPIU_GLIDE_CALL_AUDIT` 최초호출 로그를 유지한다.
  출력이 고유 export 수(≤97줄)로 상한이 잡히므로 상시 컴파일해도 안전하다.
* 근거: 게이트 진입 로그 96건 캡과 타임아웃 경로의 요약 누락을 동시에 우회하는
  유일한 수단이며, 이번 작업의 검증에도 계속 쓰인다.

### T2. 플랫폼 공용 LFB 모듈 신규 / New platform-neutral LFB module

* 신규: `include/repiu/hle/glide_lfb.h`, `src/hle/glide_lfb.cpp`
* `CMakeLists.txt`의 `repiu_exe` 소스 목록에 `src/hle/glide_lfb.cpp` 추가.
* 제공 기능:
  * `GlideLfbSurface` — 스테이징 버퍼 소유(width×height×2, 565), lock 상태 보관.
  * `BuildGlideLfbInfoImage()` — `GrLfbInfo_t`(20바이트) 직렬화: `lfbPtr`,
    `strideInBytes`, `writeMode`, `origin`.
  * `DecodeGlideLfb565ToRgba8()` — 565 → RGBA8 변환(알파 255 고정).
  * `EncodeRgba8ToGlideLfb565()` — READ lock 대비 역변환.
* 상수: `kGlideLfbInfoByteCount = 20`, `kGlideLfbWriteMode565 = 0`,
  `kGlideBufferBackBuffer = 1`, `kGlideLfbReadOnly = 0`, `kGlideLfbWriteOnly = 1`,
  `kGlideOriginUpperLeft = 0`.

### T3. 백엔드 블릿/리드백 / Backend blit and readback

* 대상: `include/repiu/platform/win32/glide_opengl_backend.h`,
  `src/platform/win32/glide_opengl_backend.cpp`
* 추가:
  * `bool PresentLfbSurface(const std::uint8_t* rgba8, uint32 w, uint32 h, bool flip_v)`
    — 전용 GL 텍스처에 업로드 후 전체화면 쿼드를 백버퍼에 그린다.
  * `bool ReadbackFramebuffer(std::vector<std::uint8_t>* rgba8)` — `glReadPixels`.
* 상태 격리(설계 §3.3): 블릿 동안 `GL_DEPTH_TEST`/`GL_BLEND`/`GL_CULL_FACE` off,
  셰이더 텍스처 경로 강제 on, 정점색 흰색. 종료 시 직전 상태로 복원한다.
* 텍스처는 매 unlock마다 재생성하지 말고 전용 이름 하나를 재사용한다.

### T4. 게이트 핸들러 / Gate handlers

* 대상: `src/platform/win32/boundary/linexe_glide_boundary.cpp` (위임만)
* `_GRLFBLOCK@24`:
  * 인자 6개를 게스트 스택에서 직접 읽는다(미러는 8 dword라 충분하나 명시적으로 처리).
  * `info` 포인터 가독성 확인 → `size` 필드 읽어 20이 아니면 로그만 남기고 진행.
  * WRITE lock: 스테이징 버퍼 확보 후 `GrLfbInfo_t` 기록, `EAX=1`(FXTRUE).
  * READ lock: `ReadbackFramebuffer` → 565 인코딩 → 동일하게 기록, `EAX=1`.
  * 미지원 조합(565 외 writeMode, FRONTBUFFER 등): `EAX=0`으로 정상 반환 + 계측.
  * stdcall 정리: `Esp += 7 * 4` (반환주소 + 인자 6개).
* `_GRLFBUNLOCK@8`:
  * WRITE lock이었으면 565 → RGBA8 변환 후 `PresentLfbSurface` 호출.
  * lock 상태 해제. stdcall 정리: `Esp += 3 * 4`.
* `_GRCONSTANTCOLORVALUE@4`: 값을 `GlideLogicalState`에 보존. `Esp += 2 * 4`.
* **중요:** 어떤 실패 경로도 `reject_gate`로 떨어뜨리지 않는다(유지 정책). 진짜 ABI
  위반(반환주소 비게스트, info 포인터 판독 불가)만 예외로 한다.

### T5. 논리 상태 확장 / Logical state extension

* 대상: `include/repiu/hle/glide_hle.h`
* `GlideLogicalState`에 `constant_color` 추가. 상태 이미지(`BuildGlideStateImage`/
  `ParseGlideStateImage`) 확장 시 **version을 3으로 올리고** 양쪽을 함께 갱신한다.

## 3. 검증 절차 / Verification

1. `cmake --build build\win32_x86_debug --config Debug --target repiu_loader_win32`
2. 구동: `REPIU_EXECUTION_BACKEND=aot-dynamic`, `REPIU_EXECUTION_TIMEOUT_MS=360000`,
   `REPIU_GLIDE_CALL_AUDIT=1`, `REPIU_GLIDE_PIXEL_DIAG=1`
3. 판정:
   * ordinal 112/113/92가 `unhandled (default)` 목록에서 사라질 것.
   * `gate rejected` 0건 유지.
   * **1차 성공 기준:** LFB 시퀀스 이후 `non-black pixels`가 0에서 증가.
   * 초기화·삼각형 경로 회귀 없음.
4. 결과를 작업 로그와 `docs/analysis/` 두 문서에 반영.

## 4. 문서 갱신 / Documentation Updates

* `docs/work-logs/20260721-257-glide-r4-lfb-path-log.md` 신규.
* `docs/analysis/glide2x-ovl-and-opengl-hle.md` — ordinal 감사 결과와 LFB 확정 반영.
* `docs/analysis/current-execution-frontier.md` — Task 257 항목 추가.
* `ARCHITECTURE.md` — LFB 모듈 계층 반영.

## 5. 범위 밖 / Out of Scope

`grLfbWriteRegion`/`ReadRegion`, `ConstantAlpha/Depth`, `WriteColorSwizzle`,
565 외 writeMode, FRONTBUFFER lock, `pixelPipeline` 시맨틱, 드로우 계열 확장
(이번 감사에서 `grDrawTriangle` 외 전부 미호출로 확인됨).

---

## English Summary

Implement the LFB pair proven to be in use by the Task 257 audit: a new
platform-neutral `glide_lfb` module owning the 565 staging surface and
`GrLfbInfo_t` serialization, backend blit/readback entry points with explicit GL
state isolation, and gate handlers that delegate only. `grConstantColorValue` is
retained into logical state (state image version bumped to 3). No failure path
may fall through to `reject_gate` — unsupported combinations return gracefully
per the design 237 retain policy. Success is the pixel diagnostic reporting
non-black pixels after the LFB sequence, with zero rejected gates and no
regression in the initialization or triangle paths.
