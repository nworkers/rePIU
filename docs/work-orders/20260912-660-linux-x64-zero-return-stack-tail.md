# Task 660 work order: Linux x64 zero-return stack tail

## 한국어

### 작업 범위

- zero-return frame trace에서 bounded stack-tail helper를 호출합니다.
- 기존 환경 변수와 ring capacity를 재사용합니다.
- guest instruction lowering, resolver target 선택, fail-closed 정책은
  변경하지 않습니다.
- 설계 문서: `docs/design/20260912-660-linux-x64-zero-return-stack-tail.md`

### 실행 순서

1. `TraceLinuxX64ZeroReturnFrame()`과
   `TraceLinuxX64ReturnStackTail()`의 호출 관계를 확인합니다.
2. zero-return frame 출력의 마지막에 tail 호출을 추가합니다.
3. Linux x64 Debug `repiu` 및 `repiu_core_probe`를 빌드합니다.
4. bounded runtime trace로 tail 출력과 기존 zero-return frontier를
   확인합니다.
5. 작업 로그와 frontier analysis를 갱신하고 커밋합니다.

### 완료 조건

- 기본 실행에서는 새 출력이 없습니다.
- tail 환경 변수가 있으면 zero-return frame 뒤에 bounded tail이 출력됩니다.
- core probe가 27/27로 통과합니다.
- zero target을 임의로 복구하지 않고 현재 fail-closed 동작을 유지합니다.

## English

### Scope

- Call the bounded stack-tail helper from the zero-return frame trace.
- Reuse the existing environment variable and ring capacity.
- Do not change guest instruction lowering, resolver target selection, or the
  fail-closed policy.
- Design: `docs/design/20260912-660-linux-x64-zero-return-stack-tail.md`

### Execution order

1. Confirm the relationship between `TraceLinuxX64ZeroReturnFrame()` and
   `TraceLinuxX64ReturnStackTail()`.
2. Add the tail call at the end of the zero-return frame output.
3. Build Linux x64 Debug `repiu` and `repiu_core_probe`.
4. Use a bounded runtime trace to confirm the tail output and the existing
   zero-return frontier.
5. Update the work log and frontier analysis, then commit.

### Completion criteria

- Default execution produces no new output.
- With the tail environment variable set, a bounded tail follows the
  zero-return frame.
- The core probe passes 27/27.
- No zero target is guessed into an address; current fail-closed behavior stays.
