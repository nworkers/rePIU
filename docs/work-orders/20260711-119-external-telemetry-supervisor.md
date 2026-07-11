# 외부 telemetry supervisor 작업 지시

1. 공유 telemetry POD와 mapping 이름 환경 계약을 정의한다.
2. loader host/guest dispatch가 interlocked shared field를 갱신한다.
3. Win32 supervisor executable을 추가한다.
4. supervisor가 child 실행, 주기 snapshot, timeout terminate/join을 수행한다.
5. 빌드, hello, PIU bounded 실행을 검증한다.
6. 회수된 마지막 heartbeat/EIP로 다음 frontier를 판단한다.
7. 문서와 로그를 갱신하고 커밋한다.

# External Telemetry Supervisor Work Order

Define the shared telemetry POD and environment contract, update interlocked fields from loader host/guest dispatch, add a Win32 supervisor executable, launch/poll/terminate/join the child, validate build/hello/bounded PIU execution, classify the final heartbeat/EIP, update documentation and logs, and commit.
