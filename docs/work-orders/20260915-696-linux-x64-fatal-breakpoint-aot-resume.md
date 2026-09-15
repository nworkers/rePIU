# Task 696 작업 지시 — Linux x64 fatal breakpoint AOT 재진입

원본 fatal breakpoint 처리 후의 `PUSH/CALL/HLT` tail을 Linux x64 AOT cache로
재진입시켜 guest ESP lowering을 유지합니다.

1. [x] Task 695 trace와 기존 fatal-tail 아키텍처 계약을 대조합니다.
2. [x] 일반 pending 재진입과 처리된 guest 경계 재진입을 구분하는 정책을 추가합니다.
3. [x] fatal breakpoint가 진행시킨 continuation을 AOT resume에 연결합니다.
4. [x] x64 비동일 continuation의 재진입 실패를 fail-closed 처리합니다.
5. [x] Linux x64 core probe와 `repiu`를 빌드합니다.
6. [x] `pumpit2a` live trace로 raw continuation과 RSP 손상 재발 여부를 확인합니다.
7. [x] 분석·아키텍처·작업 로그를 갱신하고 작업 단위를 커밋합니다.

## English

Resume the original fatal breakpoint's `PUSH/CALL/HLT` tail through the Linux
x64 AOT cache so guest-ESP lowering remains active.

1. [x] Correlate the Task 695 trace with the existing fatal-tail architecture
   contract.
2. [x] Add a policy that distinguishes ordinary pending re-entry from a handled
   guest-boundary re-entry.
3. [x] Connect the fatal breakpoint continuation to AOT resume.
4. [x] Fail closed when an x64 non-identical continuation cannot re-enter.
5. [x] Build the Linux x64 core probe and `repiu`.
6. [x] Use a `pumpit2a` live trace to check raw continuation and RSP-corruption
   recurrence.
7. [x] Update analysis, architecture, and the work log, then commit the task.
