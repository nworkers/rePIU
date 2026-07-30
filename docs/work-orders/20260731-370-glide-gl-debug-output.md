# 작업 지시: GL 디버그 출력으로 프레임 검사 대체 / Work order: replace the frame check with GL debug output

Task 370. 설계: [20260731-370](../design/20260731-370-glide-gl-debug-output.md)

## 한국어

### 목표

Task 369가 넣은 프레임당 `glGetError`(wall의 10.71%)를 제거하고, 동기화가 없는
`glDebugMessageCallback`으로 에러 보고를 대체한다.

### 단계

1. **정책 모듈 확장** (`glide_gl_error_policy.{h,cpp}`)
   * `Win32GlideGlErrorPolicyProfile`에 추가: `debug_output_installed`,
     `debug_message_count`, `debug_error_count`, `first_debug_message_id`,
     `first_debug_message`(고정 크기 버퍼).
   * `RecordGlideGlDebugMessage(profile, id, is_error, message, length)` —
     **할당·락 없음**, 첫 에러 메시지만 버퍼에 복사.
   * `ResolveGlideGlErrorFrameInterval(setting, *value)` — 숫자 파싱, 0 허용.
   * `TryReadGlideGlErrorFrameInterval(*value)` — `REPIU_GLIDE_GL_ERROR_FRAME_INTERVAL`
     이 설정됐는지와 값을 함께 반환.
   * snapshot에 동일 필드 반영.

2. **backend: 디버그 출력 설치**
   * `SDL_GL_GetProcAddress`로 `glDebugMessageCallback` / `glDebugMessageControl`
     해결. 셰이더 모듈의 `ResolveOpenGlFunction` 판정(널·1·2·3·-1 거부)을 따른다.
   * `GL_DEBUG_OUTPUT`(0x92E0) **켬**, `GL_DEBUG_OUTPUT_SYNCHRONOUS`(0x8242)
     **끔**.
   * 정적 트램펄린 콜백(`APIENTRY`)에서 `userParam`으로 backend를 받아 기록만 한다.
   * `NOTIFICATION`(0x826B) 등급은 카운트만 하고 첫 메시지 후보에서 제외.
   * 컨텍스트 생성 직후 `OpenWindowed`에서 1회 설치.

3. **프레임 검사 주기화**
   * 설치 성공 → 주기 0(완전 제거). 실패 → 기본 64.
   * `REPIU_GLIDE_GL_ERROR_FRAME_INTERVAL`이 명시되면 그 값을 우선한다.
   * 기존 drain 루프는 주기 도달 시에만 실행.

4. **요약 출력 확장**
   * 기존 줄에 debug output 필드를 잇는다:
     `Win32 Glide GL error policy per-call-check/frame-interval/frame-checks/frame-errors/first-code/drain-iterations`
     `Win32 Glide GL debug output installed/messages/errors/first-id/first-message`

5. **probe 확장** (`glide_gl_error_policy_probe.cpp`)
   * interval 파싱 수용/거부, 디버그 메시지 누적, 첫 에러 메시지 보존,
     NOTIFICATION 제외, `nullptr` 무해.

6. **빌드·검증**
   * Debug + Release 빌드, probe exit 0.
   * `REPIU_GLIDE_SWAP_TIME_PROFILE=1` 실측으로 accounting 구간 붕괴 확인.

7. **문서**
   * 작업 로그 `docs/work-logs/20260731-370-*.md`.
   * Task 369 설계·작업 로그에 post-present 배치 오판 정정 note.
   * `docs/analysis/glide-gate-cost-attribution.md`, `current-execution-frontier.md` 갱신.

### 완료 조건

* 디버그 출력 설치 시 프레임당 `glGetError` 호출 0회.
* `grBufferSwap`의 accounting 구간이 present 수준으로 하락.
* `GL_DEBUG_OUTPUT_SYNCHRONOUS`가 켜지지 않음.
* probe 전 항목 통과, 양 구성 빌드 성공.

### 비범위

* Task 365 batch 2, rendezvous 왕복 제거, 커널 예외 전달 계측은 별건.

---

## English

Remove the per-frame `glGetError` Task 369 introduced, which measured at 10.71% of
wall, and replace error reporting with `glDebugMessageCallback`. Extend the policy
module with debug-output counters and a fixed-size first-message buffer plus a
frame-interval resolver; install the callback once after context creation using the
shader module's proc-address validation, enabling `GL_DEBUG_OUTPUT` while leaving
`GL_DEBUG_OUTPUT_SYNCHRONOUS` off; record only counters from the trampoline, taking
no locks and allocating nothing, and exclude notification severity from the
first-message candidate. The frame check becomes periodic — interval zero when the
callback installs, otherwise 64, with `REPIU_GLIDE_GL_ERROR_FRAME_INTERVAL` taking
precedence when set. Extend the summary lines and the probe, build both
configurations, and confirm with a `REPIU_GLIDE_SWAP_TIME_PROFILE=1` capture that
the accounting interval collapses toward the present's own cost. Then update the
work log, correct the Task 369 design and log about the post-present placement, and
refresh the analysis topic and execution frontier.
