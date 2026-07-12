# AOT 내부 직접 edge 연결 작업 지시

1. 내부 direct call/jump/Jcc의 native rel32 emission을 복원합니다.
2. HLE, indirect, 지원하지 않는 branch만 dispatcher sentinel으로 유지하고, 미해결 direct target은 image 오류로 처리합니다.
3. block-fallthrough link와 함께 code-cache fixup을 검증합니다.
4. legacy와 `aot-dynamic` 3초 관찰의 진행 지표를 비교합니다.

# AOT Internal Direct Edge Linking Work Order

1. Restore native rel32 emission for internal direct call/jump/Jcc edges.
2. Keep dispatcher sentinels only for HLE, indirect, and unsupported branches, and reject unresolved direct targets as image errors.
3. Validate code-cache fixups alongside block-fallthrough links.
4. Compare three-second legacy and `aot-dynamic` observations.
