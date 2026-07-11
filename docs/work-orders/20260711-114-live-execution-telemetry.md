# 동일 프로세스 live execution telemetry 작업 지시

1. guest/host 공유 atomic phase와 heartbeat를 추가한다.
2. exception dispatch 진입과 종료를 기록한다.
3. host poll 시작 및 1초 주기 snapshot을 stderr로 출력한다.
4. 기존 timeout과 정상 실행 동작을 보존한다.
5. Win32 x86 빌드와 hello sample을 검증한다.
6. PIU를 외부 bounded wrapper로 실행해 마지막 live snapshot을 분석한다.
7. 결과에 따라 동일 프로세스 진단을 계속할지 supervisor 방식으로 전환할지 결정한다.

# In-Process Live Execution Telemetry Work Order

Add shared atomic phase and heartbeat state, instrument exception dispatch, emit immediate and once-per-second host poll snapshots to stderr, preserve existing execution behavior, validate the Win32 x86 build and hello sample, run PIU under an external bounded wrapper, and decide from evidence whether an external supervisor is required.
