# AOT 조건 분기 dispatcher 작업 지시

1. code-cache의 표준 Jcc를 guest dispatcher sentinel으로 발행합니다.
2. short/near Jcc의 조건을 EFLAGS에서 판정합니다.
3. 선택된 taken/fallthrough 게스트 주소를 기존 cache/dynamic resolver로 연결합니다.
4. Win32 빌드와 PIU 제한 시간 관찰을 수행합니다.

# AOT Conditional Transfer Dispatcher Work Order

1. Emit standard Jcc instructions in the code cache as guest-dispatch sentinels.
2. Evaluate short/near Jcc conditions from EFLAGS.
3. Connect the selected taken/fallthrough guest address to the existing cache/dynamic resolver.
4. Build on Win32 and run a bounded PIU observation.
