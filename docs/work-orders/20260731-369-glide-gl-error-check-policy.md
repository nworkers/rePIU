# 작업 지시: Glide setter GL 에러 체크 정책 / Work order: Glide setter GL error-check policy

Task 369. 설계: [20260731-369](../design/20260731-369-glide-gl-error-check-policy.md)

## 한국어

### 목표

Glide setter 핫 경로의 호출당 `glGetError()`를 기본 OFF로 내리고, 프레임당 1회
체크로 대체한다. 기존 동작은 `REPIU_GLIDE_GL_ERROR_CHECK=1`로 완전 복원 가능해야
한다.

### 단계

1. **정책 모듈 추가**
   * `include/repiu/platform/win32/glide_gl_error_policy.h`
   * `src/platform/win32/glide_gl_error_policy.cpp`
   * `ResolveGlideGlErrorCheckEnabled(std::string_view)` — `1` / `on` / `true`만 수용.
   * `GlideGlErrorCheckEnabled()` — 1회 resolve 후 캐시.
   * `Win32GlideGlErrorPolicyProfile` / `RecordGlideGlErrorFrameCheck` /
     `SnapshotGlideGlErrorPolicy`.
   * `CMakeLists.txt`의 플랫폼 소스 목록에 등록.

2. **backend 통합** (`glide_opengl_backend.{h,cpp}`)
   * resolved-once 멤버 `glide_gl_error_check_enabled_` /
     `glide_gl_error_check_resolved_` + `GlideGlErrorCheckEnabled()`.
     기존 `GlideSetterPhaseEnabled()` 패턴을 따른다.
   * `CheckGlErrorIfEnabled()` 헬퍼 — OFF면 `glGetError`를 **호출하지 않고** `true`.
   * `Win32GlideGlErrorPolicyProfile glide_gl_error_policy_` 멤버 + 접근자.

3. **setter 13개소 게이트 적용**
   `SetColorMask`, `SetRenderBuffer`, `SetDepthMask`, `SetDepthBufferMode`,
   `SetAlphaBlend`(선행 drain 루프 + 후행 체크), `SetAlphaTestFunction`,
   `SetAlphaTestReferenceValue`, `SetDepthBufferFunction`, `SetFogMode`,
   `SetClipWindow`, `SetCullMode`, `SetDitherMode`.
   * `StoreTexture`, `PresentLfbSurface`, `ReadbackFramebuffer`는 **변경 금지**.
   * Task 364 phase 계측의 타임스탬프 구조는 **유지**한다.

4. **프레임당 안전망**
   * `BufferSwapOnHostThread`에서 `RecordPresentedFrame()` 직후 1회 drain.
   * 최초 에러 코드와 drain 반복수를 기록.

5. **요약 출력 연결**
   * `live_telemetry_snapshot.cpp` → `attempt.glide_gl_error_policy`
   * `main.cpp PrintExecutionAttempt`에 한 줄 추가:
     `Win32 Glide GL error policy per-call-check/frame-checks/frame-errors/last-code/drain-iterations`

6. **probe 추가**
   * `src/tools/aot_probe/glide_gl_error_policy_probe.{h,cpp}` + `main.cpp` 등록 +
     `CMakeLists.txt` 등록.
   * 검증: resolver 수용/거부(`1` `on` `true` 수용, `` `0` `1 ` `TRUE` 거부),
     기록 누적, 최초 에러 코드 보존, `nullptr` 무해.

7. **빌드 검증**
   * `build/win32_x86_debug` Debug + Release.

8. **문서**
   * `docs/analysis/glide-gate-cost-attribution.md` 신규 + `docs/analysis/README.md` 색인.
   * `docs/analysis/glide2x-ovl-and-opengl-hle.md`에 에러 정책 반영.
   * `docs/analysis/current-execution-frontier.md` 갱신.
   * 작업 로그 `docs/work-logs/20260731-369-*.md`.

### 완료 조건

* 기본 실행에서 setter 경로의 `glGetError` 호출이 0회.
* `REPIU_GLIDE_GL_ERROR_CHECK=1`이면 변경 전과 동일한 호출 형태.
* probe 전 항목 통과, Debug/Release 빌드 성공.
* 요약에 GL error policy 줄이 출력됨.

### 비범위

* Task 365 batch 2(`grDepthMask` 생략)는 본 작업에 포함하지 않는다.
* fog / combine setter의 GLSL uniform 경로 비용(약 0.9%)은 별건으로 남긴다.
* rendezvous 왕복 제거(direct 경로)는 별건으로 남긴다.

---

## English

### Goal

Move the per-call `glGetError()` on the Glide setter hot path behind
`REPIU_GLIDE_GL_ERROR_CHECK` (default off) and replace it with a single check per
frame, keeping the previous behaviour fully restorable by setting the variable.

### Steps

Add a policy module (`glide_gl_error_policy.{h,cpp}`) exposing a strict resolver, a
resolve-once accessor, and a small profile with a frame-check recorder; register it
in `CMakeLists.txt`. Integrate it into `GlideOpenGlBackend` with a resolve-once
member mirroring `GlideSetterPhaseEnabled()` and a `CheckGlErrorIfEnabled()` helper
that does not call `glGetError` at all when disabled. Apply the gate to the thirteen
setter sites listed in the design, leaving `StoreTexture`, `PresentLfbSurface`, and
`ReadbackFramebuffer` untouched and preserving Task 364's phase timestamps. Add the
per-frame drain immediately after `RecordPresentedFrame()` in
`BufferSwapOnHostThread`, plumb the snapshot through `live_telemetry_snapshot.cpp`
into the execution attempt, and print one summary line. Add an `aot_probe` covering
resolver acceptance and rejection, accumulation, first-error retention, and null
safety. Verify with Debug and Release builds under `build/win32_x86_debug`, then
update the analysis topics, the analysis README index, the execution frontier, and
the work log.

### Done when

The default run issues no `glGetError` on the setter path; setting the variable to
`1` restores the previous call shape; every probe assertion passes; both build
configurations succeed; and the summary carries the GL error policy line.

### Out of scope

Task 365 batch two (eliding `grDepthMask`), the GLSL uniform cost on the fog and
combine setters (~0.9% of wall), and removing the rendezvous round trip.
