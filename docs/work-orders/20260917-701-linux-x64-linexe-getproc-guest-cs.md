# Task 701 작업 지시 — Linux x64 LINEXE GETPROCADDR guest CS

1. [x] 실제 종료 로그와 기존 LINEXE 분석에서 GETPROCADDR frame을 확인한다.
2. [x] Linux x64 물리 CS와 guest code selector의 차이를 확인한다.
3. [ ] GETPROCADDR 결과 selector를 selector table 기반 guest CS로 변경한다.
4. [ ] synthetic 성공 및 fail-closed probe를 추가한다.
5. [ ] Linux x64 core probe와 `repiu`를 빌드한다.
6. [ ] 실제 `pumpit2a`에서 DLL entry-point fatal 제거와 다음 frontier를 확인한다.
7. [ ] 분석·아키텍처·작업 로그를 갱신하고 커밋한다.

## English

1. [x] Confirm the GETPROCADDR frame from the real termination log and existing
   LINEXE analysis.
2. [x] Confirm the difference between Linux x64 physical CS and the guest code
   selector.
3. [ ] Resolve the GETPROCADDR result selector as guest CS through the selector
   table.
4. [ ] Add synthetic success and fail-closed probes.
5. [ ] Build the Linux x64 core probe and `repiu`.
6. [ ] Verify removal of the DLL entry-point fatal and identify the next real
   frontier.
7. [ ] Update analysis, architecture, and the work log, then commit.
