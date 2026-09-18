# Task 713 작업 로그 — `mov r8h,[esp+d]` long-mode lowering의 DL 저장 방향

설계: [20260918-713](../design/20260918-713-high-byte-destination-lowering.md) ·
작업 지시: [20260918-713](../work-orders/20260918-713-high-byte-destination-lowering.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-712](20260918-712-flat-stack-ss-override.md)

## 요약

**Linux x64에서 텍스처 경로가 처음으로 동작합니다.** 두 host의 Glide gate 96개가 모두
일치하고(이전에는 #50에서 갈라짐), 삼각형의 텍스처 좌표·combine·texture enable이
Win32와 같습니다. 원인은 long-mode lowering 하나의 **명령 방향이 뒤집힌 것**이었습니다.

## 추적

Task 712로 두 host의 파일 연산이 같아졌는데도 gate #51이 갈라졌으므로, 원인은 파일이
아니라 게스트 계산이었습니다. 게스트 코드를 거슬러 올라갔습니다(주소는 Linux 기준).

1. **gate #50 뒤의 분기.** Win32는 텍스처 상주 함수 `0x0104ABCC`로, Linux는 텍스처 없는
   재질 함수 `0x0104B4A4`로 갑니다. 둘을 고르는 곳은 재질 선택 함수 `0x0104B520`이고,
   텍스처 경로의 조건은 `scene[0xDEFC] & 2`입니다.
2. **도달 기록 probe.** 선택 함수는 여러 번 불리고 처음 몇 번은 두 host 모두 텍스처
   없음이 정상이라, 첫 도달만 잡는 probe로는 볼 수 없었습니다.
   `REPIU_EXECUTION_PROBE_LOG_ARRIVALS`를 더해 첫 N번을 기록했습니다.

   | 선택 함수 호출 | 1–3번째 | **4번째** |
   |---|---|---|
   | Win32 `scene[0xDEFC]` | `0x00` | **`0x02`** |
   | Linux `scene[0xDEFC]` | `0x00` | **`0x00`** |

3. **비트 2를 세우는 곳.** `0x010BE120`은 게임 안의 **OpenGL식 `glEnable`/`glDisable`**
   구현입니다(비교 상수가 GL enum — `0x0DE1`=`GL_TEXTURE_2D`, `0x1A00`=`GL_RENDER`).
   `GL_TEXTURE_2D` case(`0x010BEB7A`)는 `*(ctx[0x8F8]) != 0`일 때만 비트를 세웁니다.

   | | Win32 (정규화) | Linux |
   |---|---|---|
   | `glEnable(GL_TEXTURE_2D)` 호출 | 있음 | 있음 |
   | `ctx[0x8F8]` | `0x0158DAA0` | **`0x0158DA11`** |
   | 비트 설정 지점 도달 | 예 | **아니오** |

   상위 3바이트는 같고 하위 1바이트만 다르며, Linux 값은 정렬도 어긋나 있습니다.
4. **포인터의 출처.** `ctx[0x8F8]`는 GL 컨텍스트 생성 함수 `0x010C954C`가 첫 인자로
   받은 visual 포인터이고, visual은 `0x010C92B0`이 `calloc(1,0x28)`로 만들어 `EDX`에
   들고 있다가 돌려줍니다. 그 사이에 `0x010C931B`의 `mov bh,[esp]`가 있습니다.
5. **lowering.** `mov bh,[esp]`는 REX가 필요한 `[r15]`와 상위 바이트 레지스터가 한
   명령에 올 수 없어 네 명령으로 낮춰집니다(Task 676). 첫 명령은 DL을 R14B에 저장해야
   하는데 `44 88 F2`로 방출돼 있었고, 이것은 `mov dl, r14b`입니다. 저장이 아니라
   덮어쓰기라 **EDX 하위 바이트가 R14B로 바뀝니다.** 그래서 `0x...A0`이 `0x...11`이
   됐습니다.

## 수정

첫 명령을 `41 88 D6`(`mov r14b, dl`)으로 고쳤습니다. 나머지 세 명령은 해독해 확인했고
옳습니다. R14D는 emitter의 scratch(Task 558/559)라 이 네 명령 안에서 쓰는 것은 계약에
맞습니다.

두 probe가 틀린 바이트를 기대값으로 갖고 있었습니다
(`long_mode_compatibility`, `long_mode_lowering`). 구현에서 옮겨 적은 배열이라
구현과 일치했을 뿐입니다. 기대 바이트를 고치고 **의미를 보는 검사 두 개**를 더했습니다.

* `long_mode_compatibility` — 방출된 네 명령을 Zydis로 해독해 각 명령의 목적지와
  원본, 상위 바이트 명령에 REX가 없음을 확인
* `long_mode_lowering`(x64) — sequence를 **실행**해 `AH`가 `[esp]` 값이 되고 `DL`이
  보존되는지 확인. R14에는 DL과 다른 값을 넣어 우연히 통과할 수 없게 함

## 검증

### probe

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**
* **음성 확인**: 수정 전 emitter로 되돌리면 세 검사가 모두 실패합니다.
  실행 검사는 `ah=0x5a,dl=0xef`를 보고합니다 — DL이 R14(`0xDEADBEEF`)의 하위
  바이트가 됩니다. 실행에서 본 결함을 따로 떼어 그대로 재현합니다.

### Linux x64 (30초, `pumpit2a`, `REPIU_GLIDE_DRAW_DIAG=1`)

| | Task 712 | Task 713 | Win32 |
|---|---|---|---|
| gate #51 | `_GRALPHACOMBINE@20` | **`_GRTEXTEXTUREMEMREQUIRED@8`** | 같음 |
| gate #52–#53 | combine/blend | **다운로드 / clamp** | 같음 |
| 두 host gate 96개 일치 | #50까지 | **96개 전부** | — |
| 삼각형 st0 | `0/0` | **`0/0`, `256/0`** | 같음 |
| combine / texEnabled | `1/2`, `0` | **`3/1`, `1`** | 같음 |
| 삼각형 #3 non-black | 0 | **28,237** | 28,230 |
| 폴트 | 0 | 0 | 0 |

### Win32 회귀

long mode 전용 lowering이라 Win32 방출은 바뀌지 않습니다. 같은 조건으로 Task 712
결과와 비교했습니다: gate 96개 차이 **0**, 삼각형의 텍스처·combine·픽셀 **동일**,
DOS read/seek/open 91/25/11 **동일**, 폴트 0. **회귀 없음.**

## 도달 기록 probe에 대해

`REPIU_EXECUTION_PROBE_LOG_ARRIVALS`는 반복 사용할 수 있는 관측 도구로 남겼습니다.
x64에서는 sentinel이 reentry와 single-step을 거치므로 한 호출이 세 번 기록되고,
이번 진단 실행들은 실행 끝에서 SIGILL/SIGSEGV로 끝났습니다. 기록된 값에는 영향이
없지만 관측 자체가 실행을 교란하므로, 결론은 probe 없는 실행으로 확인해야 합니다 —
이번에도 그렇게 했습니다.

---

## English

Design: [20260918-713](../design/20260918-713-high-byte-destination-lowering.md) ·
Work order: [20260918-713](../work-orders/20260918-713-high-byte-destination-lowering.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-712](20260918-712-flat-stack-ss-override.md)

### Summary

**The texture path works on Linux x64 for the first time.** All 96 Glide gates now
match across the hosts (they diverged at #50 before), and the triangles' texture
coordinates, combine and texture enable are the same as on Win32. The cause was
**one instruction in a long-mode lowering written in the wrong direction**.

### Trace

With file activity equal after Task 712 and gate #51 still diverging, the cause had
to be guest computation. Walking back through the guest code (Linux addresses):

1. **The branch after gate #50.** Win32 goes to the texture-residency function
   `0x0104ABCC`, Linux to the untextured material function `0x0104B4A4`; the
   material selector `0x0104B520` chooses, and the textured path needs
   `scene[0xDEFC] & 2`.
2. **Arrival logging.** The selector runs several times and its first calls are
   legitimately untextured on both hosts, so a first-arrival probe could not see it.
   `REPIU_EXECUTION_PROBE_LOG_ARRIVALS` logs the first N arrivals. On the fourth
   call, `scene[0xDEFC]` is **`0x02` on Win32 and `0x00` on Linux**.
3. **Where bit 2 is set.** `0x010BE120` is an **OpenGL-style
   `glEnable`/`glDisable`** inside the game (its constants are GL enums:
   `0x0DE1` = `GL_TEXTURE_2D`, `0x1A00` = `GL_RENDER`). The `GL_TEXTURE_2D` case at
   `0x010BEB7A` sets the bit only when `*(ctx[0x8F8]) != 0`. Both hosts call
   `glEnable(GL_TEXTURE_2D)`, but `ctx[0x8F8]` is `0x0158DAA0` (normalized) on Win32
   and **`0x0158DA11`** on Linux, and Linux never reaches the bit-setting point. The
   top three bytes agree; only the low byte differs, and the Linux value is
   misaligned.
4. **Where the pointer comes from.** `ctx[0x8F8]` is the visual pointer passed as
   the first argument to the GL context creator `0x010C954C`; the visual is
   `calloc(1,0x28)` from `0x010C92B0`, held in `EDX` until return — across a
   `mov bh,[esp]` at `0x010C931B`.
5. **The lowering.** `mov bh,[esp]` cannot name `[r15]` (which needs REX) and a
   high-byte register in one instruction, so it becomes four (Task 676). The first
   should save DL into R14B, but was emitted as `44 88 F2`, which is
   `mov dl, r14b`: an overwrite, not a save, **replacing EDX's low byte with R14B**.
   That is how `0x...A0` became `0x...11`.

### Fix

The first instruction is now `41 88 D6` (`mov r14b, dl`); the other three were
decoded and are correct. R14D is the emitter's scratch (Tasks 558/559), so using it
within these four instructions fits the contract.

Two probes held the wrong bytes as their expected value (`long_mode_compatibility`
and `long_mode_lowering`) — arrays copied from the implementation, which is why
they agreed with it. The expected bytes are fixed, and **two checks that look at
meaning** were added: a Zydis decode in `long_mode_compatibility` checking each
instruction's destination, source and the absence of REX on the high-byte move;
and, in the x64 `long_mode_lowering`, **executing** the sequence to check that `AH`
receives the value at `[esp]` and `DL` is preserved, with R14 loaded with a value
unlike DL so a reversed save cannot pass by coincidence.

### Verification

Linux x64 core probe **30 of 30** and Win32 x86 **28 of 28**. **Negative check**:
against the pre-fix emitter all three checks fail, and the execution check reports
`ah=0x5a,dl=0xef` — DL becomes the low byte of R14 (`0xDEADBEEF`), reproducing the
live defect in isolation.

On a 30-second Linux x64 run, gate #51 is **`_GRTEXTEXTUREMEMREQUIRED@8`**, followed
by the download and clamp gates; **all 96 gates match Win32**; the triangles carry
s/t `0/0` and `256/0`, combine `3/1` and texture enable `1`, the same as Win32; and
triangle #3 leaves **28,237** non-black pixels (Win32: 28,230), with no faults.

**Win32 regression.** The lowering is long-mode only, so Win32 emission is
unchanged. Compared against Task 712 under identical conditions: **zero** gate
differences, **identical** triangle texture state, combine and pixels, identical
DOS read/seek/open (91/25/11), no faults. **No regression.**

### About arrival logging

`REPIU_EXECUTION_PROBE_LOG_ARRIVALS` stays as a reusable observation tool. On x64
the sentinel passes through reentry and single-step, so one call is logged three
times, and these diagnostic runs ended in SIGILL/SIGSEGV at the end. The recorded
values are unaffected, but the observation perturbs execution, so conclusions have
to be confirmed by runs without the probe — as they were here.
