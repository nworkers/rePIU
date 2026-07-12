# Native return fast path 작업 지시 / Work Order

## 한국어

1. signature 검증형 함수 fast path 상태와 관찰 counter를 추가한다.
2. 진입 시 반환 주소 hardware breakpoint를 설치하고 Trap Flag를 해제한다.
3. 정상 반환 또는 중간 예외에서 debug register와 single-step 상태를 복원한다.
4. Win32 x86 Debug를 빌드하고 PIU 실행 처리량과 다음 frontier를 비교한다.
5. architecture, analysis 및 작업 로그를 갱신한다.

## English

1. Add signature-verified function-fast-path state and observation counters.
2. Install a return-address hardware breakpoint and clear Trap Flag on entry.
3. Restore debug registers and single-step state on normal return or intermediate exception.
4. Build Win32 x86 Debug and compare PIU throughput and its next frontier.
5. Update architecture, analysis, and the work log.
