# Task 702 작업 지시 — Linux x64 Glide gate 반환 AOT 재진입

1. [x] 실제 gate stack과 원본 호출부 bytes를 비교해 null 인자 원인을 확인한다.
2. [x] Glide gate 반환의 Linux x64 AOT 재진입 설계를 작성한다.
3. [x] 조기 Glide gate 경로에 handled-boundary AOT 재진입을 연결한다.
4. [x] 재진입 실패의 long-mode fail-closed 처리를 적용한다.
5. [x] Linux x64 core probe와 `repiu`를 빌드한다.
6. [x] 실제 `pumpit2a`에서 query 인자, 후속 gate와 새 frontier를 확인한다.
7. [x] 분석·아키텍처·작업 로그를 갱신하고 커밋한다.

## English

1. [x] Compare the real gate stack with original caller bytes and identify the
   null-argument cause.
2. [x] Design Linux x64 AOT re-entry after a Glide gate return.
3. [x] Connect handled-boundary AOT re-entry to the early Glide-gate path.
4. [x] Apply long-mode fail-closed handling when re-entry fails.
5. [x] Build the Linux x64 core probe and `repiu`.
6. [x] Verify the query argument, later gates, and next frontier in real
   `pumpit2a`.
7. [x] Update analysis, architecture, and the work log, then commit.
