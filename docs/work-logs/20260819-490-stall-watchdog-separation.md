# Task 490 실행 제한과 정지 감시 분리 작업 로그

설계: [20260819-490-stall-watchdog-separation.md](../design/20260819-490-stall-watchdog-separation.md)

작업 지시: [20260819-490-stall-watchdog-separation.md](../work-orders/20260819-490-stall-watchdog-separation.md)

## 결과

- `REPIU_EXECUTION_TIMEOUT_MS`가 wall-clock 실행시간만 제한하도록 기존 암묵적 1초
  watchdog 연결을 제거했습니다.
- 별도 `REPIU_STALL_TIMEOUT_MS`를 추가했습니다. 미설정·빈 값·`0`·잘못된 값은 기본값
  `0`, 즉 비활성으로 해석합니다.
- 공용 runtime policy에 네 축의 진행 snapshot과 값 불일치 기반 비교를 추가했습니다.
  대소 비교를 하지 않으므로 32-bit 카운터 wrap도 진행으로 판정합니다.
- 실행별 `aot_dbt_glide_dispatch_entry_count`를 추가해 유효한 direct-dispatch 진입을
  성공/fallback 전에 기록합니다.
- `PollThreadUntilExit`는 diagnostic, single-step, AOT boundary/reentry, Glide direct
  entry를 진행으로 보고 wall/stall budget을 독립적으로 판정합니다.
- `Win32MinimalExecutionAttempt::stall_timed_out`과 종료 로그를 추가해 wall timeout과
  stall timeout을 구분합니다.
- 완료된 Task 445 후속 항목을 `docs/TODO.md`에서 제거하고 아키텍처와 누적 실행
  frontier를 갱신했습니다.

## 검증

- Win32 x86 Debug 전체 빌드: 성공(353.2초). 기존 CP949 `C4819` 경고만 남았습니다.
- Debug `repiu_aot_probe build/runtime_mounts/pumpit8/PIU/PIU.EXE`: exit 0,
  `valid=true`, `cache_valid=true`, `execution_timeout_policy=true`.
- Win32 x86 Release 전체 빌드: 성공(361.1초). 기존 경고만 남았습니다.
- Release의 같은 pumpit8 전체 probe: exit 0이며 위 세 항목이 모두 true입니다.
- 마지막 non-AOT explicit-stall progress-context 보완 뒤 Debug/Release 증분 전체 빌드도
  각각 37.3초/39.4초에 exit 0으로 통과했습니다.
- pumpit2 profile은 parent CHD 디렉터리 `roms/pumpit2`가 없어 mount 전에 exit 1로
  종료됐습니다. 기존 materialized pumpit2와 동일한 CHD identity를 가진 pumpit2a로
  runtime 검증을 수행했습니다.
- pumpit2a, wall 40초/stall 비활성: guest elapsed 40.00초, 프로세스 41.81초,
  `timed_out=true`, `stall_timed_out=false`. 이전 6.27초 조기 종료가 재현되지 않았고
  direct dispatch는 279,602회 성공했습니다.
- pumpit2a, wall 10초/stall 1초: guest elapsed 10.00초, 프로세스 11.47초,
  `stall_timed_out=false`. 명시적 watchdog이 활발한 실행을 오판하지 않았습니다.
- pumpit2a, wall 10초/stall 1ms 진단: 프로세스 1.48초,
  `stall_timed_out=true`. 별도 종료 사유 연결을 확인했습니다.

## 결론

wall-clock 상한을 사용하는 CI, 회귀 실행과 성능 캡처가 더 이상 암묵적인 1초
무진행 감시를 동반하지 않습니다. 정지 진단이 필요한 실행만 별도 stall budget을
명시하며, Glide direct-dispatch 활동도 생존 신호로 반영됩니다.

---

# Task 490 Execution-Budget and Stall-Watchdog Separation Work Log

Design: [20260819-490-stall-watchdog-separation.md](../design/20260819-490-stall-watchdog-separation.md)

Work order: [20260819-490-stall-watchdog-separation.md](../work-orders/20260819-490-stall-watchdog-separation.md)

## Result

- Removed the implicit one-second watchdog coupling so
  `REPIU_EXECUTION_TIMEOUT_MS` limits wall-clock execution time only.
- Added independent `REPIU_STALL_TIMEOUT_MS`. Unset, empty, explicit `0`, and malformed
  values resolve to the disabled default of `0`.
- Added a four-axis progress snapshot and inequality-based comparison to common runtime
  policy. Counter wrap therefore counts as progress.
- Added per-run `aot_dbt_glide_dispatch_entry_count`, recorded before a valid direct
  dispatch can choose success or fallback.
- `PollThreadUntilExit` now treats diagnostic, single-step, AOT boundary/reentry, and
  Glide direct entry as progress and evaluates wall/stall budgets independently.
- Added `Win32MinimalExecutionAttempt::stall_timed_out` and ending logs to distinguish
  wall and stall termination.
- Removed the completed Task 445 follow-up from `docs/TODO.md` and updated architecture
  and cumulative execution-frontier documentation.

## Verification

- Full Win32 x86 Debug build passed in 353.2 seconds with only existing CP949 `C4819`
  warnings.
- Debug pumpit8 full AOT probe exited zero with `valid=true`, `cache_valid=true`, and
  `execution_timeout_policy=true`.
- Full Win32 x86 Release build passed in 361.1 seconds with only existing warnings.
- The same Release pumpit8 probe exited zero with all three fields true.
- After the final non-AOT explicit-stall progress-context adjustment, incremental full
  Debug and Release builds also exited zero in 37.3 and 39.4 seconds respectively.
- The pumpit2 profile could not mount because parent CHD directory `roms/pumpit2` was
  absent. Runtime verification used pumpit2a, whose existing materialized mount has the
  same CHD identity as pumpit2.
- pumpit2a with wall 40 seconds and stall disabled reached guest elapsed 40.00 seconds
  (process 41.81 seconds), reported `timed_out=true` and `stall_timed_out=false`, and
  completed 279,602 direct dispatches. The former 6.27-second termination did not recur.
- pumpit2a with wall 10 seconds and stall one second reached the wall limit (process
  11.47 seconds) with `stall_timed_out=false`.
- A diagnostic one-millisecond stall run reported `stall_timed_out=true` separately.

## Conclusion

CI, regression runs, and performance captures may now use a wall-clock bound without
implicitly arming a one-second no-progress watchdog. Runs that need stall diagnosis set
an independent stall budget, and Glide direct-dispatch activity participates in the
liveness definition.
