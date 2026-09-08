# Task 646 작업 지시: Linux x64 legacy 연속 스택 명령 처리

## 한국어

1. `instruction_emulation`에 지원 중인 일반·segment 스택 명령 연속 처리 helper를 추가합니다.
2. guest code 범위, EIP 전진, 최대 16개 제한으로 fail-closed 조건을 둡니다.
3. x64 `aot_legacy_fallback`의 shared HLE 및 직접 segment fault 경로에서 첫 처리 성공 뒤 helper를 호출합니다.
4. 공용 core probe에 혼합 연속열, 종료 조건, 처리 상한 검증을 추가합니다.
5. Linux x64 `repiu`와 `repiu_core_probe`를 빌드하고 전체 probe를 실행합니다.
6. 실제 게임 경로에서 `PUSH ES`/`PUSH FS` 이후 guest ESP와 다음 frontier를 측정합니다.
7. 분석 문서와 작업 로그에 확인 결과를 반영합니다.

## English

1. Add an `instruction_emulation` helper that consumes consecutive supported general and segment stack instructions.
2. Fail closed on invalid guest code range, lack of EIP progress, or the 16-instruction maximum.
3. Invoke the helper after the first successful operation in the x64 `aot_legacy_fallback` shared-HLE and direct segment-fault paths.
4. Extend the shared core probe with mixed-sequence, termination, and processing-limit coverage.
5. Build Linux x64 `repiu` and `repiu_core_probe`, then run the full probe suite.
6. Measure guest ESP and the next frontier after `PUSH ES`/`PUSH FS` in the real game path.
7. Record confirmed results in the analysis document and work log.
