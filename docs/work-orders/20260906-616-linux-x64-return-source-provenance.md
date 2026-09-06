# 20260906-616 Linux x64 반환 source provenance 작업 지시

## 한국어

### 목적

공용 x64 return thunk에서 발생한 `guest_source=0`을 일반 `RET`의 zero
return word와 indirect call의 zero target 중 어느 것으로 분류할 수 있는지
resolver 입력 frame에서 확인합니다.

### 작업 항목

1. `REPIU_LINUX_X64_RETURN_FRAME_TRACE` opt-in 진단과 8회 상한을 추가합니다.
2. `LinuxX64AotDispatchFrame`의 guest 상태와 continuation 필드를 출력합니다.
3. frame guest ESP 기준 `-8/-4/0/+4` stack word를 fault-safe하게 읽고 valid
   mask를 출력합니다.
4. emitter producer tag를 frame status로 전달해 direct RET와 indirect call을
   확정적으로 구분합니다.
5. 마지막 indirect/return 상태와 source-stack 일치 여부를 함께 기록합니다.
6. Linux x64 Debug `repiu_core_probe` 및 `repiu`를 빌드합니다.
7. `pumpit2a`를 반환 trace와 함께 실행하여 producer를 분류합니다.
8. 확인된 사실과 다음 수정 경계를 분석 문서와 작업 로그에 기록합니다.

### 완료 기준

* trace off에서 기존 실행 제어 흐름과 출력이 유지됩니다.
* trace on에서 `source=0` frame의 guest ESP와 stack window가 기록됩니다.
* direct RET 후보와 indirect-call 후보를 관찰 결과에 따라 구분합니다.
* core probe가 실패 없이 통과합니다.
* resolver 결과나 fault recovery를 추정으로 변경하지 않고 후속 작업을
  명확히 분리합니다.

### 제외 범위

* `source=0` target 보정 또는 resolver 정책 변경
* return thunk assembly의 제어 흐름 변경
* guest 원본 바이트 패치
* allocator, DOS ABI, 또는 간접 호출 target의 의미 추정에 따른 동작 수정

## English

### Objective

Classify `guest_source=0` at the shared x64 return thunk as either a zero return
word from an ordinary `RET` or a zero target from an indirect call, using the
resolver input frame.

### Work items

1. Add the opt-in `REPIU_LINUX_X64_RETURN_FRAME_TRACE` diagnostic with an
   eight-event bound.
2. Print the guest state and continuation fields in
   `LinuxX64AotDispatchFrame`.
3. Read the frame guest ESP `-8/-4/0/+4` stack words fault-safely and print a
   validity mask.
4. Pass an emitter producer tag through frame status to distinguish direct RET
   from indirect call conclusively.
5. Include the last indirect/return state and source-to-stack matches.
6. Build Linux x64 Debug `repiu_core_probe` and `repiu`.
7. Run `pumpit2a` with return tracing and classify the producer.
8. Record confirmed facts and the next boundary in the analysis and work log.

### Done criteria

* Trace off preserves existing execution control flow and output.
* Trace on records the guest ESP and stack window for a zero-source frame.
* The frame producer tag and observed data distinguish the direct-RET and
  indirect-call candidates.
* The core probe passes without failures.
* No resolver or fault-recovery behavior is changed by inference; any fix is
  split into a following task.

### Out of scope

* Correcting `source=0` or changing resolver policy
* Changing return-thunk assembly control flow
* Patching original guest bytes
* Changing behavior based on an unproven allocator, DOS ABI, or indirect-call
  target interpretation
