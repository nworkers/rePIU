# Task 700 작업 지시 — Linux x64 CS override 간접 점프 경계

1. [x] `0x010F777C`의 raw guest bytes와 기존 오판 원인을 확인한다.
2. [x] 설계를 CS override 간접 점프 경계 처리로 정정한다.
3. [x] 재진입 분류와 기존 간접 전송 decoder에 `2E` 접두사 지원을 추가한다.
4. [x] 실제 명령 형식의 synthetic probe를 추가한다.
5. [x] Linux x64 core probe와 `repiu`를 빌드한다.
6. [x] 실제 `pumpit2a`에서 기존 경계 통과와 다음 frontier를 확인한다.
7. [x] 분석·아키텍처·작업 로그를 갱신하고 커밋한다.

## English

1. [x] Confirm the raw guest bytes at `0x010F777C` and the cause of the earlier
   misclassification.
2. [x] Correct the design to handle a CS-override indirect-jump boundary.
3. [x] Add `2E` prefix support to reentry classification and the existing
   indirect-transfer decoder.
4. [x] Add a synthetic probe using the observed instruction form.
5. [x] Build the Linux x64 core probe and `repiu`.
6. [x] Verify that real `pumpit2a` passes the former boundary and identify the
   next frontier.
7. [x] Update analysis, architecture, and the work log, then commit.
