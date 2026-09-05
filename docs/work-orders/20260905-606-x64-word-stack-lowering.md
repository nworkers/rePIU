# Task 606: 구현 계획 / Work order

## 한국어

1. 기존 stack lowering에 `66 PUSH/POP r16`을 추가한다.
2. 실행 probe로 폭·상위 register·flags·인접 stack·SP 특례를 검증한다.
3. Linux x64 `repiu`, `repiu_core_probe`, register probe를 빌드하고 실행한다.
4. 기본 `pumpit2a`를 제한 시간으로 재실행해 반환주소와 다음 오류 지점을 확인한다.
5. Task 605 결론을 정정하고 분석·아키텍처·작업 로그를 갱신해 커밋한다.

## English

1. Add `66 PUSH/POP r16` to the existing stack lowering.
2. Execute width, upper-register, flag, adjacent-stack, and SP checks.
3. Build and run Linux x64 `repiu`, core probe, and register probe.
4. Run default `pumpit2a` with a timeout to check the return address and next fault.
5. Correct Task 605, update analysis, architecture, and work log, and commit.
