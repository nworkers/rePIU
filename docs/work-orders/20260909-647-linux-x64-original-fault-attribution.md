# Task 647 작업 지시: Linux x64 복구 전 원래 fault 귀속

설계: [20260909-647](../design/20260909-647-linux-x64-original-fault-attribution.md)

## 한국어

1. `ThreadContext`에 bounded recovery provenance 상태와 source/path 표현을
   추가합니다.
2. `RecoverToHost` 직전의 fault callback 및 shutdown recovery 경로에서 source
   EIP와 path를 기록합니다.
3. `REPIU_FAULT_EXIT_TRACE`에 recovery source와 path를 추가하되, 설정이 없으면
   기존 출력과 실행을 유지합니다.
4. core probe에서 초기 상태, guest/cache source, 범위 밖 거부를 검증합니다.
5. Linux x64 `repiu`와 `repiu_core_probe`를 빌드하고 전체 probe를 실행합니다.
6. 실제 실행에서 recovery destination과 원래 source EIP의 연결을 측정하고
   `docs/analysis/linux-port-frontier.md`를 갱신합니다.
7. 작업 로그, `ARCHITECTURE.md` 갱신을 남기고 작업 단위를 커밋합니다.

## English

1. Add bounded recovery provenance state and source/path representation to
   `ThreadContext`.
2. Record the source EIP and path immediately before `RecoverToHost` in both the
   fault callback and shutdown-recovery paths.
3. Extend `REPIU_FAULT_EXIT_TRACE` with recovery source and path while preserving
   existing output and execution when unset.
4. Verify initial state, guest/cache sources, and out-of-range rejection in the
   core probe.
5. Build Linux x64 `repiu` and `repiu_core_probe`, then run the full probe suite.
6. Measure the recovery-destination to original-source association in a real run
   and update `docs/analysis/linux-port-frontier.md`.
7. Leave a work log, update `ARCHITECTURE.md`, and commit the task unit.
