# AOT 내부 직접 edge 연결 작업 로그

내부 direct call/jump/Jcc를 native cache edge로 복원했습니다. 3초 `aot-dynamic` 관찰은 `SPR.RES` 로딩까지 진행했으며, legacy도 같은 구간에서 해당 단계에 도달했습니다. 남은 병목은 동적 indirect call/return dispatcher 반복입니다.

# AOT Internal Direct Edge Linking Work Log

Restored native cache edges for internal direct calls, jumps, and Jcc. A three-second `aot-dynamic` run reached `SPR.RES` loading, matching the legacy stage in the same interval. The remaining bottleneck is repeated dynamic indirect-call/return dispatch.
