# 20260910-651 작업 지시: Linux x64 legacy stack run 완결

설계: [20260910-651 설계](../design/20260910-651-linux-x64-aot-boundary-stack-drain.md)

## 한국어

1. legacy stack-run helper에 직접형 `SUB ESP, imm8/imm32` guest 의미를 추가합니다.
2. segment HLE 뒤 bounded drain을 상태 flag와 무관하게 허용하고 `0F` POP FS/GS를
   directed dispatch에서 처리합니다.
3. prologue stack allocation과 상태 독립 epilogue drain 합성 probe를 유지·확장합니다.
4. Linux x64 Debug 빌드, 전체 core probe, 실제 `pumpit2a`를 검증합니다.
5. 아키텍처·분석·작업 로그를 갱신하고 커밋합니다.

## English

1. Add guest semantics for direct `SUB ESP, imm8/imm32` to the legacy stack-run helper.
2. Allow bounded draining after segment HLE independently of state flags and handle
   `0F` POP FS/GS in directed dispatch.
3. Retain and extend synthetic coverage for prologue allocation and flag-independent
   epilogue draining.
4. Verify Linux x64 Debug build, the complete core probe, and real `pumpit2a`.
5. Update architecture, analysis, and the work log, then commit.
