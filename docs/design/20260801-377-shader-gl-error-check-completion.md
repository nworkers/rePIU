# GLSL 셰이더 검토와 남은 `glGetError` / GLSL shader review and the remaining `glGetError`

Task 377. **아직 구현 전. 검토 결과와 진행 방향 기록입니다.**

다음 세션에서 이 문서만 읽고 이어갈 수 있도록 확인된 사실, 미결 항목, 재현 명령을
모두 담았습니다.

* 선행: [369](20260731-369-glide-gl-error-check-policy.md),
  [370](20260731-370-glide-gl-debug-output.md),
  [371](20260731-371-glide-swap-interval-override.md)
* 관련: [docs/analysis/glide2x-ovl-and-opengl-hle.md](../analysis/glide2x-ovl-and-opengl-hle.md)

## 한국어

### 1. 셰이더는 무엇이고 언제 쓰이는가 — **확인됨**

Glide의 color/alpha combine, fog, 텍스처 샘플링을 GLSL로 구현한 **단일 프로그램**
입니다([glide_opengl_shader.cpp](../../src/platform/win32/glide_opengl_shader.cpp),
583줄).

| 항목 | 확인 결과 |
|---|---|
| 프로그램 수 | **1개** (`implementation_->program`) |
| `Initialize()` 호출 | 창 생성 시 **1회** ([backend:631](../../src/platform/win32/glide_opengl_backend.cpp#L631)) |
| 런타임 재컴파일 | **없음** — `compile_shader`/`link_program`은 `Initialize` 안에서만 사용 |
| 고정 기능 병용 | 예 — `glBegin`/`glEnd` 즉시 모드 위에 셰이더가 얹혀 있음 |

**셰이더 컴파일 스톨은 이 구조에 존재하지 않습니다.** 다른 에뮬레이터에서 흔한
"첫 등장 시 수백 ms 멈춤"이 여기서는 발생할 수 없습니다.

진입점은 여덟 개입니다.

| 진입점 | 호출 빈도 |
|---|---|
| `SetTextureEnabled` | **draw마다** ([backend:990](../../src/platform/win32/glide_opengl_backend.cpp#L990)) |
| `SetBlitMode` | LFB blit 진입/이탈 |
| `SetAlphaCombine` / `SetColorCombine` | 해당 Glide setter |
| `SetFogMode` / `SetFogColor` / `SetFogTable` | fog 계열 |
| `SetConstantColor` | `grConstantColorValue` |

### 2. 미결: `glGetError` 5개가 남아 있다 — **wall의 8.90%**

Task 369는 `glide_opengl_backend.cpp`만 정책 게이트 뒤로 옮겼고 **셰이더 모듈은
건드리지 않았습니다.** 남은 위치입니다.

| 행 | 함수 |
|---|---|
| [437](../../src/platform/win32/glide_opengl_shader.cpp#L437) | `SetFogMode` |
| [458](../../src/platform/win32/glide_opengl_shader.cpp#L458) | `SetFogColor` |
| [482](../../src/platform/win32/glide_opengl_shader.cpp#L482) | `SetFogTable` |
| [508](../../src/platform/win32/glide_opengl_shader.cpp#L508) | `SetAlphaCombine` |
| [540](../../src/platform/win32/glide_opengl_shader.cpp#L540) | `SetColorCombine` |

gameplay 캡처 실측입니다.

| ordinal | 셰이더 `glGetError` | work/call | wall |
|---|---|---:|---:|
| **`grAlphaCombine`** | **있음** | **285,694** | **6.62%** |
| `grFogColorValue` | 있음 | 168,061 | 1.26% |
| `grColorCombine` | 있음 | 32,823 | 0.76% |
| `grFogTable` | 있음 | 35,261 | 0.26% |
| **`grConstantColorValue`** | **없음** | **1,676** | 0.06% |
| **합계(있음)** | | | **8.90%** |

**같은 파일, 같은 uniform 업로드 기구, 같은 드라이버인데 `glGetError` 유무로
1,676 vs 285,694 — 170배입니다.** Task 369가 backend에서 만든 자연 실험이 그대로
재현됩니다. **uniform 업로드 자체는 싸고, 비용은 전부 에러 체크입니다.**

`grAlphaCombine`이 369 이전 48,980 → 현재 285,694로 5.8배 오른 것도 설명됩니다 —
`grDepthMask`가 프레임당 19회 배수하던 것을 멈추자 누적 명령이 다음 `glGetError`인
여기서 배수됩니다.

### 3. 게이트에 못 미치는 부수 관측

`SetTextureEnabled`가 **draw마다** 조건 없이 `glUseProgram` + `glUniform1i`를
부릅니다([backend:990](../../src/platform/win32/glide_opengl_backend.cpp#L990)).
값이 바뀌지 않아도 매번입니다.

다만 `grDrawTriangle` work/call이 2,434~2,996 cycle, 프레임당 84~86회로 약
**0.06 ms/frame**이라 게이트 미달입니다. Task 365의 직전값 비교 방식으로 없앨 수
있지만 단독 근거는 되지 않습니다.

### 4. 진행 방향 — Task 377

셰이더 모듈의 `glGetError` 5개를 **기존 정책 게이트
`REPIU_GLIDE_GL_ERROR_CHECK` 뒤로** 옮깁니다. Task 369가 만든 기구를 재사용하므로
변경이 작습니다.

| 항목 | 결정 |
|---|---|
| 게이트 | `REPIU_GLIDE_GL_ERROR_CHECK` (기본 OFF) — **신설하지 않고 재사용** |
| OFF일 때 반환 | uniform 업로드가 끝나면 `true` |
| ON일 때 | 기존과 동일 |
| 에러 가시성 | Task 370의 `glDebugMessageCallback`이 이미 상시 수신 중 |

마지막 항목이 중요합니다 — **370이 push 방식 보고를 이미 깔아두었으므로 이 5개를
없애도 에러가 안 보이게 되지 않습니다.**

### 5. 사전 등록 게이트

| 등급 | 기준 | 행동 |
|---|---|---|
| A | 실측 회수 ≥ 5% of wall | 유지·확정 |
| B | 2 ~ 5% | 유지하되 이동분 조사 |
| C | < 2% | 되돌리고 기록만 |

### 6. **반드시 지킬 측정 조건** — Task 370의 교훈

370에서 프레임 검사를 없애자 **대기가 present로 이동**했고, 371이 그것을 디스플레이
제한으로 규명했습니다. 이번에도 비용이 다음 동기화 지점으로 이동할 수 있습니다.

* **`REPIU_GLIDE_SWAP_INTERVAL=0` 고정** — vsync는 30 fps로 양자화해 CPU 개선을
  완전히 가립니다(371).
* **`REPIU_GLIDE_ORDINAL_TIME_PROFILE=1` 필수** — 이동 여부는 ordinal별 `work`로만
  보입니다. musicselect4 캡처는 이것이 꺼져 있어 크기를 재지 못했습니다.
* **A/B는 wall cycle과 프레임 수를 함께 확인** — 1초 무진행 watchdog이 조기 종료해도
  `timed_out=true`로 보고합니다(372).
* **판정은 gameplay 장면으로** — 자동 장면은 이 setter들이 훨씬 적습니다.
* **카운터를 빼거나 나누기 전에 각각이 무엇을 세는지 코드에서 확인** (376의 교훈).

재현 명령입니다.

```
cmd /c "set REPIU_EXECUTION_BACKEND=aot-dbt&& set REPIU_EXECUTION_TIMEOUT_MS=0&& set REPIU_GLIDE_SWAP_INTERVAL=0&& set REPIU_EXECUTION_TIME_PROFILE=1&& set REPIU_GLIDE_ORDINAL_TIME_PROFILE=1&& build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit1 > shader377.log 2>&1"
```

확인 지표는 `_GRALPHACOMBINE@20`의 work/call, `_GRBUFFERSWAP@4`의 work/call(이동
감지), 그리고 `Glide GL debug output installed/messages/errors`입니다.

### 7. 현재 남은 축 (참고)

| 항목 | wall 비중 | 상태 |
|---|---:|---|
| 커널 예외 왕복 | 22.5% | 열림 — 경계 발생 빈도가 표적 |
| VEH 핸들러 본체 | 20.3% | 열림 |
| **셰이더 `glGetError`** | **8.90%** | **본 작업** |
| 게스트 실행 | 약 57% | 열림 |
| 텍스처 / DOS I/O / Glide gate 예외 | — | 닫힘 (375 / 374 / 368) |

---

## English

### What the shader is

A single GLSL program implementing Glide's colour and alpha combine, fog, and
texture sampling, layered over `glBegin`/`glEnd` immediate mode. `Initialize()` runs
once at window creation and `compile_shader`/`link_program` appear nowhere else, so
**runtime recompilation stalls cannot occur in this design**. Eight entry points
exist; `SetTextureEnabled` is the only one called per draw.

### The outstanding cost

Task 369 gated the backend's `glGetError` calls but never touched the shader module,
which still has five: `SetFogMode`, `SetFogColor`, `SetFogTable`, `SetAlphaCombine`,
and `SetColorCombine`. Measured on a gameplay capture they cost **8.90% of wall**,
with `grAlphaCombine` alone at 285,694 cycles per call and 6.62%. The same file's
`SetConstantColor`, which has no error check, costs 1,676 — a 170-fold gap through
identical uniform-upload machinery, so the uploads are cheap and the cost is entirely
the check. `grAlphaCombine` rising 5.8x since Task 369 fits the same mechanism:
`grDepthMask` stopped draining nineteen times a frame, so the accumulated commands
now drain at the next `glGetError`.

A separate observation stays below the gate: `SetTextureEnabled` issues an
unconditional `glUseProgram` and `glUniform1i` on every draw, worth about 0.06 ms per
frame.

### Direction

Move the five calls behind the existing `REPIU_GLIDE_GL_ERROR_CHECK` policy rather
than inventing a new one, and rely on Task 370's already-installed
`glDebugMessageCallback` for error visibility. Pre-registered gate: keep it if the
measured recovery is 5% of wall or more, keep but investigate migration between 2 and
5%, revert below 2%.

### Measurement conditions that are not optional

Task 370 removed a check and the wait moved into the present, which Task 371 then
identified as a display limit. The same migration is possible here, so: pin
`REPIU_GLIDE_SWAP_INTERVAL=0` because vsync quantises to 30 fps and hides CPU
progress; enable `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, without which migration is
invisible and which the last music-select capture lacked; compare wall cycles
alongside frame counts because the one-second watchdog reports `timed_out` on an
early exit; judge on gameplay rather than the automated scene, where these setters
are far rarer; and confirm in code what a counter counts before subtracting or
dividing, which is the lesson Task 376 recorded.

## 최종 인계 / Final handoff

Task 377 이후 이어진 Music Select 성능 조사의 최종 채택·기각 결과와 추가 확인 항목은 [20260802-393-performance-investigation-handoff.md](20260802-393-performance-investigation-handoff.md)에 정리했습니다.

The final adopted/rejected results and remaining checks from the Music Select investigation that followed Task 377 are consolidated in [20260802-393-performance-investigation-handoff.md](20260802-393-performance-investigation-handoff.md).