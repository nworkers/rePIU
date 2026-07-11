# 원본 fatal tail 계속 실행 작업 로그

## 구현

* `CC 52 E8 rel32 F4` signature에만 breakpoint 재개를 허용했다.
* 현재 Windows context가 `CC` 자체를 가리키는 것을 실행으로 확인하고 정확히 1 byte 진행했다.
* breakpoint/message 주소와 bounded ASCIZ 문자열을 execution attempt에 기록했다.
* traced DOS `AH=09h` 출력을 연결했다.
* `F2/F3 REP MOVSB/MOVSD`의 linear↔DOS low-memory register-frame copy를 처리했다.
* fatal 경로에서 관찰된 DPMI `AX=0300h`, `BL=2Fh` 성공 반환을 제한적으로 처리했다.

```mermaid
flowchart LR
    INT3["INT 3"] --> DIAG["fatal diagnostics"]
    DIAG --> AH09["original AH=09h printer"]
    AH09 --> COPY["low-memory frame copy"]
    COPY --> DPMI["INT31 0300h / BL=2Fh"]
    DPMI --> EXIT["INT21 4C01h"]
    EXIT -. "if it returned" .-> HLT["HLT fallback"]
```

## 검증

* Win32 x86 Debug build 성공.
* `dos4gw_hello`: `Hello, world!`, exception 없음, fatal count 0.
* PIU: fatal breakpoint count 1, 주소 `0x020F3438`, message 주소 `0x021A623C`.
* 원본 error printer가 실제 console에 `Fatal error: unable to initialize DLL loader.` 출력.
* 후속 실행에서 caught exception 없이 `INT 21h AX=4C01h` 도달 확인.
* `HLT`는 terminate가 반환할 때의 fallback이므로 관찰되지 않았다.

# Original Fatal-Tail Continuation Work Log

Restricted breakpoint continuation to the original `CC 52 E8 rel32 F4` signature, recorded its address and bounded `EDX` message, connected traced DOS `AH=09h`, handled the observed low-memory `REP MOVS` register-frame copies, and narrowly accepted fatal-path DPMI `AX=0300h/BL=2Fh`.

The Win32 x86 Debug build and DOS/4GW hello regression succeeded. PIU recorded one fatal breakpoint at `0x020F3438`, printed the original fatal message through its own error printer, and subsequently reached DOS terminate `AX=4C01h` without a caught exception. The source `HLT` is a fallback if termination returns and was not reached.
