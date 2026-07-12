# First-chance host exception 진단 결과 / Result

선택형 `debug-exceptions` supervisor를 구현했습니다. `DEBUG_ONLY_THIS_PROCESS` event loop는 초기 breakpoint만 소비하고 guest 예외를 기존 VEH에 전달하며, guard-page context/page/bytes를 출력합니다.

초기 구현에서 creation flag가 `CreateFileMapping`의 high-size 인자에 잘못 연결되어 8 GiB mapping과 error 8을 만들었고 이를 수정했습니다. 정상 debug run에서는 guard-page가 없었고 process가 timeout 후 exit 0으로 종료됐습니다. 일반 실행 비교에서 마지막 host exception `0x40010006`을 확인했고, host debug-print exception을 `EXCEPTION_CONTINUE_EXECUTION`으로 처리해 초기 `0x80000001`/`0x40010006` 종료를 제거했습니다.

최종 일반 실행은 341초를 통과해 369초까지 진행했습니다. loader 내부 timeout 순간 `0xC0000005`가 발생해 다음 경계가 timeout teardown으로 이동했습니다.

```mermaid
flowchart LR
    A[early abnormal exit] --> D[debug event mode]
    D --> P[host DBG_PRINTEXCEPTION_C]
    P --> H[host exception consumed]
    H --> L[normal run passes 341 s]
    L --> T[369 s timeout teardown race]
```

The optional debug-event mode showed no guard-page exception. Comparison identified host `DBG_PRINTEXCEPTION_C` as the early-exit trigger. Consuming host debug-print exceptions restored normal execution through 341 seconds. The run then reached a new `0xC0000005` exactly at the loader's 369-second timeout, moving the frontier to timeout teardown.
