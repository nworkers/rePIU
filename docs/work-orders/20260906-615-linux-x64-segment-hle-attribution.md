# 20260906-615 Linux x64 segment HLE attribution work order

## 한국어

### 작업 목표

Task 614 이후 `0x010F4A96` `PUSH ES` fault가 segment HLE 체인에서 어떻게
처리되는지 제어 흐름 변경 없이 확인합니다.

### 작업 항목

1. `REPIU_SEGMENT_HLE_TRACE` opt-in toggle와 제한된 진단 출력을 추가합니다.
2. x64 decode-window 판정과 `HandleSegmentPushInstruction` 호출/결과를
   같은 fault에 연결해 기록합니다.
3. Linux x64 Debug `repiu_core_probe` 및 `repiu`를 빌드합니다.
4. `pumpit2a`를 `0x010F4A96` watch와 새 trace로 실행합니다.
5. handler 미호출, handler 거절, handler 성공 후 다음 frontier 중 하나로
   결과를 분류하고 문서화합니다.

### 완료 기준

* trace off에서 기존 제어 흐름과 출력이 유지됩니다.
* trace on에서 `0x010F4A96`에 대한 decode/handler 결과를 확인할 수 있습니다.
* core probe가 실패 없이 통과하고 Linux x64 실행 증거가 작업 로그에 남습니다.
* 원인 수정은 진단 결과가 요구할 때 다음 작업으로 분리합니다.

### 제외 범위

* x64 fault recovery trampoline의 동작 변경
* guest 원본 바이트 패치
* allocator 또는 DOS memory ABI 추정

## English

### Objective

Determine how the `0x010F4A96` `PUSH ES` fault is handled by the segment HLE
chain after Task 614, without changing control flow.

### Work items

1. Add the opt-in `REPIU_SEGMENT_HLE_TRACE` toggle and bounded diagnostics.
2. Attribute the x64 decode-window decision and the
   `HandleSegmentPushInstruction` call/result to the same fault.
3. Build the Linux x64 Debug `repiu_core_probe` and `repiu` targets.
4. Run `pumpit2a` with the `0x010F4A96` watch and the new trace.
5. Classify the result as handler not reached, handler rejected, handler
   succeeded and a later frontier, then document the evidence.

### Done criteria

* Trace off preserves the existing control flow and output.
* Trace on exposes the decode and handler result for `0x010F4A96`.
* The core probe passes without failures and Linux x64 execution evidence is
  recorded in the work log.
* Any behavior fix required by the evidence is split into a following task.
* The shared-dispatch result for `0x010F4A96` is recorded when the instruction
  is handled before the later fault-chain fallback.

### Out of scope

* Changing the x64 fault-recovery trampoline
* Patching original guest bytes
* Guessing the allocator or DOS memory ABI

### Verified routing requirement

The `PUSH ES` instruction can be handled by the shared
`DispatchGuestHleHandlers` path used by single-step reentry, before the later
fault-chain fallback. The work order therefore treats both dispatch locations
as diagnostic boundaries.
