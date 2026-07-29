# 20260729-354 Glide buffer swap 시간 분해 작업 로그 / Work log

* 설계: [20260729-354-glide-buffer-swap-time-decomposition.md](../design/20260729-354-glide-buffer-swap-time-decomposition.md)
* 작업 지시: [20260729-354-glide-buffer-swap-time-decomposition.md](../work-orders/20260729-354-glide-buffer-swap-time-decomposition.md)
* 최종 A/B: `build/benchmarks/glide-buffer-swap-final/20260729-194221/` (Git 제외)

## 한국어

### 결과

Task 353에서 확인한 `grBufferSwap` host work를 직접 분해했습니다. 최종 Release
60초 동일 바이너리 A/B의 프레임 중앙값은 control 1,296(1,295~1,327), profile
1,339(1,334~1,357)로 +3.32%였으며 observer gate ±5%를 통과했습니다.

profile 세 실행의 4,030회 swap은 모두 성공했고 failure/clamp는 0입니다. 내부 phase
total은 ordinal 85 host-work의 평균 99.940%를 덮었습니다.

| phase | 세 실행 cycle 합 | 실행별 평균 share |
|---|---:|---:|
| setup | 2,216,381 | 0.006% |
| `SDL_GL_SwapWindow` | 39,916,072,524 | **99.589%** |
| FPS accounting | 124,001,473 | 0.379% |
| finalize | 9,409,404 | 0.027% |
| total | 40,051,699,782 | 100% |

요청 interval은 4,030회 모두 `1`이었고, 각 실행의 첫 profile swap에서
`SDL_GL_GetSwapInterval`도 모두 `1`을 반환했습니다. 현재 실행 환경에서는 backend가
명시적으로 interval을 설정하지 않아도 실제 context 상태가 원본 요청과 일치합니다.
따라서 Task 353의 “interval이 사용되지 않는다”는 코드 사실은 유지되지만, “실제
interval이 미확정”이라는 질문은 현재 호스트에 대해 `1`로 해소됐습니다.

`SDL_GL_SwapWindow`는 current profile wall-clock의 실행별 9.66%, 9.89%, 4.91%로
중앙값 약 9.66%였습니다. 실행별 평균 present cycle은 약 11.83M, 11.86M, 5.98M인데
최대값은 1.40B, 1.40B, 1.03B로 평균의 118~173배입니다. aggregate만으로는 이 tail이
vblank miss, GPU/driver stall, 또는 스케줄링 중 무엇인지 분리할 수 없습니다.

그러나 실제 interval과 guest 요청이 일치하고 present 자체가 99% 이상이므로 이를
성능 최적화라는 이유로 끄거나 프레임을 생략하지 않습니다. 즉시 실행 가능한 다음
HLE 비용 축은 Task 353의 2위인 `grLfbLock` readback입니다. swap interval의
명시적 적용은 다른 호스트에서 불일치가 확인될 때 portability/fidelity 작업으로
다룹니다.

### 구현

* 기본 OFF `REPIU_GLIDE_SWAP_TIME_PROFILE=1|on|true`
* guest gate에서 온 `BufferSwap`만 계측하고 LFB 내부 direct present는 제외
* host-side entry/present-start/present-end/accounting-end/finish timestamp로 네
  phase와 total을 누적
* 첫 profile swap에서 `SDL_GL_GetSwapInterval`을 관측만 하고 설정은 변경하지 않음
* 요청 interval 0/1/기타 분포, 성공/실패, 최대 present, clamp를 고정 profile에 누적
* quiescent final snapshot과 loader summary 연결
* 합성 probe와 Task 347 기반 동일 바이너리 control/profile wrapper, CSV/JSON 추가

### 검증

* `git diff --check`: 성공
* PowerShell wrapper parse: 성공
* Win32 x86 Release `repiu_aot_probe`, `repiu_loader_win32`: 빌드 성공
* 전체 probe: `execution_time_profile_all=true`,
  `glide_ordinal_timing_all=true`, `glide_buffer_swap_timing_all=true`
* 10초 smoke: control/profile 330/322(-2.42%), present 99.61%,
  ordinal host-work coverage 99.85%, SDL interval 1
* 3×60초 A/B: Task 347 semantic invariant와 설계 G1~G7 모두 통과
* 요청/관측 interval: 실행별 전 요청 1 / SDL query 1

기존 C4819 및 LNK4217 경고 외에 새 compile/link 경고나 오류는 없었습니다.

### 다음 작업

`grLfbLock`의 약 4.51% wall-clock 후보를 guest surface 준비, OpenGL readback,
format/stride 변환, handoff로 나눕니다. swap present tail histogram은 실제
interval 불일치나 presentation cadence 문제를 조사할 때 별도 opt-in으로 추가합니다.

---

## English

### Result

Directly decomposed the `grBufferSwap` host work identified by Task 353. The
final same-binary three-run 60-second Release A/B produced control median
frames of 1,296 (1,295--1,327) and profile median frames of 1,339
(1,334--1,357), a +3.32% phase shift within the ±5% observer gate.

All 4,030 profiled swaps succeeded with zero failures or clamps. Internal
phase totals covered a mean 99.940% of ordinal 85 host work.
`SDL_GL_SwapWindow` averaged 99.589% of internal swap time; setup, FPS
accounting, and finalize averaged 0.006%, 0.379%, and 0.027%. Every guest
request supplied interval 1, and the one-time SDL query returned interval 1
in all three runs.

The code still does not explicitly apply the guest interval, but the current
host context already matches it. Present occupied 9.66%, 9.89%, and 4.91% of
profile wall time, a 9.66% median. Maximum present samples were 118--173 times
each run's mean, but aggregate telemetry cannot distinguish vblank misses,
GPU/driver stalls, and scheduling in that tail.

Because requested and actual intervals match and present itself exceeds 99%
of internal work, this task does not disable synchronization or drop original
frames. The next actionable HLE cost is Task 353's second-ranked `grLfbLock`
readback. Explicit interval application belongs to a portability/fidelity
task if another host demonstrates a mismatch.

### Implementation and validation

Disabled-by-default `REPIU_GLIDE_SWAP_TIME_PROFILE=1|on|true` records only
guest-gate buffer swaps, excluding internal direct LFB presentation. Five
host-side timestamps split setup, SDL present, FPS accounting, and finalize;
the profile also retains requested interval distribution, one-time SDL query,
success/failure, maximum present, and clamps. Final quiescent reporting,
synthetic probe coverage, and a Task 347-based control/profile CSV/JSON
wrapper were added.

Release probe and loader builds passed. The full probe reported
`execution_time_profile_all=true`, `glide_ordinal_timing_all=true`, and
`glide_buffer_swap_timing_all=true`. A ten-second smoke passed at -2.42%
observer shift, 99.61% present share, 99.85% ordinal-work coverage, and SDL
interval 1. The final three-run A/B passed every Task 347 semantic invariant
and design gate G1--G7. Only pre-existing C4819 and LNK4217 warnings remained.

### Next work

Split the approximately 4.51% wall-time `grLfbLock` candidate into guest
surface preparation, OpenGL readback, format/stride conversion, and handoff.
Add a present-tail histogram only when investigating an interval mismatch or
presentation-cadence defect.
