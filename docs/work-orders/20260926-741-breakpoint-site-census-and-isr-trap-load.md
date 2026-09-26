# Task 741: breakpoint 지점 census와 pumpitea trap 부하 작업 지시

설계: [20260926-741](../design/20260926-741-breakpoint-site-census-and-isr-trap-load.md)

## 한국어

1. `ThreadContext`에 32칸 breakpoint 지점 census(host/guest 주소, cache 여부, 횟수, exit site 4종)를
   두고 `RecordVehExceptionCensus`와 `NoteVehExitSite`에서 채운다.
2. live telemetry가 최종 보고용 attempt에 복사하고, 로더가 상위 16개를 찍는다.
3. Linux x64 Release로 pumpitea attract 30초를 측정해 상위 지점을 디스어셈블로 확정한다.
4. 확정된 원인에 맞는 수정을 넣고 같은 측정으로 breakpoint 수와 fps를 비교한다.
5. 설계·작업 로그·analysis를 갱신하고 커밋한다.

## English

1. Give `ThreadContext` a 32-slot breakpoint site census (host/guest address, cache flag, count,
   four exit sites) filled from `RecordVehExceptionCensus` and `NoteVehExitSite`.
2. Copy it into the final-report attempt from the live telemetry and print the top 16 in the loader.
3. Measure pumpitea's 30 s attract on Linux x64 Release and settle the top sites by disassembly.
4. Apply the fix the measurement names and compare breakpoint count and fps with the same run.
5. Update the design, work log and analysis, then commit.
