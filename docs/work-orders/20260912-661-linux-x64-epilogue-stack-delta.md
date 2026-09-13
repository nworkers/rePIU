# Task 661 작업 지시: Linux x64 epilogue stack delta 진단

## 한국어

### 작업 범위

- `0x010EFEC4 PUSH ES`의 실제 HLE stack transition을 기존 watch로
  확인합니다.
- `0x010EFEC7 CALL 0x010F09F0`와 callee의 반환 구조를 AOT map으로
  확인합니다.
- `0x010F0232` `POP ES` 직전 execution trace와 zero-return frame을
  대조합니다.
- stack width, return target, DPMI/guest ABI는 변경하지 않습니다.
- 설계 문서: `docs/design/20260912-661-linux-x64-epilogue-stack-delta.md`

### 실행 순서

1. 기존 `PUSH ES` watch를 실행하여 ESP 감소와 destination을 기록합니다.
2. `F022D–F0237` execution trace로 epilogue 진입 레지스터와 stack word를
   기록합니다.
3. `EFEC4` 및 `F09F0` AOT map으로 CALL/fallthrough와 callee RET bytes를
   확인합니다.
4. bounded 256-record stack tail에서 관련 writer와 call 순서를 확인합니다.
5. 확인됨/추정/미확정 상태를 analysis와 작업 로그에 기록합니다.

### 완료 조건

- `PUSH ES` 전후 ESP가 확인됩니다.
- `0x010F09F0`의 실제 return encoding과 `0x010EFECC` fallthrough가
  정적으로 확인됩니다.
- `0x010F0232` 진입 ESP와 stack window가 기록됩니다.
- 4바이트 delta의 위치를 확정하거나, 확정하지 못한 이유와 다음 관찰
  경계를 명확히 남깁니다.
- 소스 변경 없이 기존 fail-closed zero-return 동작을 유지합니다.

## English

### Scope

- Confirm the actual HLE stack transition for `0x010EFEC4 PUSH ES` with the
  existing watch.
- Confirm the AOT map for `0x010EFEC7 CALL 0x010F09F0` and the callee return
  structure.
- Compare the execution trace and zero-return frame immediately before
  `0x010F0232 POP ES`.
- Do not change stack width, return target, or any DPMI/guest ABI.
- Design: `docs/design/20260912-661-linux-x64-epilogue-stack-delta.md`

### Execution order

1. Run the existing `PUSH ES` watch and record its ESP decrement and
   destination.
2. Capture epilogue registers and stack words with the `F022D–F0237`
   execution trace.
3. Use the `EFEC4` and `F09F0` AOT maps to confirm CALL/fallthrough and callee
   RET bytes.
4. Inspect the relevant writer and CALL sequence in a bounded 256-record tail.
5. Record confirmed, inferred, and unresolved states in analysis and the work
   log.

### Completion criteria

- The ESP transition around `PUSH ES` is confirmed.
- The actual return encoding in `0x010F09F0` and fallthrough
  `0x010EFECC` are confirmed statically.
- The entry ESP and stack window at `0x010F0232` are recorded.
- The location of the four-byte delta is either established, or the reason it
  remains unresolved and the next observation boundary are explicit.
- Existing fail-closed zero-return behavior remains unchanged, with no source
  change.
