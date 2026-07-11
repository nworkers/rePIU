# operand-size prefix segment store HLE 설계

DOS4GW 식별 성공 뒤 PIU는 `66 8C E8` (`mov ax,gs`)로 client selector를 저장합니다. 기존 handler는 prefix 없는 `8C /r`만 register 형식으로 처리해 실제 Win32 GS `002Bh`가 저장됐습니다. `66 8C /r` register 형식도 software guest segment 값을 16비트 destination에 기록하고 명령 길이 3으로 진행합니다.

```mermaid
flowchart LR
    ID["guest GS=0020h"] --> MOV["66 8C E8 mov ax,gs"]
    MOV --> HLE["segment-store HLE"]
    HLE --> SAVE["saved GS=0020h"]
```

# Operand-Size-Prefixed Segment Store HLE Design

Handle register-form `66 8C /r` using the software guest segment value so PIU saves GS `0020h`, not the Win32 hardware GS `002Bh`.
