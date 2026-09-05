# 설계 20260905-587 — Linux x64 세그먼트 PUSH HLE

상위 작업: [20260904-585](20260904-585-linux-x64-fault-address.md)

## 배경

Task 585의 shadow selector block이 long-mode guard의 잘린 포인터 접근을 제거한 뒤,
`pumpit2a`는 guest `0x010F18A4`의 환경 블록 스캔을 정상적으로 통과했습니다. 다음
frontier는 guest `0x010F4A96`의 `06` (`push es`)에서 발생한 `SIGILL`입니다.

원본 32-bit protected-mode guest에서 `push es`는 현재 ES selector를 guest stack에
32-bit stack slot으로 저장합니다. 그러나 x86-64 long mode에서는 `push es`, `push ss`,
`push ds` 인코딩이 불법입니다. AOT가 해당 명령을 boundary로 남긴 뒤 legacy
single-step 경로가 원본 바이트를 실행하면 Linux가 `SIGILL`을 발생시킵니다.

`push fs`와 `push gs`는 long mode에서 실행 가능하지만 host selector와 guest shadow
selector가 다르고 host가 8-byte push를 수행합니다. 따라서 이 둘도 원본 바이트를
실행해서는 안 됩니다.

## 결정

`instruction_emulation`에 `HandleSegmentPushInstruction`을 추가하고, 기존 segment
HLE 체인의 segment-load/pop 앞에서 호출합니다. 다음 opcode만 인식합니다.

| Guest opcode | Shadow selector | Guest EIP 증가량 |
|---|---:|---:|
| `06` (`push es`) | `guest_es` | 1 |
| `16` (`push ss`) | `guest_ss` | 1 |
| `1E` (`push ds`) | `guest_ds` | 1 |
| `0F A0` (`push fs`) | `guest_fs` | 2 |
| `0F A8` (`push gs`) | `guest_gs` | 2 |

성공 경로는 다음 계약을 지킵니다.

1. `ESP - 4`가 guest writable 범위인지 먼저 확인합니다.
2. selector의 하위 16비트를 zero-extended 32-bit 값으로 그 주소에 저장합니다.
3. `ESP`를 새 주소로, `EIP`를 명령 뒤로 갱신하며 EFLAGS와 다른 일반 레지스터는
   변경하지 않습니다.
4. 범위 검증이나 opcode 인식에 실패하면 `false`를 반환하여 기존 HLE fallback을
   보존합니다.

```mermaid
sequenceDiagram
    participant AOT as AOT boundary
    participant HLE as Segment PUSH HLE
    participant Stack as Guest stack
    participant Next as Next guest instruction

    AOT->>HLE: guest EIP at PUSH Sreg
    HLE->>Stack: validate ESP - 4
    HLE->>Stack: store zero-extended shadow selector
    HLE->>HLE: ESP -= 4; EIP += opcode length
    HLE->>Next: resume through normal dispatch
```

## 범위와 검증

이 작업은 selector를 load/store하는 기존 HLE, AOT guard code, 또는 guest logic을
바꾸지 않습니다. core probe를 다시 실행하고, Linux x64 `pumpit2a`를 실행해
`0x010F4A96`의 `SIGILL`이 사라지고 실행 frontier가 전진하는지 확인합니다.

---

# Design 20260905-587 — Linux x64 Segment PUSH HLE

Parent task: [20260904-585](20260904-585-linux-x64-fault-address.md)

## Background

After Task 585's shadow-selector block removed the truncated-pointer access in
the long-mode guard, `pumpit2a` cleanly passed the environment-block scan at
guest `0x010F18A4`. The next frontier is `SIGILL` at guest `0x010F4A96`, byte
`06` (`push es`).

In the original 32-bit protected-mode guest, `push es` stores the current ES
selector in a 32-bit guest-stack slot. In x86-64 long mode, however, the
`push es`, `push ss`, and `push ds` encodings are invalid. When AOT leaves the
instruction as a boundary and the legacy single-step route executes the
original byte, Linux raises `SIGILL`.

`push fs` and `push gs` are executable in long mode, but their host selectors
can differ from their guest shadows and the host performs an 8-byte push. They
must therefore not execute from original guest bytes either.

## Decision

Add `HandleSegmentPushInstruction` to `instruction_emulation`, and invoke it
before the existing segment-load/pop handlers in the segment HLE chain. It
recognizes only the opcode forms listed above.

The success path validates `ESP - 4`, writes the zero-extended shadow selector,
updates only ESP and EIP, and otherwise preserves the guest register and flag
state. An unrecognized opcode or invalid range returns `false` so the existing
fallback remains responsible.

## Scope and verification

This task does not alter selector load/store HLE, AOT guard code, or guest
logic. Re-run the core probe and execute Linux x64 `pumpit2a`, confirming that
the `0x010F4A96` SIGILL disappears and the execution frontier advances.
