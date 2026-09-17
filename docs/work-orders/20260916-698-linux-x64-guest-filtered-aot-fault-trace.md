# Task 698 작업 지시

1. [x] guest-address 환경 변수 parser를 기존 cache-address filter 옆에 추가합니다.
2. [x] fault 순간의 exact/previous/fallthrough reverse map을 계산한 뒤 guest filter를
   적용합니다.
3. [x] 기존 cache filter, 16건 제한, 출력 형식을 유지합니다.
4. [x] Linux x64 빌드, core probe, bounded 실제 실행을 검증합니다.
5. [x] 분석과 작업 로그를 갱신하고 커밋합니다.

## English

1. [x] Add a guest-address environment parser beside the existing cache-address
   filter.
2. [x] Apply the guest filter after computing exact, previous, and fallthrough
   reverse maps at the fault instant.
3. [x] Preserve the existing cache filter, 16-line limit, and output format.
4. [x] Verify the Linux x64 build, core probe, and a bounded real run.
5. [x] Update analysis and the work log, then commit the task.
