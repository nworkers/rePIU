# Native linear span #DB 취소 원인 계측 / Attributing native linear-span #DB cancellations

Task 378. 근거: [current execution frontier](../analysis/current-execution-frontier.md), Task 377 gameplay capture.

## 설계

music-select 캡처에서 native linear span은 6,461회 진입했지만 hardware-breakpoint 경계 도달은 31회이고, `EXCEPTION_SINGLE_STEP` 취소는 6,430회였습니다. span 진입은 TF를 지우고 경계에 Dr0만 설정하지만, 기존 로그만으로는 #DB가 TF, 다른 debug-register slot, 또는 다른 원인 중 무엇인지 알 수 없습니다.

이 작업은 guest 코드나 span 실행 정책을 바꾸지 않습니다. 예상된 Dr0 경계는 기존처럼 boundary로 처리하고, 예상 밖 `EXCEPTION_SINGLE_STEP` 취소만 DR6 우선순위(Dr0, Dr1, Dr2, Dr3, BS/TF, 기타)로 상호 배타적으로 분류합니다. 각 원인마다 첫 EIP와 카운터만 기록하며, clock read, 새 예외, 제어 흐름 변경을 추가하지 않습니다.

### 결정 기준

`NativeFastPathState`에 원인별 카운터와 첫 EIP를 둡니다. `LeaveNativeLinearSpan`이 DR6을 복원하기 전에 캡처하고, snapshot과 종료 요약으로 노출합니다. 하나의 원인이 취소의 70% 이상이면 다음 작업에서 해당 debug-register/TF 소유권 수정 후보를 검토합니다. 원인이 분산되면 기존 fail-closed 동작을 유지하고 native span 축은 종료합니다.

## English

In the music-select capture, native linear spans entered 6,461 times but reached their hardware-breakpoint boundary only 31 times, while 6,430 `EXCEPTION_SINGLE_STEP` cancellations occurred. Span entry clears TF and sets only Dr0 for its boundary, so the old log cannot determine whether #DB comes from TF, another debug-register slot, or another cause.

This task changes neither guest code nor span execution policy. Expected Dr0 boundaries remain boundary completions. Only unexpected `EXCEPTION_SINGLE_STEP` cancellations are classified mutually exclusively by DR6 precedence (Dr0, Dr1, Dr2, Dr3, BS/TF, other). The code records only a counter and first EIP per cause; it adds no clock read, exception, or control-flow change.

### Decision gate

`NativeFastPathState` holds the cause counters and first EIPs. `LeaveNativeLinearSpan` captures DR6 before restoration, and the snapshot/final summary exposes it. A cause accounting for at least 70% of cancellations makes its debug-register/TF ownership a candidate for the next task. A dispersed result retains the current fail-closed behavior and closes the native-span axis.