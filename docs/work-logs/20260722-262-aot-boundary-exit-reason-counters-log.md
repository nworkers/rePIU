# 작업 로그: AOT 경계 이탈 사유별 카운터
# Work Log: Per-Reason Counters for AOT Boundary Exits

**Task:** 262 — 다음 단계 1번 / Next step 1
**브랜치 (Branch):** `claude/task-262-aot-dynamic-perf-aipxsa`
**설계 (Design):** `docs/design/20260722-262-aot-boundary-exit-reason-counters.md`
**작업 지시 (Work order):** `docs/work-orders/20260722-262-aot-boundary-exit-reason-counters.md`

## 한 일 (What was done)

`aot-dynamic`의 초당 약 1,400회 경계 이탈을 사유별로 관측할 수 있게 계측을 추가했다.
`aot_boundary_count`가 코드 전체에서 `HandleAotReentry`의 브레이크포인트 경로 한
곳에서만 증가한다는 사실을 확인하고, 그 지점에서 이탈 게스트 명령을 디코드해 5개
사유로 분류·집계한다.

Added instrumentation to observe `aot-dynamic`'s ~1,400/s boundary exits by
reason. Confirmed `aot_boundary_count` is incremented in exactly one place — the
breakpoint path of `HandleAotReentry` — and made that site decode the boundary
guest instruction into five reasons.

### 변경 파일 (Changed files)

* **신규** `src/platform/win32/aot/aot_boundary_reason.{h,cpp}` — host-neutral
  순수 분류기: `enum class AotBoundaryReason` + `ClassifyAotBoundaryInstruction`.
* `CMakeLists.txt` — `repiu_exe` 타깃에 새 소스 추가.
* `src/platform/win32/execution/thread_context.h` — 사유별 atomic 5개.
* `src/platform/win32/aot/aot_runtime_dispatch.{h,cpp}` — `BumpAotBoundaryReason`
  추가, `BumpAotBoundaryCount` 호출 지점에서 `IsGuestRangeReadable`로 최대 2바이트
  확보 후 분류·집계.
* `include/repiu/platform/win32/live_telemetry.h` — 사유별 volatile 필드 5개,
  `kWin32LiveTelemetryVersion` 16 → 17.
* `include/repiu/platform/win32/execution_trampoline.h` — 결과 구조체 필드 5개.
* `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` — 요약 복사.
* `src/host/win32/main.cpp` — 정상 종료 요약에 `boundary reason
  ret/indir/direct/cond/other` 한 줄.
* `src/host/win32/supervisor_main.cpp` — 주기 외부 덤프에
  `boundary_reason(ret/indir/direct/cond/other)=` 추가.

### 사유 분류 (Reason taxonomy)

| 사유 | opcode |
|---|---|
| `kReturn` | C3 C2 CB CA |
| `kIndirectBranch` | FF /2 /3 /4 /5 (인라인 캐시 미스) |
| `kDirectBranch` | E8 E9 EB 9A EA (경계 밖 타깃) |
| `kConditionalBranch` | 70..7F, 0F 80..8F, E0..E3 |
| `kOther` | 그 외 (비전달·prefix·미지원 stop) |

다섯 카운터의 합 = `aot_boundary_count` (불변식). 게스트 코드 쓰기 무효화와 페이지
retire/quarantine은 이미 전용 카운터가 있어 이 분류의 대상이 아니다.

The five sum to `aot_boundary_count`. Guest-code-write invalidation and page
retire/quarantine already have dedicated counters and are out of scope here.

## 검증 (Verification)

* **분류기 단위 검증 (통과).** host-neutral 분류기를 단독 컴파일하고 대표 opcode
  30개 케이스를 실행 — 전부 PASS(return/direct/indirect/conditional/other +
  truncated·null 경계 케이스 포함). 테스트: `scratchpad/test_boundary_reason.cpp`.
* **Win32 전 경로 빌드 (통과).** VS 2026(v18) C++ 툴체인으로
  `scripts/build_win32_x86.ps1` Debug 빌드 정상 완료(exit 0). 새 `aot_boundary_reason.cpp`,
  수정한 `aot_runtime_dispatch.cpp` 모두 MSVC로 컴파일됨(기존 C4819 코드페이지 경고만,
  무해). loader/supervisor 재링크 확인.
* **런타임 실측 (완료).** supervisor로 `pumpit1` 120초 aot-dynamic/legacy 각 1회
  구동, 사유별 카운터가 채워지고 **다섯 합 = `aot_boundary`(73,326) 불변식 확인**.
  결과는 아래 "실측 결과" 참조.

> **이전 초안 정정.** 이 로그의 최초 버전은 "loader host가 이 환경에서 빌드 불가"라고
> 적었으나 오판이었다. 이 환경에는 VS 2026 툴체인과 기존 build 디렉터리가 있어 빌드·실측
> 모두 가능했다.

* **Classifier unit test (passed).** 30 representative opcode cases, all PASS.
* **Full Win32 build (passed).** `scripts/build_win32_x86.ps1` Debug build
  completed (exit 0) with the VS 2026 toolchain; new/changed files compiled under
  MSVC (only the pre-existing benign C4819 codepage warning); loader/supervisor
  relinked.
* **Runtime measurement (done).** One 120 s supervised run each of aot-dynamic and
  legacy on `pumpit1`; the per-reason counters populated and the five summed to
  `aot_boundary` (73,326) exactly. *Correction: the first draft of this log
  claimed the host could not build here — it was wrong; a VS 2026 toolchain and an
  existing build dir made both build and measurement possible.*

## 실측 결과 (Measurement results)

Debug 빌드, `pumpit1`, 키 입력 없음, supervisor 외부 샘플링, 120초 동일 시점.

| 지표 | legacy | aot-dynamic | 비 |
|---|---:|---:|---:|
| `progress` | 606,613 | 29,457 | 20.6x |
| `single_step` | 3,058,527 | 786,814 | 3.9x |
| `dispatch_entry` | 3,069,426 | 798,252 | 3.85x |
| `aot_boundary` | 0 | 73,326 | — |

**aot-dynamic 이탈 사유 (n=73,326):** `other` 56,870 (77.6%) · `indirect` 9,305
(12.7%) · `return` 7,151 (9.8%) · `direct` 0 · `conditional` 0.

**핵심.** 지배 사유는 간접 분기(인라인 캐시)가 아니라 **비전달 명령 `other` 77.6%**다.
`direct`·`conditional`은 전 구간 0(캐시 내부 체이닝 검증). `indirect`는 ≈77초 이후
프레임 루프에서만 급증하는 후반 2차 현상. 상세·해석은
`docs/analysis/current-execution-frontier.md` Task 262 "진행 갱신" 참조.

**English.** Debug build, 120 s: legacy progress 606,613 vs aot-dynamic 29,457
(20.6x). aot-dynamic boundary reasons (n=73,326): other 77.6%, indirect 12.7%,
return 9.8%, direct 0, conditional 0. The dominant reason is a non-transfer
instruction (`other`), not indirect-branch inline-cache; direct/conditional are
never sentinels; indirect churn is a late-phase (~77 s) secondary effect.

## 다음 (Next)

1. Win32에서 `REPIU_EXECUTION_BACKEND=aot-dynamic REPIU_EXECUTION_TIMEOUT_MS=120000
   repiu_loader_win32.exe pumpit1`을 재구동해 supervisor 덤프의
   `boundary_reason(ret/indir/direct/cond/other)` 분포를 읽고, 초당 약 1,400회가
   어느 사유에 몰려 있는지 확정한다. 분포가 확정되면 frontier와 관련 analysis에
   반영한다.
2. Task 262 다음 단계 2번: AOT 체류량 계측(블록 진입당 실행 명령 수 또는 체류 시간
   비율)으로 커버리지 분모를 확보한다.

1. Re-run on Win32 and read the `boundary_reason` distribution from the
   supervisor dump to settle which reason the ~1,400/s concentrate in; record the
   result in the frontier and relevant analysis.
2. Task 262 next-step 2: measure AOT residency (instructions per block entry or
   residency-time fraction) to obtain the coverage denominator.
