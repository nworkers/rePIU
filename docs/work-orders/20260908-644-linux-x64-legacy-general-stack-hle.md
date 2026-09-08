# Task 644 작업 지시: Linux x64 legacy 일반 스택 명령 HLE

## 한국어

1. `instruction_emulation`에 `PUSH/POP r32` guest-stack handler를 추가합니다.
2. x64 `aot_legacy_fallback`의 shared HLE dispatch에서만 handler를 호출합니다.
3. 공용 core probe에 일반 동작, ESP 특수 동작, 범위 거부 검증을 추가합니다.
4. Linux x64 `repiu`와 `repiu_core_probe`를 빌드하고 probe를 실행합니다.
5. 실제 게임 경로를 재실행하여 prologue ESP와 다음 frontier를 확인합니다.
6. 분석 문서와 작업 로그에 확인된 결과를 반영합니다.

## English

1. Add a guest-stack `PUSH/POP r32` handler to `instruction_emulation`.
2. Invoke it only from shared HLE dispatch during x64
   `aot_legacy_fallback`.
3. Add shared core-probe coverage for ordinary behavior, ESP special cases,
   and range rejection.
4. Build Linux x64 `repiu` and `repiu_core_probe`, then run the probes.
5. Rerun the real-game path to inspect prologue ESP and the next frontier.
6. Record confirmed results in the analysis document and work log.
