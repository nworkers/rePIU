# Task 490 실행 제한과 정지 감시 분리 작업 지시

설계: [20260819-490-stall-watchdog-separation.md](../design/20260819-490-stall-watchdog-separation.md)

1. 공용 runtime timeout policy에 stall budget 설정과 진행 snapshot 비교를 추가합니다.
2. Win32 환경 변수 `REPIU_STALL_TIMEOUT_MS`를 읽어 기본 비활성 정책을 적용합니다.
3. Glide direct-dispatch 진입을 실행별 atomic 카운터에 기록합니다.
4. `PollThreadUntilExit`에서 wall timeout과 stall timeout을 독립적으로 판정합니다.
5. 실행 결과와 로그가 wall/stall 종료 사유를 구분하도록 연결합니다.
6. 공용 probe를 확장하고 Win32 x86 Debug/Release 빌드와 runtime 검증을 수행합니다.
7. `ARCHITECTURE.md`, 관련 analysis, `docs/TODO.md`와 작업 로그를 갱신합니다.

## 완료 조건

- 유한 wall budget이 stall watchdog을 암묵적으로 활성화하지 않습니다.
- 명시적으로 활성화한 stall watchdog은 Glide direct-dispatch 진입을 진행으로 봅니다.
- wall timeout과 stall timeout이 실행 결과와 로그에서 구분됩니다.
- probe와 영향 범위 빌드가 통과하고 pumpit2 조기 종료 회귀가 해소됩니다.

---

# Task 490 Execution-Budget and Stall-Watchdog Separation Work Order

Design: [20260819-490-stall-watchdog-separation.md](../design/20260819-490-stall-watchdog-separation.md)

1. Add stall-budget configuration and progress-snapshot comparison to common runtime
   timeout policy.
2. Read Win32 `REPIU_STALL_TIMEOUT_MS` with a disabled-by-default policy.
3. Record Glide direct-dispatch entry in a per-run atomic counter.
4. Make `PollThreadUntilExit` evaluate wall and stall timeouts independently.
5. Distinguish wall and stall termination in execution results and logs.
6. Extend common probes and run Win32 x86 Debug/Release builds and runtime checks.
7. Update `ARCHITECTURE.md`, relevant analysis, `docs/TODO.md`, and the work log.

## Completion Criteria

- A finite wall budget does not implicitly arm the stall watchdog.
- An explicitly enabled stall watchdog treats Glide direct-dispatch entry as progress.
- Execution results and logs distinguish wall timeout from stall timeout.
- Probes and impact-appropriate builds pass, and the pumpit2 premature timeout
  regression is cleared.
