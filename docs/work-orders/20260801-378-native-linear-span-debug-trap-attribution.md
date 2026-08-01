# 작업 지시: Native linear span #DB 취소 원인 계측 / Work order: native linear-span #DB cancellation attribution

Task 378. 설계: [20260801-378](../design/20260801-378-native-linear-span-debug-trap-attribution.md)

## 작업 내용

1. `NativeFastPathState`에 상호 배타적인 #DB 취소 원인별 카운터와 첫 EIP를 추가합니다.
2. 예상된 Dr0 경계 처리는 보존하고, `LeaveNativeLinearSpan`에서 예상 밖 #DB만 DR6/BS로 분류합니다.
3. snapshot과 종료 로그에 계측값을 연결합니다.
4. 정적 점검과 Win32 x86 Release 빌드를 시도하고, music-select interval-zero 캡처로 70% 판단 기준을 검증합니다.

범위 밖: guest span 실행, TF/Dr 복원 순서, fallback 정책의 동작 변경.

## English

1. Add mutually exclusive #DB cancellation-cause counters and first EIPs to `NativeFastPathState`.
2. Preserve expected Dr0 boundary handling and classify only unexpected #DBs in `LeaveNativeLinearSpan` using DR6/BS.
3. Wire the values into the snapshot and final log.
4. Run static checks and attempt a Win32 x86 Release build, then validate the 70% decision gate with an interval-zero music-select capture.

Out of scope: changing guest span execution, TF/Dr restoration order, or fallback policy.