# 20260729-353 Glide ordinal 시간 귀속 작업 로그 / Work log

* 설계: [20260729-353-glide-ordinal-time-attribution.md](../design/20260729-353-glide-ordinal-time-attribution.md)
* 작업 지시: [20260729-353-glide-ordinal-time-attribution.md](../work-orders/20260729-353-glide-ordinal-time-attribution.md)
* 최종 A/B: `build/benchmarks/glide-ordinal-time-final/20260729-191628/` (Git 제외)

## 한국어

### 결과

현재 Glide gate 시간을 정확한 ordinal별로 귀속했습니다. 최종 Release 60초 A/B에서
control 프레임 중앙값은 1,048(1,025~1,075), profile은 1,041(1,033~1,046)로
-0.67%였습니다. 세 실행 모두 overflow/clamp 0, 전체 Glide cycle coverage 평균
99.970%, 완료 ordinal count와 handled gate count가 일치했습니다.

세 실행의 안정된 1위는 ordinal 85 `_GRBUFFERSWAP@4`입니다. Glide gate의 평균
50.21%, 현재 profile 실행 wall-clock의 중앙값 약 17.32%를 차지했습니다. 3,120회
합계의 호출당 평균은 27,623,092 cycle이며 backend interval의 99.09%가 host work,
0.41%가 wake, 0.50%가 complete였습니다. 현재 backend는 `swap_interval=1`을 받지만
그 값을 사용하지 않고 host thread에서 `SDL_GL_SwapWindow`를 호출합니다. 따라서 다음
작업은 swap/vsync를 추측하지 말고 그 호출 내부를 직접 분해해야 합니다.

2위 `_GRLFBLOCK@24`는 Glide gate의 13.00%, wall-clock 약 4.51%였습니다.
`_GRDRAWTRIANGLE@12`는 5.34%로 렌더 submit 자체는 현재 첫 대상이 아닙니다.

주요 state setter 16개의 합은 Glide gate의 24.91%였고, backend 시간의 95.59%가
wake+complete였습니다. 이 집합은 host work보다 thread handoff가 지배하지만, 원본
호출 순서와 상태 변화를 보존하는 batching/coalescing 설계가 필요하므로 swap 분해
뒤 후보로 둡니다.

### 구현

* 기본 OFF `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1|on|true`
* ordinal 0~255 직접 index 고정 profile, hot path allocation/lock/sort 없음
* global `ExecutionTimeScope`의 기존 두 TSC read 결과를 optional completed-cycle
  output으로 전달해 ordinal gate 시간에 재사용
* Task 333의 기존 enter/publish/host-start/host-finish/resume timestamp를 backend가
  현재 ordinal에 직접 귀속
* timeout 중 열린 gate 하나와 부분 backend interval을 명시적으로 허용하되, 완료
  count와 모든 누적값의 방향을 검증
* 종료 시에만 활성 entry를 cycle 내림차순으로 정렬하고 전체 행 출력
* 합성 probe와 동일 바이너리 control/profile 3회 wrapper 및 CSV/JSON 산출물 추가

### 기각한 첫 구현

첫 구현은 gate마다 기존 backend 누적 snapshot을 앞뒤로 복사하고 별도 TSC 두 번을
읽었습니다. 첫 60초 A/B에서 control/profile 프레임 중앙값이 1,314/1,055
(-19.7%)로 G5를 실패했습니다. profile 실행은 host work가 큰 다른 phase로
이동했으므로 그 순위는 사용하지 않았습니다.

snapshot 복사를 제거한 뒤에도 3×10초 중앙값이 203/179(-11.82%)였습니다. 마지막으로
추가 TSC를 없애고 global gate scope의 cycle을 공유하자 3×10초 중앙값이 187/187로
같아졌고, 최종 60초 A/B도 -0.67%로 통과했습니다. 동기 handoff 경로에서는 총
산술 비용보다 timestamp 위치 자체가 phase를 바꿀 수 있다는 관찰 근거입니다.

### 검증

* PowerShell wrapper parse: 성공
* Win32 x86 Release `repiu_aot_probe`, `repiu_loader_win32` 빌드: 성공
* 전체 probe: `execution_time_profile_completed_output=true`,
  `glide_ordinal_timing_all=true`, exit 0
* 3×10초 shared-clock smoke: control/profile 187/187, 모든 completeness gate 통과
* 3×60초 A/B: 기존 Task 347 semantic invariant 통과
* profile active entry: 실행별 39개
* completed gate: 80,768 / 82,381 / 82,576
* timeout open gate: 1 / 0 / 0
* overflow/clamp: 모두 0

### 다음 작업

`grBufferSwap` host work가 단일 ordinal 30% gate를 크게 넘었습니다. 다음 작업은
`BufferSwap`을 command wake, `SDL_GL_SwapWindow`, frame-rate accounting, 결과 전달로
분리하고, SDL swap interval의 실제 설정·반환값과 host 환경의 vsync/present blocking을
관측해야 합니다. 원본 `swap_interval=1` 의미를 임의로 무시하거나 프레임을 drop하지
않습니다.

---

## English

### Result

Attributed current Glide gate time to exact ordinals. In the final three-run
60-second Release A/B, control median frames were 1,048 (1,025--1,075) and
profile median frames were 1,041 (1,033--1,046), a -0.67% observer effect.
Every run had zero overflow/clamps, mean 99.970% global Glide-cycle coverage,
and completed ordinal counts equal to handled gate counts.

Ordinal 85 `_GRBUFFERSWAP@4` ranked first in every run. It averaged 50.21% of
the Glide gate and about 17.32% of profile-run wall time. Across 3,120 calls,
mean cost was 27,623,092 cycles; 99.09% of its backend interval was host work,
against 0.41% wake and 0.50% completion. The backend receives
`swap_interval=1` but currently ignores it and calls `SDL_GL_SwapWindow` on
the host thread, so the next task must decompose that call rather than assume
whether it is vsync or another present stall.

`_GRLFBLOCK@24` ranked second at 13.00% of the gate and about 4.51% of wall
time. `_GRDRAWTRIANGLE@12` was only 5.34%. Sixteen major state setters
together held 24.91% of the gate, with 95.59% of their backend time in wake
plus completion; semantics-preserving batching/coalescing remains a later
candidate.

### Implementation and validation

The disabled-by-default profile directly indexes 256 fixed ordinal entries and
performs no hot-path allocation, locking, or sorting. An optional completed
cycle output reuses the existing global `ExecutionTimeScope` timestamps, while
the backend directly attributes Task 333's existing handoff timestamps to the
bound ordinal. Final reporting alone sorts and emits all active rows. A
synthetic probe and a same-binary control/profile wrapper validate counts,
coverage, timeout partials, backend deltas, observer impact, and stable ranks.

The first implementation copied cumulative backend snapshots twice and read
two extra timestamps per gate. It shifted median frames from 1,314 to 1,055
(-19.7%) and was rejected. Removing snapshots alone still yielded a -11.82%
three-run ten-second median. Reusing the global gate clock removed the extra
timestamps: the three-run smoke became 187/187 and the final 60-second A/B
passed at -0.67%.

Release loader and probe builds passed; the full probe reported
`execution_time_profile_completed_output=true` and
`glide_ordinal_timing_all=true`; all Task 347 semantic invariants passed.
The profile retained 39 active entries per run, 80,768/82,381/82,576 completed
gates, one/zero/zero timeout-open gates, and zero overflow/clamps.

### Next work

`grBufferSwap` host work exceeds the single-ordinal 30% gate. Next decompose
`BufferSwap` into command wake, `SDL_GL_SwapWindow`, frame-rate accounting,
and result delivery, while observing the actual SDL swap-interval state and
whether present blocks on vsync. Do not silently ignore the guest's
`swap_interval=1` semantics or drop original frames.
