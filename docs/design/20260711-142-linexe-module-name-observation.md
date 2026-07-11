# LINEXE module name 관찰 설계

selector `0090h` descriptor와 module record `0090:059A`의 name pointer, 실제 ASCIZ bytes, GS byte-load 횟수 및 첫 접근을 캡처합니다.

```mermaid
flowchart LR
    MODULE["0090:059A"] --> PTR["+04 name far pointer"]
    PTR --> NAME["0090:0504 LINEXE_LOADER"]
    NAME --> LOAD["GS byte-load trace"]
```

# LINEXE Module-Name Observation Design

Capture selector `0090h`, the module name far pointer, direct ASCIZ bytes, and GS byte-load trace to distinguish data and dispatch failures.
