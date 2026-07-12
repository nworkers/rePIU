# AOT worker 기반 indirect inline cache 작업 지시

1. platform-neutral code-cache image에 indirect inline-cache site metadata와 guarded slot을 추가합니다.
2. Win32 placement가 동적 append 시 site offset을 병합하도록 합니다.
3. cache breakpoint의 실제 cache 주소를 보존해 정확한 miss site를 식별합니다.
4. worker request를 translate와 patch 동작으로 분리합니다.
5. worker가 target/cache edge/guard 순서로 patch하고 RX 및 instruction cache를 복원합니다.
6. PIU 3초 실행에서 `F514F -> F1E140` 반복 dispatch 감소와 legacy 대비 resource progress를 측정합니다.
7. `C3/C2`에 stack-top guard와 EFLAGS 비파괴 pop을 추가해 return dispatcher 감소를 측정합니다.
8. 확인 결과를 analysis, architecture, work log에 반영합니다.

# Worker-backed AOT Indirect Inline Cache Work Order

Add guarded indirect slots and metadata, merge them during Win32 placement/append, preserve the exact cache miss address, split worker translate and patch requests, patch under W^X, compare bounded PIU performance, and document the evidence.
