# Task 699 작업 지시

1. [x] 초기 map sentinel miss와 환경 변수 configuration을 분리합니다.
2. [x] dynamic append의 기존 sentinel 설치 경로가 armed probe를 소비하게 유지합니다.
3. [x] x64 compatibility 분기 전에 역매핑된 probe snapshot을 기록합니다.
4. [ ] 실제 `0x010F928B` probe로 register snapshot을 수집합니다.
   `0x010F777C` planner-HLE fail-closed 경계를 먼저 해결해야 합니다.
5. [x] Linux x64 빌드와 core probe를 검증합니다.
6. [x] 분석 및 작업 로그를 갱신하고 진행 상태를 커밋합니다.

## English

1. [x] Separate an initial-map sentinel miss from environment configuration.
2. [x] Keep the existing dynamic-append sentinel path consuming an armed probe.
3. [x] Record the reverse-mapped probe snapshot before x64 compatibility routing.
4. [ ] Capture a register snapshot with a real `0x010F928B` probe. The
   planner-HLE fail-closed boundary at `0x010F777C` must be resolved first.
5. [x] Verify the Linux x64 build and core probe.
6. [x] Update analysis and the work log, then commit the progress.
