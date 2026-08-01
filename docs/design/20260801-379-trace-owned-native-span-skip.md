# Single-step 추적 소유 native span 건너뛰기 / Skip native spans owned by single-step tracing

Task 379. 근거: Task 378의 music-select 캡처, [current execution frontier](../analysis/current-execution-frontier.md).

## 설계

Task 378 캡처에서 일반 native linear span의 예상 밖 취소 6,021건은 모두 DR6 BS/TF 원인이었습니다. 일반 span 진입 호출은 `HandleSingleStepTrace` 안에 있고, 이 함수는 `enable_single_step_trace`가 참일 때만 실행됩니다. span이 TF를 일시적으로 지워도, 처리 함수의 기존 끝부분은 다음 AOT-DBT 재진입을 위해 TF를 다시 설정합니다. 따라서 일반 span은 정상 Dr0 경계에 도달할 수 없는 시도입니다.

일반 native linear span 호출에 `!context->enable_single_step_trace` 조건을 추가합니다. 이 조건이 거짓이면 기존 single-step 처리와 TF 재설정으로 그대로 진행하며, scan, Dr0/Dr7 저장·설정, 이후 TF 취소만 생략합니다. `TryEnterRetiredTrapNativeSpan` 및 그 정책은 변경하지 않습니다.

## 검증 기준

interval-zero music-select 캡처에서 일반 span의 entry/cancel이 0이어야 하며, AOT 재진입·예외 처리·화면 동작은 유지되어야 합니다. 성능 변화는 같은 구간의 wall time과 FPS로만 판단합니다.

## English

Task 378's music-select capture attributed all 6,021 unexpected ordinary native-linear-span cancellations to DR6 BS/TF. The ordinary span entry call is inside `HandleSingleStepTrace`, which runs only when `enable_single_step_trace` is true. Even if a span temporarily clears TF, the existing end of that handler re-arms TF for the next AOT-DBT re-entry. The ordinary span therefore cannot reach its intended Dr0 boundary.

Add `!context->enable_single_step_trace` to the ordinary native-linear-span entry condition. When false, the existing single-step processing and TF re-arm proceed unchanged; only the scan, Dr0/Dr7 save-and-set, and subsequent TF cancellation are skipped. `TryEnterRetiredTrapNativeSpan` and its policy remain unchanged.

## Verification

An interval-zero music-select capture must show zero ordinary span entries and cancellations while retaining AOT re-entry, exception handling, and screen behavior. Assess any performance change only from the same-scene wall time and FPS.