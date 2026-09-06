# 20260906-617 Linux x64 zero return word의 RET site 추적 설계

## 한국어

### 배경

Task 616에서 `source=0`이 indirect call target이 아니라 direct `RET`가
`ESP-4`의 zero word를 소비한 결과임을 확인했습니다. 그러나 현재 producer
tag는 `ret`/`indirect-call` 종류만 전달하므로 어느 guest `RET` instruction이
해당 word를 소비했는지는 알 수 없습니다.

### 목표

direct `RET` emitter가 thunk 진입 직전에 자신의 guest instruction 주소를
frame metadata로 전달하고, 기존 zero-source trace가 이를 출력하도록 합니다.

* direct `RET`: `R10D = guest RET site`
* indirect call: `R10D = 0x80000000 | guest indirect-call site`
* thunk: `R10D`를 기존 frame `status`에 복사
* zero-source trace: producer kind와 producer site를 함께 출력

guest 주소는 현재 runtime arena의 낮은 31-bit 범위에 있으므로 high bit를
producer discriminator로 사용할 수 있습니다. `MOV R10D, imm32`는 caller-
saved host register와 frame metadata만 사용하고 guest GPR/flags를 변경하지
않습니다.

```mermaid
flowchart LR
    R[guest RET record] --> E[emit site tag]
    E --> T[shared x64 thunk]
    T --> F[frame.status = producer tag]
    F --> Z{source == 0?}
    Z -- yes --> D[print producer site and stack window]
    D --> W[locate zero-word writer]
```

### 경계

* resolver target, return thunk 분기, guest stack 값은 변경하지 않습니다.
* producer site를 기록하는 metadata는 진단용이며 guest control flow를
  선택하지 않습니다.
* 첫 단계는 RET site 식별까지이며, stack writer 수정은 후속 증거를 본 뒤
  별도 작업으로 결정합니다.

## English

### Background

Task 616 confirmed that `source=0` was produced by an ordinary direct `RET`
consuming a zero word at `ESP-4`, not by an indirect-call zero target. The
current producer tag carries only `ret` versus `indirect-call`, so it does not
identify which guest `RET` instruction consumed the word.

### Goal

Have the direct `RET` emitter pass its guest instruction address as thunk
metadata and include it in the existing zero-source trace:

* direct `RET`: `R10D = guest RET site`;
* indirect call: `R10D = 0x80000000 | guest indirect-call site`;
* thunk: copy `R10D` into the existing frame `status`; and
* zero-source trace: print producer kind and producer site.

Guest addresses currently occupy the low 31-bit runtime-arena range, so the
high bit can serve as the producer discriminator. `MOV R10D, imm32` uses only a
caller-saved host register and frame metadata and changes neither guest GPRs
nor flags.

```mermaid
flowchart LR
    R[guest RET record] --> E[emit site tag]
    E --> T[shared x64 thunk]
    T --> F[frame.status = producer tag]
    F --> Z{source == 0?}
    Z -- yes --> D[print producer site and stack window]
    D --> W[locate zero-word writer]
```

### Boundaries

* Resolver targets, return-thunk branching, and guest stack values remain
  unchanged.
* The producer-site metadata is diagnostic and does not select guest control
  flow.
* The first step ends at identifying the RET site; any stack-writer fix is
  decided in a separate task after the evidence is available.
