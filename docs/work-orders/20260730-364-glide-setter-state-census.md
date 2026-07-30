# 20260730-364 Glide setter 반복률/phase 귀속 작업 지시 / Work order

* 설계: [20260730-364-glide-setter-state-census.md](../design/20260730-364-glide-setter-state-census.md)
* 상위 계획: [20260730-363-glide-call-performance-plan.md](../design/20260730-363-glide-call-performance-plan.md)
* 범위: 기본 OFF 계측 추가 및 측정. **최적화 구현 없음.**

## 한국어

### 1. 구현 항목

| # | 파일 | 내용 |
|---|---|---|
| 1 | `include/repiu/platform/win32/glide_setter_state_census.h` | census 자료형, 대상 setter 판정, 기록/스냅샷 API |
| 2 | `src/platform/win32/telemetry/glide_setter_state_census.cpp` | 위 구현 (`REPIU_GLIDE_SETTER_CENSUS`) |
| 3 | `include/repiu/platform/win32/glide_setter_phase_timing.h` | GL phase 자료형과 기록 API |
| 4 | `src/platform/win32/telemetry/glide_setter_phase_timing.cpp` | 위 구현 (`REPIU_GLIDE_SETTER_PHASE`) |
| 5 | `src/platform/win32/execution/thread_context.h` | census profile 보유 |
| 6 | `src/platform/win32/boundary/linexe_glide_boundary.cpp` | gate 경계 단일 hook (scope 객체) |
| 7 | `include/repiu/platform/win32/glide_opengl_backend.h` | phase profile 보유와 접근자 |
| 8 | `src/platform/win32/glide_opengl_backend.cpp` | `SetDepthMask`/`SetAlphaBlend` phase 계측 |
| 9 | `include/repiu/platform/win32/execution_trampoline.h` | attempt 스냅샷 필드 |
| 10 | `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 스냅샷 수집과 정렬 |
| 11 | `src/host/win32/main.cpp` | 종료 시 요약/ordinal별 로그 |
| 12 | `src/tools/aot_probe/glide_setter_state_census_probe.{h,cpp}` | census 단위 probe |
| 13 | `src/tools/aot_probe/glide_setter_phase_timing_probe.{h,cpp}` | phase 단위 probe |
| 14 | `src/tools/aot_probe/main.cpp`, `CMakeLists.txt` | probe 등록과 빌드 |
| 15 | `scripts/task364_glide_setter_state_census.ps1` | 3회 control/profile A/B와 gate 검사 |

### 2. 필수 제약

* 두 계측 모두 기본 OFF. OFF일 때 hot path에 추가 분기 하나 외의 비용이 없어야 합니다.
* hot path에서 allocation, 문자열 생성, 정렬을 하지 않습니다. 고정 크기 counter만
  씁니다.
* census는 dispatch 결과를 읽기만 합니다. 어떤 gate도 생략하거나 순서를 바꾸지
  않습니다.
* phase 계측은 backend가 이미 쓰는 `ReadGlideGateTimingCycles()`를 공유하고 새 clock
  함수를 만들지 않습니다.
* `message_` 대입을 timestamp 구간 밖으로 옮기되, 다른 의미 변경은 하지 않습니다.
* texture generation은 census 내부 값이며 렌더링 경로에서 읽히지 않습니다.

### 3. 검증 절차

1. `scripts/build_win32_x86.bat` (Debug) 통과
2. `scripts/build_win32_x86_release.bat` (Release) 통과
3. `repiu_aot_probe.exe` exit 0 — 두 구성 모두, 신규 probe 포함
4. `scripts/task364_glide_setter_state_census.ps1 -Runs 3 -DurationSeconds 60`
   * 설계 §5의 gate C1~C8을 script가 자동 검사하고 위반 시 throw
5. 결과로 설계 §6의 G1~G3을 판정하고 작업 로그에 기록

### 4. 산출물

* CSV: control/profile 실행별 행
* JSON: 중앙값, 관측자 delta, setter별 반복률, phase share, 절감 상한
* 작업 로그와 `docs/analysis/current-execution-frontier.md` 갱신
* `docs/analysis/glide2x-ovl-and-opengl-hle.md`에 확인된 사실 반영

### 5. 완료 조건

* 두 구성 빌드와 probe 통과
* gate C1~C8 통과
* G1~G3 판정과 다음 작업 순서가 문서에 기록됨
* 성능 코드 변경 없음(계측만)이 diff로 확인됨

---

## English

### Scope

Add two disabled-by-default instruments and measure with them. No optimization
is implemented in this task.

The census lives in a new telemetry module owned by `ThreadContext` and is
hooked once at the Glide gate boundary through a scope object, so no setter
dispatch case is edited. The GL phase split lives in a second module owned by
the OpenGL backend and instruments only the host-thread bodies of
`SetDepthMask` and `SetAlphaBlend`, sharing the backend's existing cycle
source rather than adding a clock. Snapshot plumbing follows the Task 353/354
pattern through `execution_trampoline.h`, `live_telemetry_snapshot.cpp`, and
the exit summary in `main.cpp`, and each module gets a unit probe registered in
the probe suite and CMake.

### Constraints

Both instruments are off by default and add nothing but one branch to the hot
path when off. No allocation, string building, or sorting occurs on the hot
path; sorting and formatting happen at exit. The census only reads dispatch
results and never skips or reorders a gate. The phase instrument reuses
`ReadGlideGateTimingCycles()`. The `message_` assignment moves out of the timed
region and nothing else changes semantically. The texture generation counter is
census-local and never read by the rendering path.

### Verification

Debug and Release builds must pass, the probe suite must exit 0 in both
configurations with the new probes included, and
`scripts/task364_glide_setter_state_census.ps1 -Runs 3 -DurationSeconds 60`
must satisfy gates C1 through C8 from the design, throwing on violation. The
run then decides pre-registered gates G1 through G3, which are recorded in the
work log along with the resulting next order. Deliverables are per-run CSVs, a
summary JSON with medians, observer delta, per-setter repetition rates, phase
shares, and the elision ceiling, plus updates to the work log, the execution
frontier, and the Glide HLE analysis topic.
