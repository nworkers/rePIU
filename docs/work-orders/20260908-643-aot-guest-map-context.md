# Task 643 작업 지시: AOT guest map 문맥 추적

설계: [20260908-643](../design/20260908-643-aot-guest-map-context.md)

## 한국어

1. 제한된 context radius 환경 변수를 파싱합니다.
2. 일치 entry 앞뒤의 guest/emitted bytes와 metadata를 출력합니다.
3. Linux x64 `repiu`와 core probe를 빌드·검증합니다.
4. 실제 `0x010F1E56` 문맥을 캡처하고 분석 문서를 갱신합니다.
5. 작업 로그를 작성하고 커밋합니다.

## English

1. Parse a bounded context-radius environment setting.
2. Print guest/emitted bytes and metadata around each matched entry.
3. Build and verify Linux x64 `repiu` and the core probe.
4. Capture real context around `0x010F1E56` and update analysis.
5. Write the work log and commit the task.
