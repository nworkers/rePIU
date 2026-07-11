# 원본 fatal tail 계속 실행 작업 지시

1. breakpoint와 fatal-tail signature를 제한적으로 판별한다.
2. breakpoint 주소, message 주소와 문자열을 execution attempt에 기록한다.
3. 원본 `push edx; call error_printer`를 재개한다.
4. 연결된 `HLT`를 fatal 종료로 회수한다.
5. 빌드, hello 회귀, 장시간 PIU 실행과 실제 메시지 출력을 검증한다.
6. 분석 문서와 작업 로그를 갱신하고 커밋한다.

# Original Fatal-Tail Continuation Work Order

Recognize the breakpoint and fatal-tail signature narrowly, record its address and message in the execution attempt, resume the original `push edx; call error_printer`, collect its terminal `HLT` as a fatal exit, validate the build, hello regression, extended PIU execution and actual message output, then update analysis and work-log documentation and commit.
