# 설계 20260905-601 — Linux x64 guest `66 EA` far jump HLE

## 목적

Task 600은 guest `PUSH CS`를 통과한 뒤 `0x010F016B`의
`66 EA 04 00 2C 00`에서 SIGILL을 확인했습니다. 이는 operand-size override가
있는 32-bit protected-mode `JMP FAR ptr16:16`, 즉 `002C:0004`로의 제어 이전입니다.

Linux x64 long mode는 `EA` far immediate jump를 실행할 수 없습니다. selector table은
이미 selector `002Ch`의 base와 limit를 보유하므로, HLE가 그 pointer를 linear guest
EIP로 변환해야 합니다.

## 설계 결정

1. 이번 단위는 정확히 관측한 6-byte `66 EA off16 sel16` 형식만 처리합니다.
2. offset과 selector를 instruction bytes에서 읽고
   `TranslateSelectorOffset(selector, offset, 1)`로 검증·변환합니다.
3. 성공하면 `EIP=linear_target`만 설정합니다. far jump는 stack과 EFLAGS를 바꾸지
   않으므로 이를 보존합니다.
4. selector가 없거나 limit를 벗어나면 처리하지 않고 기존 fail-closed fault를
   유지합니다.
5. LINEXE export 전용 far-transfer boundary와 `INT 31h AX=1E7Fh` probe의 의미는
   바꾸지 않습니다.

```mermaid
flowchart LR
    I[66 EA off16 sel16] --> D[off=0004 sel=002C]
    D --> T[selector table]
    T -->|valid| L[EIP = base(002C)+0004]
    T -->|absent or out of limit| F[existing fail-closed fault]
```

## 검증

* valid `002C:0004`, missing selector, and limit-overflow를 core probe로 확인합니다.
* Linux x64 build 및 core probe를 실행합니다.
* `REPIU_DPMI_1E7F_PROBE_SUCCESS=1` 실행에서 `0x010F016B` SIGILL이 사라지고
  다음 frontier가 관측되는지 확인합니다.

---

# Design 20260905-601 — Linux x64 guest `66 EA` far-jump HLE

## Purpose

After Task 600 passed guest `PUSH CS`, execution reached SIGILL at
`0x010F016B`, bytes `66 EA 04 00 2C 00`. This is an operand-size-override
32-bit protected-mode `JMP FAR ptr16:16`, transferring control to `002C:0004`.

Linux x64 long mode cannot execute the `EA` far-immediate jump. The selector
table already owns selector `002Ch`'s base and limit, so HLE must translate the
pointer to a linear guest EIP.

## Decisions

1. Handle only the observed six-byte `66 EA off16 sel16` form.
2. Decode offset and selector from instruction bytes and validate/translate with
   `TranslateSelectorOffset(selector, offset, 1)`.
3. On success set only `EIP=linear_target`. A far jump changes neither stack
   nor EFLAGS, so preserve both.
4. Preserve fail-closed behavior for an absent selector or out-of-limit offset.
5. Do not alter the LINEXE-export far-transfer boundary or the semantic status
   of the `INT 31h AX=1E7Fh` probe.

## Verification

* Use a core probe for valid `002C:0004`, missing selector, and limit overflow.
* Build and run Linux x64 `repiu` and `repiu_core_probe`.
* With `REPIU_DPMI_1E7F_PROBE_SUCCESS=1`, verify that SIGILL at `0x010F016B`
  disappears and a later frontier is observed.
