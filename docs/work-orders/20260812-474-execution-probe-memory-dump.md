# 실행 probe 메모리 구간 dump 작업 지시

1. `execution_probe_memory_dump` 전용 header와 source를 추가하고, 환경 변수 파싱·요청
   검증·파일 기록을 그 안에 둡니다.
2. 요청과 결과를 thread context에 보관하고, 설정 시점에 buffer를 한 번 할당합니다.
3. 기존 first-hit recorder에서 base 주소를 계산하고 전체 구간이 읽기 가능할 때만
   memcpy합니다.
4. guest thread 종료 경로에서 한 번만 파일로 기록합니다.
5. telemetry snapshot과 loader 진단에 포착·기록 상태를 전달합니다.
6. Win32 x86 Debug 빌드를 수행합니다.
7. `pumpit8` `+0xE49F8`에서 iCCP chunk 전체를 추출하고, 독립 zlib decoder로 검증합니다.
8. 분석 문서와 작업 로그를 갱신하고 하나의 작업 커밋으로 남깁니다.

# Execution Probe Memory Range Dump Work Order

1. Add a dedicated `execution_probe_memory_dump` header and source holding environment parsing,
   request validation, and file writing.
2. Keep the request and result in the thread context and allocate the buffer once at configuration
   time.
3. Compute the base address in the existing first-hit recorder and memcpy only when the complete
   range is readable.
4. Write the file exactly once on the guest-thread exit path.
5. Carry capture and write status through the telemetry snapshot and loader diagnostics.
6. Run the Win32 x86 Debug build.
7. Extract the complete iCCP chunk at `pumpit8` `+0xE49F8` and validate it with an independent zlib
   decoder.
8. Update the analysis document and work log, and leave one task commit.
