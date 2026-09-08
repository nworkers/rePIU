# Task 645 작업 지시: direct-edge fallback 진입 출처 추적

## 한국어

1. direct-edge fallback 역조회가 선택적으로 guest source도 반환하게 합니다.
2. breakpoint 역조회 세 경로를 구분하여 기존 target-filter 진단에
   source/bytes/cache/ESP 출력을 추가합니다.
3. direct-edge probe에 source 반환 검증을 추가합니다.
4. segment HLE trace에 처리 직후 guest bytes를 추가합니다.
5. Linux x64 빌드와 core probe를 실행합니다.
6. 실제 `pumpit2a`에서 target `0x010F1D74`의 source 명령을 캡처합니다.
7. 분석 문서와 작업 로그에 결과를 반영합니다.

## English

1. Let direct-edge fallback reverse lookup optionally return the guest source.
2. Distinguish the three breakpoint reverse-lookup paths and extend the
   existing target-filter diagnostic with source, bytes, cache address, and ESP.
3. Verify source recovery in the direct-edge probe.
4. Add post-handler guest bytes to the segment HLE trace.
5. Run the Linux x64 build and core probe.
6. Capture the source instruction for target `0x010F1D74` in real `pumpit2a`.
7. Update the analysis document and work log with the result.
