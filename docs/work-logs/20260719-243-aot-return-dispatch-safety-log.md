# Task 243 AOT return-stack safety log

## 결과

저장 레지스터 epilogue의 AOT zero-EIP 종료를 dispatcher guard로 차단했습니다. Win32 x86 Debug 빌드는 성공했고 180초 AOT 실행에서 기존 `0x0304ED35` 종료는 재현되지 않았습니다.

다만 고빈도 epilogue 호출에서 return dispatcher/reentry가 급증하는 liveness frontier가 남아 있습니다. legacy/trap backend는 같은 180초 동안 계속 진행하므로 Glide HLE가 아닌 동적 AOT return-cache 수명주기 문제입니다.

## 검증

- `cmd /c scripts\build_win32_x86.bat`
- `REPIU_EXECUTION_BACKEND=aot-dynamic`, supervisor 180000 ms
- legacy/trap, supervisor 180000 ms

# Task 243 AOT Return-Stack Safety Log

The dispatcher guard removes the saved-register epilogue zero-EIP termination. The Win32 x86 Debug build succeeds and the former `0x0304ED35` failure does not recur in a 180-second AOT run.

A high-frequency return dispatcher/re-entry liveness frontier remains. The legacy/trap backend continues during the same 180-second run, so the remaining issue is dynamic AOT return-cache lifetime rather than Glide HLE.
