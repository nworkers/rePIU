# 20260911-653 작업 지시: Linux x64 unresolved return legacy bridge

설계: [20260911-653 설계](../design/20260911-653-linux-x64-return-legacy-bridge.md)

## 한국어

1. Linux x64 return resolver에 identical-first-instruction fallback policy를 추가합니다.
2. guest EFLAGS+TF, register, ESP를 보존하는 platform legacy bridge thunk를 구현합니다.
3. 승인/거부 policy와 thunk 연결을 probe로 검증하고 실제 실행에서 bridge 진입을 검증합니다.
4. Linux x64 Debug 빌드, 전체 core probe, 실제 `pumpit2a`를 실행합니다.
5. 아키텍처·분석·작업 로그를 갱신하고 커밋합니다.

## English

1. Add an identical-first-instruction fallback policy to the Linux x64 return resolver.
2. Implement a platform legacy bridge thunk preserving guest EFLAGS+TF, registers, and ESP.
3. Probe policy admission/refusal and thunk linkage, then verify bridge entry in a real run.
4. Build Linux x64 Debug and run the complete core probe and real `pumpit2a`.
5. Update architecture, analysis, and the work log, then commit.
