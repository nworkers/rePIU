# Task 511 작업 지시 — 실행 중에 읽는 귀속 보고

설계: [20260828-511](../design/20260828-511-live-execution-profile-report.md) ·
작업 로그: [20260828-511](../work-logs/20260828-511-live-execution-profile-report.md)

## 1. 파생값을 먼저 빼십시오

`veh_exclusive`와 `unaccounted`를 만드는 식이 `main.cpp` 안에 있습니다.
`Win32ExecutionTimeShares`와 `ComputeExecutionTimeShares`를 `execution_time_profile`에 두고,
`main.cpp`가 그것을 부르게 하십시오.

**`main.cpp`의 로그 출력은 한 글자도 바뀌면 안 됩니다.** 변수 이름을 그대로 두면 하류 코드가
손대지지 않습니다. 이것은 추출이지 변경이 아닙니다.

## 2. 보고기는 별도 파일입니다

`live_execution_profile_report.{h,cpp}`. 통합 지점에는 호출 한 줄만 남깁니다.

* env는 `REPIU_LIVE_PROFILE_INTERVAL_MS`, **한 번만** 읽습니다.
* 문자열은 고정 버퍼 `snprintf`로 만들고 `WriteHostErrorStream`으로 냅니다.
* **첫 호출은 보고하지 않고 시계만 시작합니다.** 첫 프레임 이전 구간은 다른 질문입니다.
* 누적 몫과 **창(window) 값**을 함께 냅니다. 누적만 내면 늦게 오는 비용이 가려집니다.

## 3. 훅은 `grBufferSwap` 한 자리입니다

`linexe_glide_boundary.cpp`의 `case go::kGrBufferSwap`, 프레임 덤프 훅 바로 옆입니다.

**게이트 진입마다 부르지 마십시오.** 시계 읽기가 계측을 계측 대상으로 만듭니다 — Task 353의
규칙이고, 프로파일 구조체 주석 두 곳이 그것을 근거로 들고 있습니다.

## 4. 검증

| 항목 | 기준 |
|---|---|
| Linux Release 빌드 | 성공 |
| **Linux live 보고** | 실행 중 여러 줄, 합이 말이 되는 값 |
| 재현 | 3회에서 `veh` 몫이 같은 범위 |
| **Windows 대조군** | 같은 장면·같은 프로파일로 요약의 share |
| Windows Debug probe | 15 / 15, 실패 0 |
| `main.cpp` 요약 | 추출 전과 같은 값 |

**cycle로 비교하십시오.** 두 호스트가 같은 기계이므로 TSC cycle이 직접 비교됩니다 — 백분율만
보면 510이 겪은 함정에 다시 빠집니다.

## 5. 하지 마십시오

* 하위 버킷으로 들어가지 마십시오. 최상위 다섯이 축을 가리키면 그것이 511의 끝입니다.
* 성능을 고치지 마십시오.

---

# Task 511 work order — attribution read while the run is going

Design: [20260828-511](../design/20260828-511-live-execution-profile-report.md) ·
Work log: [20260828-511](../work-logs/20260828-511-live-execution-profile-report.md)

## 1. Extract the derived values first

The formulas for `veh_exclusive` and `unaccounted` live inside `main.cpp`. Put
`Win32ExecutionTimeShares` and `ComputeExecutionTimeShares` in `execution_time_profile`, and have
`main.cpp` call them.

**`main.cpp`'s log output must not change by a character.** Keeping the variable names leaves
everything downstream untouched. This is an extraction, not a change.

## 2. The reporter is its own file

`live_execution_profile_report.{h,cpp}`. Leave one call at the integration point.

* The environment variable is `REPIU_LIVE_PROFILE_INTERVAL_MS`, read **once**.
* Build the string with a fixed-buffer `snprintf` and emit through `WriteHostErrorStream`.
* **The first call starts the clock and reports nothing.** The stretch before the first frame is a
  different question.
* Print the cumulative share **and a window value**. Cumulative alone hides a cost that arrives late.

## 3. The hook is one place: `grBufferSwap`

`case go::kGrBufferSwap` in `linexe_glide_boundary.cpp`, beside the frame-dump hook.

**Do not call it on every gate entry.** The clock read would make the instrument part of what it
measures -- Task 353's rule, which the profile struct's own comments cite twice.

## 4. Verification

| Item | Criterion |
|---|---|
| Linux Release build | succeeds |
| **The Linux live report** | several lines during the run, with values that add up |
| Repeatability | the `veh` share lands in the same range over three runs |
| **The Windows control** | the same scene and profile, shares from the summary |
| Windows Debug probe | 15 of 15, zero failures |
| `main.cpp`'s summary | the same values as before the extraction |

**Compare in cycles.** Both hosts are the same machine, so TSC cycles compare directly -- reading
percentages alone walks back into the trap Task 510 documented.

## 5. Do not

* Do not descend into the sub-buckets. If the top five name the axis, that is where 511 ends.
* Do not fix performance.
