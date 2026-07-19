# 간접 원거리 LINEXE 호출 경계 작업 지시 / Indirect Far LINEXE Call Boundary Work Order

## 작업 / Work

1. `FF /3` absolute-memory indirect far call을 LINEXE boundary에서 식별합니다.
2. guest image의 6-byte far pointer를 읽어 offset과 selector를 기록합니다.
3. 확인된 LINEXE export 여부만 기록하고, far-call ABI를 추측하지 않아 dispatcher는 호출하지 않습니다.
4. 실행 attempt 출력에 관측 결과를 추가합니다.
5. Win32 x86 빌드와 대상 실행으로 검증합니다.

1. Identify `FF /3` absolute-memory indirect far calls at the LINEXE boundary.
2. Read the six-byte guest far pointer and record its offset and selector.
3. Record whether the target is a confirmed LINEXE export, without dispatching before the far-call ABI is confirmed.
4. Add the observation to execution-attempt output.
5. Verify with a Win32 x86 build and target execution.
