# Task 662 작업 지시: Linux x64 guest 진입 provenance 계측

## 한국어

### 작업 범위

1. Task 661의 unresolved boundary를 확인하기 위해
   `REPIU_LINUX_X64_GUEST_ENTRY_TRACE` 주소 필터를 추가합니다.
2. `VehExitRecorder`에 fault 입력 EIP와 dispatcher 진입/종료 시점의 EIP 및
   ESP를 보관하고, 진입 또는 종료 EIP가 guest 주소 또는 AOT cache의
   역매핑으로 필터와 일치할 때만 bounded line을 출력합니다.
3. source comments는 영어만 사용하고, guest 실행 상태·stack·resolver
   semantics는 수정하지 않습니다.
4. 검증 후 `docs/analysis/linux-port-frontier.md`와 작업 로그에
   확인됨/미확정 결과를 기록합니다.

### 구현 순서

1. `execution_trampoline.cpp`의 fault dispatcher와 AOT reverse-map 호출
   순서를 재확인합니다.
2. Linux x64 전용 주소 파서·trace helper와 `VehExitRecorder` snapshot을
   추가합니다.
3. Debug build 및 `repiu_core_probe`를 실행합니다.
4. 다음 bounded runtime을 실행합니다.

   `REPIU_LINUX_X64_GUEST_ENTRY_TRACE=0x010F0232`

   필요하면 기존 `REPIU_LINUX_X64_RETURN_FRAME_TRACE=1` 및
   `REPIU_FAULT_EXIT_TRACE=1`를 함께 사용해 순서를 대조합니다.

5. 결과를 analysis/work-log에 한국어 후 영어 순서로 반영하고 작업 단위를
   커밋합니다.

### 완료 조건

- 필터가 없을 때 기존 기본 출력과 동작이 유지됩니다.
- 필터가 일치할 때 fault 입력 EIP와 종료 guest/cache EIP가 bounded line에
  기록됩니다.
- Linux x64 Debug build와 core probe가 통과합니다.
- `0x010F0232`에 대한 실제 관찰 여부와 남은 비관찰 한계가 문서에
  명시됩니다.

### 작업지시 보완

진입 EIP와 종료 EIP를 모두 비교합니다. HLE가 대상 명령을 처리하면서
종료 EIP를 다음 명령으로 바꿔도 대상 명령의 cache boundary 진입을 놓치지
않도록 하기 위한 결정입니다.

## English

### Scope

1. Add the `REPIU_LINUX_X64_GUEST_ENTRY_TRACE` address filter to resolve the
   boundary left open by Task 661.
2. Preserve the fault input EIP and dispatcher entry/exit EIP and ESP in
   `VehExitRecorder`, and emit a bounded line when either endpoint, or its
   AOT-cache reverse mapping, matches the guest filter.
3. Use English-only source comments and do not change guest state, stack
   semantics, or resolver policy.
4. After verification, record confirmed and unresolved results in
   `docs/analysis/linux-port-frontier.md` and the work log.

### Implementation order

1. Reconfirm the fault-dispatch and AOT reverse-map order in
   `execution_trampoline.cpp`.
2. Add the Linux x64-only filter parser, trace helper, and
   `VehExitRecorder` snapshot.
3. Build Debug and run `repiu_core_probe`.
4. Run a bounded runtime with
   `REPIU_LINUX_X64_GUEST_ENTRY_TRACE=0x010F0232`, optionally alongside
   `REPIU_LINUX_X64_RETURN_FRAME_TRACE=1` and
   `REPIU_FAULT_EXIT_TRACE=1` for ordering.
5. Record results in Korean followed by English in the analysis and work log,
   then commit the task unit.

### Completion criteria

- Default behavior and output are unchanged when the filter is absent.
- A matching filter records the fault input EIP, dispatcher entry/final
  guest/cache EIP, and entry/exit guest ESP in a bounded line.
- Linux x64 Debug build and the core probe pass.
- The documents state whether `0x010F0232` was observed and what the remaining
  blind spot means.
