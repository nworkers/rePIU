# Task 716 작업 로그 — long mode에서 `CS:` 데이터 접근을 복사로 방출

설계: [20260918-716](../design/20260918-716-long-mode-cs-data-boundary.md) ·
작업 지시: [20260918-716](../work-orders/20260918-716-long-mode-cs-data-boundary.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-715](20260918-715-guest-clock-sampling.md)

## 요약

**safe point 주입을 켠 Linux x64의 27–28초 크래시를 없앴습니다.** 원인은 "일시적
`int3`"가 아니라, x64에 처리기가 없는 HLE boundary였습니다. 게스트의 `itoa`가 읽는
`mov al, cs:[edx+table]`을 이제 long mode에서 복사로 방출합니다. 주입은 여전히
opt-in이며, 켜면 **27–28초에 다른 크래시(SIGSEGV)**가 납니다.

## Task 714의 해석 정정

보고된 `rip=0x20007D0D`는 되감기 전의 host RIP입니다. `int3`는 한 바이트 앞
`0x20007D0C`에 그대로 있고, 보고의 `67 88 01`은 그 다음 명령입니다. "누가 `0xCC`를
썼다가 되돌렸는가"라는 Task 714의 질문은 잘못 세운 질문이었습니다.

## 원인

게스트 `0x010F74A1`: `2E 8A 82 58 74 0E 00` = `mov al, cs:[edx+0x000E7458]`.
계획기는 segment prefix가 붙은 명령을 모든 host에서 HLE boundary로 만들고, CS는
`kSegmentOverrideMem`으로 빠지지 않습니다. Win32는 single-step으로 처리하지만 x64에는
처리기가 없습니다. 이 `itoa`는 ISR에서만 불리므로 주입이 꺼져 있으면 도달하지
않습니다.

## 첫 시도에서 틀린 것 두 가지

1. **분류기만 고쳤습니다.** CS 명령은 계획 단계에서 boundary가 되므로 분류기에 도달하지
   않았고, 실행 결과가 바이트 하나 다르지 않았습니다. 분류기 probe는 통과했습니다.
2. **emitter를 고치자 게임이 1초 만에 멈췄습니다**(주입과 무관, off에서도 재현).
   `ValidateAotCodeCacheHleCoverage`가 첫 바이트가 `0xCC`가 아닌 boundary를 DBT HLE
   dispatch 슬롯으로 검사해 실패했고, 엔진이 **AOT 이미지 전체를 거부**했습니다.
   방출 바이트를 비교하는 probe는 이것을 잡지 못했습니다. 검증기를 고치고, probe에
   커버리지 검증을 넣었습니다(넣자마자 probe가 이 결함을 재현했습니다).

## 수정

* emitter: long-mode 분기에서 `IsLongModeCsDataBoundary` 기록을 `EmitLongModeCopy`로
  방출하고 다음 게스트 명령으로 `E9 rel32`를 붙임. 카운터
  `long_mode_cs_data_boundary_count`
* 분류기: CS만 쓰는 메모리 operand → `kAddressSizePrefix`. 16비트, 다른 segment,
  절대 `disp32`는 거절
* HLE 커버리지 검증: 위 복사와 fallthrough fixup을 인식

pumpit2a에서 복사로 방출되는 곳은 `0x010F74A1`, `0x010F7541` 두 곳입니다.

## 검증

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**, Win32 전체 빌드 오류 0
  * `long_mode_cs_override_data=true`: 낮춘 바이트를 long mode로 해독해
    `mov al, [edx+0xE7458]`(주소 폭 32)임을 확인, ES·절대 형태·16비트·CS 점프 테이블은
    거절
  * `long_mode_emission_cs_data_boundary=true`: 복사 뒤 점프가 다음 블록에 도착,
    나머지 boundary는 `int3`, 커버리지 검증 통과
* Linux 기본(off) 12초: 폴트 0, 렌더 루프 도달(이전과 같음)
* Linux 주입 on 30초: 틱 카운터가 초당 약 232로 증가(25초 4,257). **`0x010F74A1`
  크래시 소멸.** 27–28초에 SIGSEGV(아래)
* Win32 30초: 폴트 0, 틱 카운터 29초 5,579(이전 5,711, 출발 시각 차이)

## 새 frontier — 주입 on의 SIGSEGV

```text
guest 0x010ADAC9  8B 04 97   mov eax, [edi+edx*4]    edi=0xF186F186 edx=0xE
```

`0x010ADA84`는 해시 테이블 조회이고, 호출자 `0x01082314`는 GL 텍스처 바인드
(`0xDE1`=`GL_TEXTURE_2D`, `0xDE0`, `0x806F`)입니다. 테이블 포인터는
`ctx->Shared`의 필드(`[[esi]+8]`)이며 그 값이 `0xF186F186`로 덮여 있습니다.
`0xF186`은 RGB565로 주황빛(R 30, G 12, B 6)이고, 이 시점 장면은 주황 페이드입니다.
**픽셀 쓰기가 게스트 힙을 덮은 것으로 추정**합니다(미확인). 시계가 흐르면서 게임이
Win32처럼 다음 장면으로 넘어가 도달하는 코드일 가능성이 높습니다. Task 714의
"20초 전 SIGSEGV"와 같은 것인지는 확인하지 않았습니다.

## 다음

1. `0xF186F186`을 쓴 주체 — 같은 주소를 Win32에서 읽어 비교하고, 그 구조체를 덮는
   쓰기를 찾기
2. 그다음 주입을 기본값으로 켤지 판단

---

## English

Design: [20260918-716](../design/20260918-716-long-mode-cs-data-boundary.md) ·
Work order: [20260918-716](../work-orders/20260918-716-long-mode-cs-data-boundary.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-715](20260918-715-guest-clock-sampling.md)

### Summary

**The 27–28-second crash of Linux x64 with safe-point injection on is gone.** It was
not a "transient `int3`" but an HLE boundary with no handler on x64. The
`mov al, cs:[edx+table]` read by the guest's `itoa` is now emitted as a copy in long
mode. Injection stays opt-in; with it on, **a different crash (SIGSEGV) occurs at
27–28 seconds**.

### Correcting Task 714

The reported `rip=0x20007D0D` is the host RIP before rewinding. The `int3` stays
where it is, one byte earlier at `0x20007D0C`, and the `67 88 01` in the report is
the next instruction. Task 714's question — who writes `0xCC` there and back — was
the wrong question.

### Cause

Guest `0x010F74A1` is `2E 8A 82 58 74 0E 00` = `mov al, cs:[edx+0x000E7458]`. The
planner makes every segment-prefixed instruction an HLE boundary on every host, and
CS is not split off as `kSegmentOverrideMem`. Win32 single-steps it; x64 has no
handler. The `itoa` is called only from the ISR, so with injection off it is never
reached.

### Two things the first attempt got wrong

1. **Only the classifier was changed.** The CS instruction becomes a boundary at
   planning and never reaches the classifier, so the run did not change by a byte,
   while the classifier probe passed.
2. **With the emitter changed, the game stopped one second in** — independent of
   injection, reproduced with it off. `ValidateAotCodeCacheHleCoverage` checked a
   boundary whose first byte is not `0xCC` as a DBT HLE dispatch slot, failed, and
   the engine **refused the whole AOT image**. The byte-comparing probe could not
   see this. The validator was fixed and the probe now runs coverage validation,
   which reproduced the defect as soon as it was added.

### Fix

* Emitter: in the long-mode branch, an `IsLongModeCsDataBoundary` record is emitted
  through `EmitLongModeCopy` followed by an `E9 rel32` to the next guest
  instruction; counted in `long_mode_cs_data_boundary_count`.
* Classifier: a CS-only memory operand is `kAddressSizePrefix`; 16-bit code, other
  segments, and absolute `disp32` stay refused.
* HLE coverage validation recognizes the copy and its fallthrough fixup.

In pumpit2a two sites are emitted as copies: `0x010F74A1` and `0x010F7541`.

### Verification

* Linux x64 core probe **30 of 30**, Win32 x86 **28 of 28**, full Win32 build with
  no errors. `long_mode_cs_override_data=true` decodes the lowered bytes in long
  mode as `mov al, [edx+0xE7458]` with 32-bit addressing and refuses ES, the
  absolute form, 16-bit code, and a CS jump table;
  `long_mode_emission_cs_data_boundary=true` shows the copy's jump landing on the
  next block, the other boundaries staying `int3`, and coverage validation passing.
* Linux default (off), 12 s: no faults, render loop reached, as before.
* Linux with injection on, 30 s: the tick counter climbs about 232 per second
  (4,257 at 25 s); **the `0x010F74A1` crash is gone**; SIGSEGV at 27–28 s (below).
* Win32, 30 s: no faults, tick counter 5,579 at 29 s (5,711 before; the start time
  differs).

### New frontier: the SIGSEGV with injection on

Guest `0x010ADAC9` `8B 04 97` = `mov eax, [edi+edx*4]` with `edi=0xF186F186`,
`edx=0xE`. `0x010ADA84` is a hash-table lookup; its caller `0x01082314` is a GL
texture bind (`0xDE1` = `GL_TEXTURE_2D`, `0xDE0`, `0x806F`). The table pointer is a
field of `ctx->Shared` (`[[esi]+8]`) and has been overwritten with `0xF186F186`.
`0xF186` is orange in RGB565 (R 30, G 12, B 6), and the scene at that moment is the
orange fade, so **a pixel write overwriting the guest heap is the working guess**
(unconfirmed). It is probably code the game reaches because, with the clock
running, it moves on to the next scene as Win32 does. Whether it is the same as
Task 714's "SIGSEGV before 20 seconds" was not checked.

### Next

1. What wrote `0xF186F186`: read the same address on Win32 for comparison and find
   the write that overwrites the structure.
2. Then decide whether to turn injection on by default.
