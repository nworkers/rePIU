# DOS read size provenance / DOS Read Size Provenance

## 한국어

RES payload 크기 truncation을 찾기 위해 기존 DOS file-I/O ring의 read entry에 guest EIP와 stack 상위 8 dword를 추가한다. 이를 Watcom `AH=3Fh` wrapper의 caller chain에 역매핑하여 32-bit 요청 크기가 16-bit chunk loop로 변환되는 지점을 찾는다. PIU.DAT 전용 동작은 추가하지 않는다.

```mermaid
flowchart LR
    S[32-bit RES size] --> W[Watcom read wrapper]
    W --> C[16-bit DOS chunks]
    C --> I[INT 21h AH=3Fh]
    I --> T[capture EIP and return stack]
    T --> R[static caller mapping]
```

## English

Extend each read entry in the existing DOS file-I/O ring with guest EIP and the top eight stack dwords. Map these return addresses through the Watcom `AH=3Fh` wrapper and restore the observed protected-mode DOS/4GW contract. The wrapper passes the byte count in 32-bit `ECX` and consumes a 32-bit `EAX` result, so HLE must not reduce the interface to real-mode `CX/AX`. No PIU.DAT-specific behavior is added.
