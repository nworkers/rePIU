# Task 695 작업 지시 — Linux x64 guest-entry 범위 trace

raw guest RSP 손상 구간으로 처음 복귀시키는 callback을 찾기 위해 기존 Linux x64
guest-entry trace에 선택적 끝 주소를 추가합니다.

1. [x] 기존 fault의 최초 low-RSP signal과 AOT map을 대조합니다.
2. [x] RF 및 standalone stack-allocation 가설을 live smoke로 배제합니다.
3. [x] inclusive guest-entry trace 범위와 exact fallback을 구현합니다.
4. [x] Linux x64 core probe와 `repiu` target을 빌드합니다.
5. [x] 범위 live trace로 raw guest 전환 callback을 확인합니다.
6. [x] 분석·아키텍처·작업 로그를 갱신하고 작업 단위를 커밋합니다.

## English

Add an optional end address to the existing Linux x64 guest-entry trace so the
callback that first resumes into the raw guest RSP-corruption range can be
identified.

1. [x] Correlate the first low-RSP signal with the AOT map.
2. [x] Exclude the RF and standalone stack-allocation hypotheses with live
   smoke runs.
3. [x] Implement an inclusive guest-entry trace range with exact fallback.
4. [x] Build the Linux x64 core probe and `repiu` target.
5. [x] Identify the raw-guest transition callback with the live range trace.
6. [x] Update analysis, architecture, and the work log, then commit the task
   unit.
