# 연속 segment load와 REP STOSD 작업 지시

1. 인접한 지원 segment load를 한 dispatch에서 처리한다.
2. 복원된 shadow ES trace를 검증한다.
3. 관찰된 EAX=0, DF=0 `REP STOSD`를 guest range 검증 후 일괄 처리한다.
4. register와 EIP를 x86 의미에 맞게 갱신한다.
5. 빌드, hello sample, PIU 반복 실행을 검증한다.
6. 새 frontier와 문서를 갱신하고 커밋한다.

# Adjacent Segment Loads and REP STOSD Work Order

Consume adjacent supported segment loads in one dispatch, verify restored shadow ES, batch the observed EAX-zero/DF-clear REP STOSD after guest-range validation, update registers and EIP with x86 semantics, validate the build/hello/PIU paths, document the next frontier, and commit.
