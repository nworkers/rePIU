# Task 716 설계 — long mode에서 `CS:` 데이터 접근을 복사로 방출

## 목적

Task 715는 Linux x64에서 safe point 틱 주입을 켜면 게임 시계가 되살아나지만
27–28초에 `int3`로 죽는다는 것을 보였다. 이 작업은 그 크래시를 없앤다.

## 원인

Task 714는 이 크래시를 "일시적 `int3`"라고 기록했지만 틀린 해석이었다. 보고된
`rip`(`0x20007D0D`)는 되감기 전의 host RIP이고, 실제 `int3`는 한 바이트 앞
`0x20007D0C`에 **계속** 있다. 보고에 찍힌 `67 88 01`은 그 다음 명령이다.

그 `int3`는 게스트 `0x010F74A1`의 HLE boundary다.

```text
010F74A1  2E 8A 82 58 74 0E 00   mov al, cs:[edx+0x000E7458]   ; itoa 숫자표
```

* 계획기(`IsHleBoundary`)는 segment prefix가 붙은 명령을 **모든 host에서** HLE
  boundary로 만든다. ES/SS/DS/FS/GS 데이터 접근은 `kSegmentOverrideMem`으로 따로
  빠지지만 CS는 빠지지 않는다.
* Win32는 원본 명령을 single-step으로 실행해 boundary를 처리한다. 게스트 코드는
  host의 flat CS(base 0)로 실행되므로 이것이 맞다.
* x64에는 이 boundary를 처리하는 곳이 없다. 게임은 이 `itoa`를 ISR에서만 부르므로,
  주입이 꺼져 있으면 도달하지 않고 켜면 처음 실행에서 죽는다.

## 설계

long mode는 CS override를 무시한다. 32비트 코드의 `cs:[edx+disp]`는 Win32에서도
선형 주소 `edx+disp`이므로, **prefix를 그대로 둔 채 보통의 `67` lowering으로 복사하면
의미가 같다.**

1. **emitter** — long-mode 분기에서 `IsLongModeCsDataBoundary`가 참인 boundary
   기록을 `kCopy`처럼 `EmitLongModeCopy`로 보낸다. boundary는 블록을 끝내므로 복사
   뒤에 다음 게스트 명령으로 가는 `E9 rel32`(`kBlockFallthrough`)를 붙인다.
   * 조건: 32비트 코드, segment prefix 있음, 특권 명령 아님, 범주가
     INTERRUPT/IO/IOSTRINGOP/RDWRFSGS/SEGOP/SYSCALL/분기가 아님, segment 레지스터
     operand 없음, 메모리 operand가 하나 이상이고 모두 CS.
2. **분류기** — `ClassifyLongModeBytes`가 CS만 쓰는 메모리 operand를
   `kAddressSizePrefix`로 판정한다. 16비트 코드(실제 CS base가 있음), 다른 segment,
   절대 `disp32` 형태(ModRM 재작성이 필요)는 계속 거절한다.
3. **HLE 커버리지 검증** — `ValidateAotCodeCacheHleCoverage`는 첫 바이트가 `0xCC`가
   아닌 boundary를 DBT HLE dispatch 슬롯으로 보고 검사한다. 위 복사를 인식하지
   못하면 **이미지 전체가 거부된다.** 복사와 그 fallthrough fixup을 확인하도록 한다.

Win32는 long-mode 분기를 타지 않으므로 동작이 바뀌지 않는다.

## 검증

* 분류기 probe: `itoa` 명령의 판정·lowering을 long mode로 **해독해서** 확인, ES·절대
  형태·16비트·CS 점프 테이블은 거절
* emitter probe: 한 계획 안에서 CS 복사가 다음 블록으로 점프해 도착하는지, 다른
  boundary는 `int3`로 남는지, 커버리지 검증을 통과하는지
* Linux x64 기본(off) 실행 회귀 없음, 주입 on 실행에서 `0x010F74A1` 크래시 소멸
* Win32 core probe와 30초 실행이 이전과 같음

---

## English

### Purpose

Task 715 showed that turning on safe-point tick injection on Linux x64 revives the
game clock but dies on an `int3` at 27–28 seconds. This task removes that crash.

### Cause

Task 714 called it a "transient `int3`", which was a misreading. The reported `rip`
(`0x20007D0D`) is the host RIP before rewinding; the actual `int3` sits one byte
earlier at `0x20007D0C` and **stays there**. The `67 88 01` in the report is the
next instruction. That `int3` is the HLE boundary for guest `0x010F74A1`,
`2E 8A 82 58 74 0E 00` = `mov al, cs:[edx+0x000E7458]` (an `itoa` digit table).

* The planner (`IsHleBoundary`) makes every segment-prefixed instruction an HLE
  boundary **on every host**. ES/SS/DS/FS/GS data accesses are split off as
  `kSegmentOverrideMem`; CS is not.
* Win32 services the boundary by single-stepping the original, which is right
  because guest code runs under the host's flat CS (base 0).
* Nothing services it on x64. The game calls this `itoa` only from its ISR, so with
  injection off it is never reached, and with injection on it dies on first use.

### Design

Long mode ignores a CS override, and on Win32 `cs:[edx+disp]` in 32-bit code is the
linear `edx+disp`, so **copying it with the prefix kept, behind the ordinary `67`,
means the same thing.**

1. **Emitter**: in the long-mode branch, a boundary record for which
   `IsLongModeCsDataBoundary` holds goes through `EmitLongModeCopy` as a `kCopy`
   would. A boundary ends its block, so the copy is followed by an `E9 rel32`
   (`kBlockFallthrough`) to the next guest instruction. The screen requires 32-bit
   code, a segment prefix, no privileged instruction, a category other than
   interrupt/IO/string IO/FS-GS access/segment op/syscall/branch, no segment
   register operand, and at least one memory operand, all of them CS.
2. **Classifier**: `ClassifyLongModeBytes` judges a CS-only memory operand
   `kAddressSizePrefix`. 16-bit code (which has a real CS base), other segments,
   and the absolute `disp32` form (which needs its ModRM rewritten) stay refused.
3. **HLE coverage validation**: `ValidateAotCodeCacheHleCoverage` treats a
   boundary whose first byte is not `0xCC` as a DBT HLE dispatch slot. Without
   recognizing the copy, **the whole image is refused**; it now checks the copy and
   its fallthrough fixup.

Win32 never takes the long-mode branch, so its behavior does not change.

### Verification

* Classifier probe: the `itoa` instruction's verdict and lowering, checked by
  **decoding** the lowered bytes in long mode; ES, the absolute form, 16-bit code,
  and a CS jump table stay refused.
* Emitter probe: in one plan the CS copy jumps to and lands on the next block, the
  other boundaries stay `int3`, and HLE coverage validation passes.
* No regression in a default (off) Linux x64 run; with injection on, the
  `0x010F74A1` crash is gone.
* Win32 core probe and a 30-second run unchanged.
