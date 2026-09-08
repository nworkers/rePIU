# Task 639 작업 지시: long-mode fallthrough 경계 복구

설계: [20260908-639](../design/20260908-639-long-mode-fallthrough-boundary-recovery.md)

## 한국어

1. 미해결 `kBlockFallthrough` sentinel과 guest target을 연결하는 공용 조회 함수를 추가합니다.
2. AOT reentry가 이 target을 기존 TF 원본 실행 경로로 전달하게 합니다.
3. sentinel provenance를 `kOtherPlannerFixup`으로 색인합니다.
4. synthetic probe로 exact-match와 거절 조건을 검증합니다.
5. Linux x64 빌드, core probe, 실제 `pumpit2a` 반복 실행을 수행합니다.
6. 분석, 아키텍처, 작업 로그에 결과와 새 frontier를 기록합니다.

## English

1. Add a shared lookup connecting an unresolved `kBlockFallthrough` sentinel
   to its guest target.
2. Feed that target through the existing TF original-execution AOT reentry path.
3. Index the sentinel provenance as `kOtherPlannerFixup`.
4. Verify exact matching and rejection cases with a synthetic probe.
5. Build Linux x64, run the core probe, and repeat real `pumpit2a` runs.
6. Record the result and new frontier in analysis, architecture, and work log.
