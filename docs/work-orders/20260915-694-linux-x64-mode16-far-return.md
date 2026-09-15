# Task 694 작업 지시

object 3의 mode16 bare `RETF`를 SS-relative 16-bit frame HLE로 연결합니다.

1. [x] SS descriptor 기반 공용 stack read geometry와 mode16 far-return resolver를
   설계대로 추가합니다.
2. [x] shared/fault HLE dispatch에 prefix 없는 mode16 `CB`만 연결하고 fail-closed
   probe를 추가합니다.
3. [x] Linux x64 core probe, `repiu` 빌드, `pumpit2a` trace를 실행합니다.
4. [x] 분석·아키텍처·작업 로그를 갱신하고 작업 단위를 커밋합니다.

## English

Connect object-3 mode16 bare `RETF` through an SS-relative 16-bit frame HLE.

1. [x] Add the designed SS-descriptor stack-read geometry and mode16 far-return
   resolver.
2. [x] Connect only prefix-free mode16 `CB` to shared/fault HLE dispatch and add
   fail-closed probe coverage.
3. [x] Run the Linux x64 core probe, `repiu` build, and `pumpit2a` trace.
4. [x] Update analysis, architecture, and the work log, then commit this task unit.
