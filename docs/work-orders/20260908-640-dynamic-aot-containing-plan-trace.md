# Task 640 작업 지시: 특정 주소를 포함하는 동적 AOT plan 추적

설계: [20260908-640](../design/20260908-640-dynamic-aot-containing-plan-trace.md)

## 한국어

1. `REPIU_AOT_DYNAMIC_CONTAINS` 주소 parser와 image 포함 검사를 추가합니다.
2. 일치 append의 요청 entry, block/instruction 위치와 관련 fixup을 출력합니다.
3. 환경 변수 미설정 기본 경로와 실행 제어는 유지합니다.
4. Linux x64 빌드와 실제 `pumpit2a` 캡처를 수행합니다.
5. core probe 및 diff 검증 후 분석과 작업 로그를 갱신합니다.

## English

1. Add the `REPIU_AOT_DYNAMIC_CONTAINS` address parser and image-membership test.
2. Print the matching append's request entry, block/instruction position, and related fixups.
3. Preserve the unset default path and execution control.
4. Build Linux x64 and capture real `pumpit2a` evidence.
5. Run the core probe and diff checks, then update analysis and the work log.
