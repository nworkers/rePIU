# 20260901-558 x64에서 guest 상태를 어디에 두는가 작업 로그

설계: [20260901-558](../design/20260901-558-x64-guest-register-placement.md) ·
작업 지시: [20260901-558](../work-orders/20260901-558-x64-guest-register-placement.md)

## 한국어

### 결과 — x64가 emitter의 바이트를 처음으로 실행했습니다

```text
guest_register_emitted=true copied=5 lowered=2 refused=1
  guest_code=0x20000000 data=0x20001000 bytes=17
  eax observed=0x11224345 expected=0x11224345
  edx observed=0x11224345 expected=0x11224345
  esi observed=0x11224345 expected=0x11224345
  edi observed=0x11224345 expected=0x11224345
  ebp observed=0x11224345 expected=0x11224345
  r15 observed=0xabcdef expected=0xabcdef
guest_register_mapping=true,guest_esp_held=true
```

일곱 명령이 방출되어 실행됐고, 세 가지 방출 결과가 모두 들어 있습니다.

| guest 명령 | 방출 | 확인된 것 |
|---|---|---|
| `mov eax,[ebx+4]` | `0x67` lowering (552) | guest base로 **옳은 주소**를 읽음 |
| `inc eax` | `FF C0` 재인코딩 (557) | REX가 아니라 `INC`로 실행됨 |
| `add eax,ecx` 외 4개 | 복사 (550) | mapping이 항등이라 그대로 맞음 |

`0x11223344 + 1 + 0x1000 = 0x11224345`. **guest가 뜻한 값입니다.**

### 결정 — mapping은 고른 것이 아니라 강제된 것입니다

Task 555가 "적히지 않은 전제"로 남긴 것을 결정으로 바꿨습니다. guest GPR *n*은 host
GPR *n*에 있고, **다른 선택지가 없습니다** — `kIdenticalBytes`가 존재하는 한
`add eax, ebx`를 그대로 복사해 실행하는 것이 옳으려면 그 mapping이어야 합니다. 다른 것을
고르면 Tasks 550·552·553·557이 전부 무의미해집니다.

guest `ESP`만 다릅니다. `RSP`는 host의 SysV stack이므로(결정 3, Task 546) **`R15D`**에
둡니다.

확장 레지스터를 고른 이유가 메모리 슬롯보다 강합니다. **32비트 인코딩은 `R8`–`R15`를
이름 부를 수 없습니다** — REX가 필요한데 32비트에는 REX가 없고, 그 바이트는 거기서
`INC`/`DEC`입니다(Task 557). 즉 복사된 guest 명령이 guest `ESP`의 거처에 닿는 **인코딩
자체가 존재하지 않습니다.** 메모리 슬롯에는 그런 보장이 없습니다.

그리고 `R12`–`R15`는 callee-saved이므로 **SysV ABI가 대신 지켜 줍니다.** `R15`를 고른 것은
`R12`(base로 SIB 필요)와 `R13`(`mod=00`에서 disp8 필요)이 가진 인코딩 예외가 없기
때문입니다. `R14`는 다음 예약을 위해 비워 뒀습니다.

### 상위 절반이 0인 것을 실행으로 확인했습니다

`[r15]`로 접근할 때는 **64비트 `R15` 전체가 주소**입니다 — `0x67`이 붙은 operand가 base의
상위 절반을 아예 보지 않는 것과 다릅니다. 그래서 상위 절반이 0이어야 합니다.

bridge가 적재 **전에** `R15`를 `0xDEADBEEF00000000`으로 오염시킵니다. 그러지 않으면 0이
나와도 그것이 32비트 적재가 한 일인지 원래 0이었는지 알 수 없습니다. 오염 뒤
`mov r15d, [state+16]` 한 번으로 `0xabcdef`가 그대로 나왔습니다.

### `R15`가 살아남은 것은 시연이지 증명이 아닙니다

복사된 명령이 일곱 레지스터를 모두 쓰는 동안 `R15`가 그대로였습니다. 이것은 결정 2를
**시연**한 것이고, 증명은 인코딩 쪽에 있습니다 — 32비트 바이트가 `R15`를 지목할 방법이
없다는 것. probe는 그 논증을 반증할 기회를 준 것이고, 반증되지 않았습니다. 그 차이를
여기 적어 둡니다.

### 부수적으로 고친 것 — 32비트 호스트가 제외를 숨기고 있었습니다

`core_probe/main.cpp`는 파일 첫머리에 이렇게 적어 두었습니다.

> 제외하는 것은 통과하는 것과 다르다. `core_probe_skipped`가 그 구분을 화면에 남긴다 —
> "9 중 9 통과"만 찍히면 완전한 실행으로 읽히기 때문이다.

그런데 x64 전용 probe들은 그 목록에 없었습니다. 32비트 호스트는 `core_probe_total=19`,
`all=true`만 찍고 **세 개가 빌드되지 않았다는 사실을 말하지 않았습니다.** 그 파일이 막으려던
바로 그 읽힘이 반대편에서 다시 자라 있었습니다.

이제 이렇게 나옵니다.

```text
core_probe_skipped=3 linux_x64_aot_frame long_mode_lowering linux_x64_guest_register
core_probe_host=i386 (Task 558: the x64-only probes have nothing to test here)
```

`core_probe_host` 줄도 함께 고쳤습니다. 이전 구조에서는 i386 분기가 없어 조건을 풀면
i386이 자신을 `x64`라고 찍었을 것입니다.

### 측정

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, **20/20**, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

### 남은 것

이 결정 위에 **stack lowering**이 올라갑니다. `PUSH`/`POP`/`CALL`/`RET`/`LEAVE`를 `R15D`를
쓰는 명시적 시퀀스로 바꾸는 일이고, 다음 단위입니다. `R14`의 용도와 guest `EIP`를
register에 둘지는 그때 답합니다.

## English

### Result -- x64 ran the emitter's bytes for the first time

```text
guest_register_emitted=true copied=5 lowered=2 refused=1
  guest_code=0x20000000 data=0x20001000 bytes=17
  eax observed=0x11224345 expected=0x11224345
  edx observed=0x11224345 expected=0x11224345
  esi observed=0x11224345 expected=0x11224345
  edi observed=0x11224345 expected=0x11224345
  ebp observed=0x11224345 expected=0x11224345
  r15 observed=0xabcdef expected=0xabcdef
guest_register_mapping=true,guest_esp_held=true
```

Seven instructions emitted and executed, covering all three emission outcomes.

| Guest instruction | Emitted as | What it confirms |
|---|---|---|
| `mov eax,[ebx+4]` | `0x67` lowering (552) | reads **the right address** through a guest base |
| `inc eax` | `FF C0` re-encoding (557) | runs as `INC`, not as a REX prefix |
| `add eax,ecx` and four more | copied (550) | the identity mapping makes a copy correct |

`0x11223344 + 1 + 0x1000 = 0x11224345`. **The value the guest meant.**

### The decision -- the mapping is forced, not chosen

Task 555's "unwritten premise" is now a decision. Guest GPR *n* is in host GPR *n*, and
**there is no alternative**: as long as `kIdenticalBytes` exists, copying `add eax, ebx`
byte for byte is correct only under that mapping. Choosing another erases Tasks 550, 552,
553 and 557.

Only guest `ESP` differs. `RSP` is the host's SysV stack (decision 3, Task 546), so guest
`ESP` lives in **`R15D`**.

The reason for an extended register is stronger than for a memory slot. **A 32-bit encoding
cannot name `R8`-`R15`** -- that needs a REX prefix, 32-bit mode has none, and those bytes
are `INC`/`DEC` there (Task 557). So **no encoding exists** by which a copied guest
instruction could reach guest `ESP`'s home. A memory slot carries no such guarantee.

And `R12`-`R15` are callee-saved, so **the SysV ABI preserves it for us**. `R15` rather
than `R12` (needs a SIB byte as a base) or `R13` (needs a disp8 at `mod=00`), which carry
encoding exceptions. `R14` is left free for the next reservation.

### The zero upper half was confirmed by execution

An access through `[r15]` uses **the whole 64-bit register as the address** -- unlike a
`0x67`-prefixed operand, which never looks at the base's upper half. So the upper half must
be zero.

The bridge poisons `R15` with `0xDEADBEEF00000000` **before** the load. Without that, a
zero result could not distinguish the 32-bit load's doing from what happened to be there.
After the poison, one `mov r15d, [state+16]` produced `0xabcdef` exactly.

### `R15` surviving is a demonstration, not a proof

`R15` was untouched while copied instructions wrote all seven mapped registers. That
**demonstrates** decision 2; the proof is on the encoding side -- no 32-bit byte string can
name `R15`. The probe gave that argument a chance to be falsified and it was not. The
difference is worth writing down.

### Fixed along the way -- 32-bit hosts were hiding their exclusions

`core_probe/main.cpp` says this at the top of the file:

> Excluding them is not the same as their passing. `core_probe_skipped` keeps that
> distinction on screen, because "9 of 9 passed" printed alone would read as a complete
> run.

The x64-only probes were not in that list. A 32-bit host printed `core_probe_total=19` and
`all=true` and **said nothing about the three that had not been built** -- the very reading
that file exists to prevent, grown back on the other side.

It now prints:

```text
core_probe_skipped=3 linux_x64_aot_frame long_mode_lowering linux_x64_guest_register
core_probe_host=i386 (Task 558: the x64-only probes have nothing to test here)
```

The `core_probe_host` line was corrected with it: the old structure had no i386 arm, so
unguarding it would have had i386 announcing itself as `x64`.

### What was measured

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, **20 of 20**, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

### What is left

The **stack lowering** goes on top of this decision: turning `PUSH`, `POP`, `CALL`, `RET`
and `LEAVE` into explicit sequences that use `R15D`. That is the next unit, and it is where
`R14`'s purpose and whether guest `EIP` wants a register get answered.
