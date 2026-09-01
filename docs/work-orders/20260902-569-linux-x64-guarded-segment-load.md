# 작업 지시 20260902-569 — Linux x64 guarded segment load

설계: [20260902-569-linux-x64-guarded-segment-load.md](../design/20260902-569-linux-x64-guarded-segment-load.md)

## 범위

1. guarded-load site가 자기 prologue와 counter operand 유무를 기록하도록 확장한다.
2. guarded-load byte patch를 runtime 함수로 옮기고 engine에는 메모리 보호/flush만
   남긴다.
3. source GPR16과 shadow selector가 같을 때만 통과하는 x64 slot을 발행한다.
4. x64 probe에 selector 일치, 불일치, unresolved→native 복원 검증을 추가한다.
5. census가 emitter의 공용 판정을 사용하고 별도 guarded-load 수치를 보고하도록
   갱신한다.
6. Linux x64/i386 및 Win32 회귀를 검증하고 frontier를 다시 측정한다.
7. 관련 analysis, architecture, 작업 로그를 갱신한다.

---

# Work order 20260902-569 — Linux x64 guarded segment load

Design: [20260902-569-linux-x64-guarded-segment-load.md](../design/20260902-569-linux-x64-guarded-segment-load.md)

## Scope

1. Extend guarded-load sites to record their own prologue and whether counter
   operands exist.
2. Move guarded-load byte patching into runtime, leaving memory protection and
   cache flushing in the engine.
3. Emit an x64 slot that passes only when source GPR16 equals the shadow
   selector.
4. Add matching, mismatching, and unresolved-to-native restoration coverage to
   the x64 probe.
5. Make the census use the emitter's shared predicate and report a separate
   guarded-load count.
6. Verify Linux x64/i386 and Win32 regressions and remeasure the frontier.
7. Update the related analysis, architecture, and work log.
