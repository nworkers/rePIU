# DLL loader INT 21h AX=FF00h 역추적 설계

## 목적

장시간 실행에서 도달한 arena `+0xF3438`의 `INT 3`를 원본 LE 코드와 실행 레지스터로 역추적하여 실제 실패 조건을 확정한다. 이번 작업은 원인을 분석하고 기록하며, 확인되지 않은 DOS/4G private 구조를 합성하지 않는다.

```mermaid
flowchart RL
    INT3["+0xF3438 INT 3"] --> BRANCH["직전 조건 분기"]
    BRANCH --> INIT["DLL loader 초기화"]
    INIT --> QUERY["INT 21h AX=FF00h"]
    QUERY --> HLE["현재 HLE 응답"]
```

## 검증 방법

* arena 기준 주소를 object 2 내부 오프셋으로 변환한다.
* 원본 LE data page를 읽기 전용으로 디스어셈블한다.
* 각 fatal 분기의 `EDX` 진단 문자열 주소와 실행 시 예외 `EDX`를 대조한다.
* 초기화 함수의 전역 selector 읽기와 이를 설정하는 시작 코드를 역추적한다.

# DLL Loader INT 21h AX=FF00h Provenance Design

Trace the `INT 3` at arena `+0xF3438` backward through the original LE code and runtime registers. Convert the arena-relative address to its object-relative offset, compare each fatal branch's diagnostic-string address with exception-time `EDX`, and trace the selector global back to the startup `INT 21h AX=FF00h` query. This task records the cause without synthesizing an unverified DOS/4G private structure.
