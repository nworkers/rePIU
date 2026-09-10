# 20260910-650 작업 지시: Linux x64 간접 CALL fallback stack 의미 보존

설계: [20260910-650 설계](../design/20260910-650-linux-x64-indirect-call-fallback-stack.md)

## 한국어

1. 공용 간접 전송 handler가 대상 해석 전에 CALL stack 효과를 확정하도록 순서를
   바로잡습니다.
2. 간접 전송 miss tail의 fallback stack 정리량을 CALL/JMP별로 분리합니다.
3. CALL은 기존 guest 반환 주소를 보존하고 JMP는 기존 stack 효과를 유지하도록 layout
   검증을 갱신합니다.
4. Linux x64 Debug 빌드와 core probe를 수행합니다.
5. 실제 `pumpit2a` 실행에서 반환 주소 보존과 다음 frontier를 측정합니다.
6. 아키텍처, 누적 분석, 작업 로그를 결과에 맞게 갱신하고 하나의 커밋으로 남깁니다.

## English

1. Commit CALL stack semantics in the shared indirect-transfer handler before target
   resolution.
2. Split indirect miss-tail fallback stack cleanup by CALL/JMP kind.
3. Update layout verification so CALL preserves the guest return address while JMP keeps
   its existing stack effect.
4. Build Linux x64 Debug and run the core probe.
5. Measure return-address preservation and the next frontier in real `pumpit2a`.
6. Update architecture, cumulative analysis, and the work log, then commit the task as one
   unit.
