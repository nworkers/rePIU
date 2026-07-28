# 20260728-331 작업 지시: 성능 기준의 Release 이전과 append 재귀속 / Work order

설계: [20260728-331-release-baseline-migration.md](../design/20260728-331-release-baseline-migration.md)

## 한국어

### 목표

1. Release 실행 계약을 스크립트로 고정한다. 사용자가 직접 Release 로더를 만들 수 있어야 한다.
2. append 다섯 단계를 **Release 기준으로 재귀속**해 다음 최적화 대상을 근거와 함께 고른다.
3. Debug 기준으로만 남아 있던 결론에 구성 표기를 붙인다.

### 범위

**포함**

* `scripts/build_win32_x86.ps1`에 `-Configuration` 인자 추가(기본 `Debug`, 기존 호출 호환).
* `scripts/build_win32_x86_release.ps1`와 `.bat` 추가.
* `src/tools/aot_probe/append_phase_benchmark_probe.{h,cpp}` 추가 및 probe suite 연결.
* Debug/Release 양쪽 빌드와 probe suite 실행.
* `docs/analysis/current-execution-frontier.md` 갱신.

**제외**

* 실게임 60초 Release A/B(사용자 실행 필요).
* 전체 실행 시간 축(veh/glide-gate 중첩) 재귀속.
* 최적화 구현 자체. 이번 작업은 **대상 선정까지**다.

### 구현 지침

* probe는 새 계측을 만들지 않는다. 제품 코드의 `Win32AotWorkerTimingProfile` 단계를
  그대로 읽는다. 그래야 probe 값과 로더 값의 정의가 같다.
* probe는 arena를 예약하고 그 주소로 이미지를 재배치한 뒤 실제
  `AppendWin32DynamicAotTranslation`을 구동한다. Task 329 이후 append는 살아 있는
  guest 바이트를 직접 참조하기 때문이다.
* 번역 크기를 small(실게임 평균 1,039 명령 근처)과 large(이미지 entry)로 나눈다.
  한 크기만 재면 고정 비용과 명령당 비용을 구분할 수 없다.
* 통과 조건은 결정론적 사실만 사용한다. 타이밍 값은 보고하되 통과 조건에 쓰지 않는다.
  느린 기계에서 실패하는 probe는 쓸모가 없다.

### 검증 절차

1. `scripts/build_win32_x86.ps1` (Debug) 전체 빌드 통과.
2. `scripts/build_win32_x86_release.ps1` (Release) 전체 빌드 통과.
3. `repiu_aot_probe`가 두 구성 모두 exit 0, 신규 `append_bench_*` 포함 전 그룹 통과.
4. 두 구성의 `append_bench_*` 값을 비교해 설계의 gate G1~G5를 판정한다.
5. `scripts/test_all.ps1`은 Debug 로더 경로를 사용하므로 기존과 동일하게 동작해야 한다.

---

## English

### Goal

Fix a Release execution contract in the build scripts so the user can produce a Release loader
directly, re-attribute the five append phases in Release so the next optimization target is chosen
on Release evidence, and label existing Debug-only conclusions with their configuration.

### Scope

In scope: a `-Configuration` parameter on `scripts/build_win32_x86.ps1` defaulting to `Debug` so
existing callers are unaffected, a Release entry point in PowerShell and batch form, the new
`append_phase_benchmark_probe` wired into the probe suite, builds and probe runs in both
configurations, and the frontier document update. Out of scope: the 60-second in-game Release A/B,
which only the user can run; the whole-run bucket re-attribution, whose shares still overlap past
100%; and the optimization itself, since this task ends at target selection.

### Implementation notes

The probe adds no instrumentation and reads the product's own `Win32AotWorkerTimingProfile`
phases, so probe and loader quantities share a definition. It reserves an arena and relocates the
image into it before driving a real append, because since Task 329 the append reads live guest
bytes directly. It measures a small translation near the 1,039-instruction in-game mean and the
whole image entry, since one size cannot separate fixed from per-instruction cost. Pass conditions
use deterministic facts only; timings are reported but never gate, because a probe that fails on a
slow machine is useless.

### Verification

A full Debug build, a full Release build, `repiu_aot_probe` exiting 0 in both configurations with
the new `append_bench_*` group passing, a comparison of the two configurations' values against
gates G1-G5, and `scripts/test_all.ps1` still behaving as before since it uses the Debug loader
path.
