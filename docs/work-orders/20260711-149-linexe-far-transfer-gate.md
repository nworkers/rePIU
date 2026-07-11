# LINEXE 원거리 전이 게이트 작업 지시

1. 원본 export offset을 서비스로 해석하는 플랫폼 공용 API를 추가합니다.
2. `66 EA` 전이에서 target, register, stack을 주소 비의존적으로 관찰합니다.
3. 실행하여 임시 bridge frame과 첫 `LOADMODULE` 인자를 복원합니다.
4. 증거에 맞춰 선제 HLE와 공용 epilogue 복귀를 구현합니다.
5. Win32 x86 빌드와 실행으로 다음 경계를 확인하고 분석·작업 로그를 갱신합니다.

# LINEXE Far-Transfer Gate Work Order

Add a platform-neutral original-export decoder, observe target/register/stack state at generic `66 EA` transfers, recover the temporary bridge frame and first `LOADMODULE` argument, implement evidence-based preemptive HLE return to the shared epilogue, validate with the Win32 x86 build/runtime, and update analysis and work-log documentation.
