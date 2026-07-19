# Task 246 작업 지시: zero return-slot 증거 덤프 구현 및 채증 구동

## 작업 항목

1. `HandleAotReturnTransfer`(`src/platform/win32/aot/aot_runtime_dispatch.cpp`)에
   반환 target==0 감지 시 설계
   [20260719-246-zero-return-slot-evidence-dump.md](../design/20260719-246-zero-return-slot-evidence-dump.md)의
   증거 패킷을 stderr로 덤프하는 관측 전용 진단을 추가한다(최초 4회 한정).
2. Win32 x86 Debug 빌드 후 `aot-dynamic` 180초 구동으로 실패를 재현해 증거를
   수집한다.
3. 증거 분석 결과(0 슬롯의 형태, 호출 체인, 코드 창)를 frontier 문서와 작업
   로그에 기록한다. 근인 확정 전 수정은 하지 않는다.

# Task 246 Work Order: Zero Return-Slot Evidence Dump

1. Add the observation-only stderr evidence packet (design 246) to
   `HandleAotReturnTransfer` when the return target reads as zero (first four
   occurrences only).
2. Build Win32 x86 Debug and reproduce the failure with a 180-second
   `aot-dynamic` run.
3. Record the analysis (shape of the zeroed region, tracked call chain, live code
   window) in the frontier document and the work log. No fix before the root
   cause is confirmed.
