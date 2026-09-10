# 20260909-649 Linux x64 zero return slot writer provenance 작업 지시

## 한국어

### 작업

1. Task 648의 zero return slot, LE stack object tail, 기존 guest write watch 결과를
   설계 근거로 기록한다.
2. HLE guest write helper에 optional `GuestCpuContext` 전달 경로를 추가한다.
3. 현재 guest context를 가진 HLE instruction/AOT dispatch 호출부에서 이를 전달한다.
4. HLE trace가 guest EIP, source, 레지스터, destination, byte preview를 기록하는지
   검증한다.
5. Linux x64 Debug core probe와 실제 `pumpit2a` bounded trace를 실행한다.
6. 설계와 확인 결과를 `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`,
   작업 로그에 반영하고 커밋한다.

### 제한

* zero target을 수정하거나 RET/stack semantics를 변경하지 않는다.
* AOT 전체 store instrumentation을 추가하지 않는다.
* 원본 guest executable bytes와 runtime memory initialization을 수정하지 않는다.
* 진단 ring의 bounded 특성을 유지한다.
* source code comments are English only.

### 완료 기준

* `core_probe_failures=0`.
* exact HLE write record가 `0x0158CC58`, dword zero, non-zero guest EIP 및 registers를
  보여준다.
* `[repiu-exit]`의 Task 648 transfer provenance와 기존 unresolved thunk 동작이
  유지된다.
* 설계·분석·작업 로그가 한국어/영어로 작성되고 관련 변경이 커밋된다.

## English

### Work

1. Record Task 648's zero return slot, LE stack-object tail, and existing guest-write
   watch as the design basis.
2. Add an optional `GuestCpuContext` path to the HLE guest-write helpers.
3. Pass it from HLE instruction and AOT dispatch call sites that already have the
   current guest context.
4. Verify that the HLE trace records guest EIP, source, registers, destination, and
   byte preview.
5. Run the Linux x64 Debug core probe and a bounded real `pumpit2a` trace.
6. Update `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`, and the work log,
   then commit the task.

### Limits

* Do not repair the zero target or change RET/stack semantics.
* Do not add whole-program AOT store instrumentation.
* Do not modify original guest executable bytes or runtime memory initialization.
* Preserve the bounded diagnostic ring.
* Source-code comments remain English only.

### Done criteria

* `core_probe_failures=0`.
* The exact HLE write record shows `0x0158CC58`, a zero dword, a non-zero guest EIP,
  and registers.
* Task 648's `[repiu-exit]` transfer provenance and unresolved-thunk behavior remain
  unchanged.
* Design, analysis, and work-log documents are written in Korean/English and the
  related changes are committed.
