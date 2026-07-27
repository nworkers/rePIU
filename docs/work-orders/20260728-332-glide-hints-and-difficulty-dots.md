# 20260728-332 작업 지시: grHints 구현과 난이도 점 진단 / Work order

설계: [docs/design/20260728-332-glide-hints-and-difficulty-dots.md](../design/20260728-332-glide-hints-and-difficulty-dots.md)

## 한국어

### 목표

1. 로그에 유일하게 남은 미구현 Glide API `_GRHINTS@8`을 구현합니다.
2. 난이도 점 미표시의 원인을 A/B/C 중 하나로 좁히는 판별 계측을 추가합니다.

### 구현 항목

1. `include/repiu/hle/glide_hle.h`
   - `GlideLogicalState`에 `stw_hint`, `fifo_check_hint`, `fpu_precision_hint`,
     `allow_mipmap_dither_hint`, `hints_seen`을 추가합니다. 각 필드에 "렌더링에
     영향을 주지 않는 선언"임을 주석으로 남깁니다.
2. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `kGrHints`를 hint type별 상태 기록으로 구현합니다. 알 수 없는 type과 STW 예약
     비트는 계속 보고합니다. stdcall 프레임 정리는 그대로 둡니다.
   - `REPIU_GLIDE_DRAW_CENSUS` 계측을 `grDrawTriangle` 경로에 추가합니다.
3. `include/repiu/platform/win32/glide_opengl_backend.h`,
   `src/platform/win32/glide_opengl_backend.cpp`
   - 읽기 전용 접근자와 미스 카운터를 추가합니다.

### 안전 조건

- 계측은 env-gated 기본 OFF, 렌더링 동작 무변경.
- `grHints`는 인자 2개 + 반환 주소를 정리하는 기존 ABI를 유지합니다.
- backend 접근자는 GL 상태를 만지지 않습니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe` 전체 통과, `repiu_glide_issue_probe` 통과.
3. `REPIU_GLIDE_DRAW_CENSUS=1`로 MUSIC SELECT 화면까지 실행해 설계 4절의 판정 규칙을
   적용합니다. 이 실행은 사용자 확인 후 수행합니다.
4. 실행 후 `_GRHINTS@8` critical 로그가 사라졌는지 확인합니다.

---

## English

### Goal

Implement `_GRHINTS@8`, the only unimplemented Glide API left in the log, and add the
instrumentation that narrows the missing difficulty dots to one of the three causes in the design.

### Implementation

Add the four hint fields plus a seen flag to `GlideLogicalState`, implement `kGrHints` as
per-type state recording that still reports unknown types and reserved STW bits while keeping the
stdcall frame handling, add the `REPIU_GLIDE_DRAW_CENSUS` instrumentation to the `grDrawTriangle`
path, and expose read-only binding accessors and miss counters on the OpenGL backend.

### Safety

Instrumentation is environment-gated and off by default with no rendering change, `grHints` keeps
its two-argument stdcall cleanup, and the backend accessors touch no GL state.

### Verification

Build, pass `repiu_aot_probe` and `repiu_glide_issue_probe`, then run to the MUSIC SELECT screen
with `REPIU_GLIDE_DRAW_CENSUS=1` and apply the design's reading rules; that run happens after user
confirmation. Confirm afterwards that the `_GRHINTS@8` critical line is gone.
