# 실행 probe 레지스터 메모리 창 작업 지시

1. 실행 attempt와 thread context에 고정 크기 레지스터 메모리 창 자료 구조를 추가합니다.
2. 선택적인 공통 memory offset을 파싱하고 기존 first-hit recorder에서 읽기 가능한
   32바이트만 복사합니다.
3. telemetry snapshot으로 결과를 전달하고 loader 종료 진단에 16진수로 출력합니다.
4. Win32 x86 Debug 빌드와 probe를 수행합니다.
5. `pumpit8` iCCP 함수 진입·반환 지점을 재현하고 분석 문서와 작업 로그를 갱신합니다.
6. 관련 변경을 하나의 작업 커밋으로 남깁니다.

# Execution Probe Register Memory Window Work Order

1. Add fixed-size register memory-window data to the execution attempt and thread context.
2. Parse an optional common memory offset and copy only a fully readable 32-byte range in the
   existing first-hit recorder.
3. Carry the result through the telemetry snapshot and print it as hexadecimal loader diagnostics.
4. Run the Win32 x86 Debug build and probes.
5. Reproduce the `pumpit8` iCCP entry and return sites, then update analysis and the work log.
6. Commit the related changes as one task commit.
