# SIB segment-load 작업 로그

memory-form `8E /r`을 공용 ModR/M/SIB decoder로 확장했습니다. 빌드는 성공했지만 module candidate 시점 `[ESP+10h]` selector가 0으로 확인돼 SIB load 이전의 `GS:0044h` word-to-register 반영이 현재 blocker입니다.

```mermaid
flowchart LR
    ROOT["direct root 0090:059A"] --> WORD["GS:0044h -> AX"]
    WORD -->|"현재 AX/stack=0"| SIB["mov GS,[ESP+10h]"]
```

# SIB Segment-Load Work Log

Extended memory-form `8E /r` through the shared SIB decoder. Direct observation found zero at `[ESP+10h]`, moving the blocker earlier to the `GS:0044h` word-to-register operation.
