# Task 641 작업 지시: AOT 전이 target 출처 추적

설계: [20260908-641](../design/20260908-641-aot-transfer-target-origin-trace.md)

## 한국어

1. target 주소를 파싱하는 opt-in trace 필터를 추가합니다.
2. 간접 CALL/JMP와 RET target 해석 직후에 종류별 증거를 기록합니다.
3. Linux x64 `repiu`와 core probe를 빌드·검증합니다.
4. 실제 `pumpit2a`에서 `0x011A8E10`의 최초 전이 출처를 캡처합니다.
5. 분석 문서와 작업 로그를 갱신하고 커밋합니다.

## English

1. Add an opt-in trace filter that parses a target address.
2. Record kind-specific evidence immediately after indirect CALL/JMP and RET target decoding.
3. Build and verify Linux x64 `repiu` and the core probe.
4. Capture the first transfer source for `0x011A8E10` in real `pumpit2a` execution.
5. Update the analysis and work log, then commit the task.
