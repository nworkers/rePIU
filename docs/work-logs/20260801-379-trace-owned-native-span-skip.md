# 작업 로그: Single-step 추적 소유 native span 건너뛰기 / Work log: skip native spans owned by single-step tracing

## 결과

- `HandleSingleStepTrace`의 일반 native linear span 진입 조건에 `!context->enable_single_step_trace` guard를 추가했습니다.
- 이 처리 함수는 trace가 활성일 때만 호출되므로, Task 378에서 확인된 항상-취소 span 시도를 제거합니다.
- 기존 single-step TF 재설정, AOT-DBT 재진입, fallback, retired-trap native span은 변경하지 않았습니다.
- `git diff --check`를 통과했습니다.
- 사용자 빌드 후 2026-08-01 music-select interval-zero 캡처로 runtime 검증했습니다. 일반 span `entry/boundary/cancel`과 #DB 원인 분포는 모두 0이었고, AOT 재진입은 `667,659`회로 계속 동작했습니다.
- 이 캡처는 33.609초에 `_GRBUFFERSWAP@4` 1,194회(약 35.5 FPS)였습니다. 이전 32.766초·1,246회(약 38.0 FPS) 캡처와 실행량이 달라 성능 향상을 주장할 수 없으며, 이 작업은 성공 불가능한 span의 비용 제거로 종료합니다.

## English

- Added `!context->enable_single_step_trace` to the ordinary native-linear-span entry condition in `HandleSingleStepTrace`.
- Because this handler is called only while tracing is enabled, it removes the always-cancelled span attempts confirmed in Task 378.
- Existing single-step TF re-arm, AOT-DBT re-entry, fallback, and retired-trap native spans are unchanged.
- `git diff --check` passed.
- A user build followed by the 2026-08-01 interval-zero music-select capture verified runtime behavior. Ordinary span `entry/boundary/cancel` and its #DB cause distribution were all zero, while AOT re-entry continued 667,659 times.
- This capture recorded 1,194 `_GRBUFFERSWAP@4` calls in 33.609 seconds (about 35.5 FPS). Its workload differs from the previous 1,246 calls in 32.766 seconds (about 38.0 FPS), so it does not support a performance-improvement claim. This task ends after removing the cost of spans that cannot succeed.