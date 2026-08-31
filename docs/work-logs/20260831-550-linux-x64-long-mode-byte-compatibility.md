# 20260831-550 Linux x64 long-mode byte compatibility 작업 로그

## 한국어

### 결과

Task 546 구현 순서 3단계(x64 emitter subset)에 착수하기 전에, 그 단계가 의존하는
판정기가 없다는 것이 확인됐습니다. Task 546 결정 5는 "x64 emitter가 long-mode
의미를 증명하기 전에는 복사된 32비트 바이트를 x86 전용으로 취급한다"고 적었지만,
증명할 주체가 없었습니다. 이번 단위는 emitter가 아니라 **emitter가 무엇을 다루어도
되는지 정하는 경계**를 만듭니다.

`ClassifyLongModeBytes`는 fail-closed입니다. 기본값이 `kUnsupported`이고, 항등임을
보인 경우에만 `kIdenticalBytes`를 줍니다.

핵심은 x64에서 32비트 바이트를 실행할 때의 위험이 **fault가 아니라 조용한 오답**
이라는 점입니다. Task 544가 만난 `pusha`/`popa`는 assembler가 거부해 주었으므로 운이
좋은 쪽 끝이었습니다. 위험한 쪽은 예외 없이 다른 명령이 되는 encoding입니다.

| 바이트 | 32비트 | 64비트 |
|---|---|---|
| `40`–`4F` | `INC`/`DEC r32` | REX prefix — 뒤따르는 명령에 붙는다 |
| `62` / `63` | `BOUND` / `ARPL` | EVEX prefix / `MOVSXD` |
| `C4` / `C5` | `LES` / `LDS` | VEX3 / VEX2 prefix |
| `A0`–`A3` | `mov eAX, moffs32` | `moffs64` — 길이가 5에서 9로 바뀐다 |
| ModRM `mod=00,rm=101` | 절대 `disp32` | `RIP`-relative |

마지막 줄이 가장 넓게 퍼집니다. 전역을 절대 주소로 읽는 것은 컴파일된 32비트 코드가
끊임없이 하는 일이기 때문입니다. 이 항목은 opcode가 아니라 addressing form이므로
opcode 목록으로는 잡히지 않아 별도로 검사합니다.

memory operand가 있는 명령은 어느 것도 `kIdenticalBytes`를 받지 않습니다. `67`
prefix가 32비트 주소 계산을 되살리지만, 그것이 정답이 되려면 guest memory가 하위
4 GiB에 있어야 하고 그 배치 결정은 Task 546 결정 4의 미결 항목입니다.

통과하는 subset은 register 대 register ALU와 `MOV`, 8·16·32비트입니다. Task 546이
"일반 GPR/flags"라고 부른 그 집합입니다.

### probe를 거부 중심으로 쓴 이유

통과 목록만 확인하는 probe는 **모든 것을 허용하는 판정기에 대해서도 통과**합니다.
따라서 `long_mode_compatibility` probe는 설계 A·B·C의 항목을 바이트로 만들어 하나도
`kIdenticalBytes`를 받지 않는지 확인하고, A의 여덟 항목은 개별 이름으로 보고합니다.

이 probe는 바이트를 해독해 판정할 뿐 실행하지 않으므로 모든 host에서 돌고 답이
같아야 합니다. x64 전용 fence 뒤가 아니라 공용 probe 목록에 둔 이유가 그것입니다 —
Windows나 i386 실행이 Linux x64와 다른 답을 내면 주장이 어긋났다는 뜻입니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Debug | `core_probe_all=true`, 15/15, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 16/16 |
| Linux x64 Release | `core_probe_all=true`, 15/15, skipped 2 |
| Win32 x86 Debug | `core_probe_all=true`, 16/16 |

Linux i386와 x64의 `long_mode_*` 출력 16줄을 `diff`로 비교해 동일함을 확인했습니다.

### 곁들여 확인한 것: Task 549의 x64 Release

Task 549는 Debug tree에서만 검증됐고, 같은 작업에서 찾은 결함이 `-O0`에서는 보이지
않는 종류였습니다. 그래서 이번에 x64 **Release** tree를 따로 만들어 돌렸습니다.
`fault_handler_all=true`, `linux_x64_aot_frame_all=true`, `host_thread_all=true`로
Debug와 같습니다. Task 549의 수정은 최적화 수준에 의존하지 않습니다.

그 과정에서 `scripts/build_linux_x64.sh`의 결함도 드러났습니다. Task 549가 추가한
그대로는 `--config Release`가 Debug tree를 **그 자리에서 reconfigure**해 검증된
tree를 지웠을 것입니다. 이제 build type이 다르면 거부하고 `--build-dir`을 안내합니다.
두 configuration을 동시에 두는 것이 요점입니다 — Task 549의 결함이 보인 것은 정확히
Debug와 Release를 나란히 비교할 수 있었기 때문입니다.

## English

### Result

Before starting step 3 of Task 546's implementation order -- the x64 emitter subset --
it became clear that the judgement that step depends on did not exist. Decision 5 of Task
546 says copied 32-bit bytes are to be treated as x86-only "unless the x64 emitter proves
their long-mode semantics", and nothing did the proving. This unit builds not the emitter
but **the boundary that decides what the emitter may touch**.

`ClassifyLongModeBytes` is fail-closed: `kUnsupported` is the default, and
`kIdenticalBytes` is returned only where identity is shown.

The point is that the risk of executing 32-bit bytes on x64 is **not faulting but being
quietly wrong**. Task 544 met `pusha`/`popa`, which the assembler refused outright -- the
lucky end of the range. The dangerous end is the encodings that become a different
instruction without raising anything.

| Bytes | 32-bit | 64-bit |
|---|---|---|
| `40`–`4F` | `INC`/`DEC r32` | REX prefix, applied to what follows |
| `62` / `63` | `BOUND` / `ARPL` | EVEX prefix / `MOVSXD` |
| `C4` / `C5` | `LES` / `LDS` | VEX3 / VEX2 prefix |
| `A0`–`A3` | `mov eAX, moffs32` | `moffs64`; length goes from five to nine |
| ModRM `mod=00,rm=101` | absolute `disp32` | `RIP`-relative |

The last row spreads furthest, because reading a global by absolute address is what
compiled 32-bit code does constantly. It is checked separately: it is an addressing form
rather than an opcode, so no opcode list catches it.

No instruction with a memory operand is answered `kIdenticalBytes`. A `67` prefix would
restore 32-bit address computation, but that is only correct while guest memory sits
below 4 GiB, and that placement is Task 546's still-open decision 4.

The admitted subset is register-to-register ALU and `MOV` at 8, 16, and 32 bits --
the "ordinary GPR/flags" set Task 546 named.

### Why the probe is written around refusals

A probe that checks only the pass list **also passes against a classifier that allows
everything**. So the `long_mode_compatibility` probe builds the bytes for the design's
lists A, B, and C, checks that not one of them is answered `kIdenticalBytes`, and reports
A's eight entries under their own names.

The probe decodes and judges bytes; it executes none of them, so it runs on every host
and must give the same answers everywhere. That is why it sits in the shared probe list
rather than behind an x64 fence -- a Windows or i386 run that disagreed with the Linux
x64 one would mean the claim had drifted.

### Verification

| Host | Result |
|---|---|
| Linux x64 Debug | `core_probe_all=true`, 15 of 15, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 16 of 16 |
| Linux x64 Release | `core_probe_all=true`, 15 of 15, 2 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 16 of 16 |

The sixteen `long_mode_*` lines from Linux i386 and x64 were compared with `diff` and are
identical.

### Checked alongside: Task 549 on x64 Release

Task 549 was verified on a Debug tree only, and the defect it found in the same unit was
of a kind `-O0` cannot show. So an x64 **Release** tree was built separately and run:
`fault_handler_all=true`, `linux_x64_aot_frame_all=true`, and `host_thread_all=true`,
the same as Debug. Task 549's fixes do not depend on the optimisation level.

Doing that exposed a flaw in `scripts/build_linux_x64.sh` as Task 549 added it: a
`--config Release` would have **reconfigured the Debug tree in place** and destroyed the
verified tree. It now refuses when the build type differs and points at `--build-dir`.
Keeping both configurations is the point -- Task 549's defect was visible precisely
because a Debug tree and a Release tree could be compared side by side.
