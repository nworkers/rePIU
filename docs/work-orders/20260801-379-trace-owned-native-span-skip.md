# 작업 지시: Single-step 추적 소유 native span 건너뛰기 / Work order: skip native spans owned by single-step tracing

Task 379. 설계: [20260801-379](../design/20260801-379-trace-owned-native-span-skip.md)

## 작업 내용

1. 일반 native linear span의 단일 단계 처리 진입 조건에 trace-ownership guard를 추가합니다.
2. retired-trap native span 및 기존 TF 재설정·fallback 흐름은 변경하지 않습니다.
3. 정적 점검과 Win32 x86 Release 빌드를 시도합니다.
4. interval-zero music-select 캡처에서 span entry/cancel 0, 정상 화면, 성능 수치를 확인합니다.

## English

1. Add a trace-ownership guard to the ordinary native-linear-span entry condition in single-step handling.
2. Do not change retired-trap native spans or existing TF re-arm/fallback flow.
3. Run static checks and attempt a Win32 x86 Release build.
4. Confirm zero ordinary span entries/cancellations, normal visuals, and performance metrics in an interval-zero music-select capture.