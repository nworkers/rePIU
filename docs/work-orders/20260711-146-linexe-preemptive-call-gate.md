# LINEXE 선제 call-gate 작업 지시

1. 공용 bridge far-transfer 직전의 EDI, BX, ESP, EBP와 stack words를 관찰합니다.
2. `LinexeCallGatePlan`으로 `0080:xxxx` target과 서비스를 검증합니다.
3. host far transfer 전에 HLE로 전환하는 공용 handler를 구현합니다.
4. 첫 서비스의 실제 인자·반환 ABI 경계까지 실행하고 다음 의사결정 지점을 기록합니다.
5. Win32 x86 빌드와 supervisor 실행으로 검증합니다.

# LINEXE Preemptive Call-Gate Work Order

Observe the shared bridge frame, validate `0080:xxxx` through `LinexeCallGatePlan`, add a preemptive handler before host far transfer, and run through the first service ABI boundary before documenting the next decision point.
