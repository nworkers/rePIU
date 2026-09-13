# 20260912-658 작업 지시: Linux x64 frontier fault 상태 추적

설계: [20260912-658 Linux x64 frontier fault 상태 추적](../design/20260912-658-linux-x64-frontier-state-trace.md)

## 한국어

### 작업 목표

`0x010F316C` AOT access fault의 원인 귀속에 필요한 fault-time `EBP` 증거를
추가하고, 기존 reverse-map 증거와 함께 실제 실행에서 확인합니다.

### 작업 항목

1. `src/platform/linux/fault_handler.cpp`의 unhandled fault 출력에 `ebp`를
   추가합니다.
2. `src/engine/execution/execution_trampoline.cpp`의 opt-in execution trace에
   `ebp`를 추가합니다.
3. Linux x64 Debug에서 `repiu`와 `repiu_core_probe`를 재빌드합니다.
4. `repiu_core_probe` 전체 결과를 확인합니다.
5. `REPIU_AOT_FAULT_TRACE=1` 및
   `REPIU_AOT_FAULT_TRACE_ADDRESS=0x200022AC`로 `pumpit2a`를 실행하여
   cache-to-guest reverse map과 `ebp`를 기록합니다.
6. 필요하면 `REPIU_EXECUTION_TRACE_START=0xF316A`와
   `REPIU_EXECUTION_TRACE_END=0xF316C`를 사용해 fault 직전 trace의 `ebp`를
   비교합니다.
7. 확인된 사실, 추정, 미확정 사항을 `docs/analysis/linux-port-frontier.md`
   에 누적하고 작업 로그를 작성합니다.

### 변경하지 않는 항목

* 원본 guest bytes와 gameplay logic
* AOT long-mode copy/lowering 판정
* x64 return resolver 및 fallback 정책
* fault resume 동작

### 최소 검증

* Debug build 성공
* `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`
* filtered live run에서 다음 정보 확인:
  * cache `0x200022AC` → guest `0x010F316C`
  * fault instruction bytes
  * fault-time `ebp`
  * fault access address와 guest arena 범위의 관계

## English

### Objective

Add the fault-time `EBP` evidence needed to attribute the cause of the
`0x010F316C` AOT access fault, then correlate it with the existing reverse-map
evidence in a real run.

### Work items

1. Add `ebp` to the unhandled-fault report in
   `src/platform/linux/fault_handler.cpp`.
2. Add `ebp` to the opt-in execution trace in
   `src/engine/execution/execution_trampoline.cpp`.
3. Rebuild Linux x64 Debug `repiu` and `repiu_core_probe`.
4. Run the complete `repiu_core_probe` suite.
5. Run `pumpit2a` with `REPIU_AOT_FAULT_TRACE=1` and
   `REPIU_AOT_FAULT_TRACE_ADDRESS=0x200022AC`, recording the cache-to-guest
   reverse map and `ebp`.
6. If needed, use `REPIU_EXECUTION_TRACE_START=0xF316A` and
   `REPIU_EXECUTION_TRACE_END=0xF316C` to compare `ebp` in the trace immediately
   before the fault.
7. Append confirmed, inferred, and unresolved findings to
   `docs/analysis/linux-port-frontier.md` and write the work log.

### Explicitly unchanged

* Original guest bytes and gameplay logic
* AOT long-mode copy/lowering decisions
* x64 return resolver and fallback policy
* Fault resume behavior

### Minimum verification

* Debug build succeeds.
* `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`.
* The filtered live run confirms cache `0x200022AC` → guest `0x010F316C`,
  fault instruction bytes, fault-time `ebp`, and whether the access address lies
  outside the guest arena.

### 현재 판단

진단 결과 `EBP=0x5E7BBC68`이 guest stack 범위와 일치하지 않고,
`0x010F3159`의 `ENTER 4,0` 경계 이후 host frame pointer가 유지된 것으로
확인되었습니다. 의미론 구현 변경은 이 작업 지시서의 범위를 넘으므로 후속
작업 지시서에서 guest `ENTER` HLE를 추가합니다.

## Current conclusion

The diagnostic result shows that `EBP=0x5E7BBC68` is not a guest-stack value
and that a host frame pointer survives the `ENTER 4,0` boundary at `0x010F3159`.
Changing instruction semantics is outside this work order; a follow-up work
order will add guest `ENTER` HLE handling.
