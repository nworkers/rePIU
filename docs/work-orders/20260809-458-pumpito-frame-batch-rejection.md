# 20260809-458 pumpito frame batch 거부 진단 작업 지시 / Pumpito Frame-Batch Rejection Diagnostics Work Order

설계: [20260809-458-pumpito-frame-batch-rejection.md](../design/20260809-458-pumpito-frame-batch-rejection.md)

## 한국어

- [x] 최신 사용자 로그에서 `batched=0`, 정확한 EIP와 byte fast path 활성화를 확인합니다.
- [x] 정적 `PIU.EXE`에서 대상 opcode 서명이 일치함을 확인합니다.
- [x] fail-closed 거부 단계별 최초 진단을 추가합니다.
- [x] Win32 x86 Debug build와 PIU10 probe를 실행합니다.
- [x] 제한 `pumpito` 실행으로 실제 거부 조건을 확보합니다.
- [x] 원본 loop 의미를 유지하며 원인을 교정하고 regression probe를 추가합니다.
- [x] build, probe와 제한 runtime에서 일괄 활성화를 재검증합니다.
- [x] 누적 분석과 작업 로그를 갱신하고 커밋합니다.
- [ ] 사용자 환경에서 음악과 화면의 동시 진행을 검증합니다.

## English

- [x] Confirm `batched=0`, the exact EIP, and byte-fast-path activation in the latest user log.
- [x] Confirm the target opcode signature in the static `PIU.EXE`.
- [x] Add first-occurrence diagnostics for each fail-closed rejection stage.
- [x] Run the Win32 x86 Debug build and PIU10 probe.
- [x] Capture the actual rejection condition in a bounded `pumpito` run.
- [x] Correct the cause while preserving original-loop semantics and add a regression probe.
- [x] Revalidate batch activation with the build, probe, and bounded runtime.
- [x] Update cumulative analysis and the work log, then commit.
- [ ] Validate concurrent music and rendering in the user's environment.
