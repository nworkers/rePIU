# DOS environment selector와 string 명령 HLE 설계

DOS4GW 성공 경로는 PSP `ES:[002Ch]`에서 environment selector를 읽어 DS로 로드합니다. 기존 임시 HLE가 0을 반환하여 정상 `LODSB`가 `0000:0`을 참조했습니다. 과거 fallback trace가 확인한 selector는 `002Ch`입니다.

```mermaid
flowchart LR
    PSP["PSP ES:002Ch"] -->|"002Ch"| DS["DS=002Ch"]
    DS --> LODS["LODSB environment scan"]
    LODS --> BLOCK["synthetic DOS environment"]
```

`LODSB` HLE는 software DS와 ESI로 한 바이트를 읽고 AL에 기록하며 DF에 따라 ESI를 증감합니다. 이는 Win32가 DOS selector를 직접 로드할 수 없는 CPU/DPMI 경계의 의미 보존입니다.

# DOS Environment Selector and String-Instruction HLE Design

Restore PSP environment selector `002Ch` and emulate `LODSB` through the software guest selector/environment view, updating AL and ESI according to DF.
