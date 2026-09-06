# 20260906-614 Linux x64 high-byte source 재인코딩 설계

## 한국어

### 배경

Task 613의 dynamic-generation probe로 allocator 경계 직전의 실제 AOT
바이트를 확인했습니다.

```text
guest:   mov byte ptr [esp], ah
lowered: 41 88 24 27
```

32-bit guest에서 ModRM `reg=100`은 `AH`입니다. 그러나 long mode에서 REX
prefix가 있으면 같은 필드는 `SPL`을 뜻합니다. 이 lowering은 `ESP`를
`R15`로 바꾸기 위해 REX를 반드시 넣으므로, guest `AH` 대신 host `RSP`의
low byte를 guest stack에 저장합니다. 그 결과 다음 `CMP`/`JNZ`가 잘못된
branch를 선택하고 allocator helper 호출을 건너뜁니다.

### 목표

* REX가 필요한 `ESP` memory operand와 ModRM `reg`의 high-byte source가
  함께 있는 명령을 원래 의미대로 실행합니다.
* `AH`, `CH`, `DH`, `BH`를 각각 원래 32-bit GPR의 high byte에서 추출합니다.
* guest flags와 guest GPR을 보존하면서 emitter scratch인 `R14D`만 임시로
  사용합니다.
* 아직 증명되지 않은 high-byte destination 및 read/write 교환형은 거절하여
  조용한 오동작을 만들지 않습니다.

### 변환

source high byte가 ModRM `reg`에 있고 memory operand의 base가 guest `ESP`인
경우에만 다음을 적용합니다.

```text
 xchg low8, high8             ; legacy encoding, flags unchanged
 mov r14b, low8               ; original high byte is now in low8
 xchg low8, high8             ; restore source GPR, flags unchanged
<original byte operation>    ; ModRM reg -> R14B, ESP -> R15
```

예를 들어 `mov [esp], ah`는 다음이 됩니다.

```text
86 C4                         xchg al,ah
41 88 C6                      mov r14b,al
86 C4                         xchg al,ah
45 88 34 27                  mov byte ptr [r15],r14b
```

`XCHG`와 `MOV`는 EFLAGS를 변경하지 않으므로 `MOV`뿐 아니라 source-only
byte 연산에도 원래 flags 의미를 유지합니다. `R14D`는 기존 stack lowering이
정한 emitter scratch이며, 32-bit guest bytes가 직접 이름 부를 수 없는
callee-saved host register입니다.

### 판정 경계

* `AH/CH/DH/BH`가 읽기 전용 source이고 `operand.encoding`이 ModRM `reg`인
  경우만 새 lowering으로 분류합니다.
* high-byte destination, `XCHG` 같은 read/write operand, implicit 또는
  다른 encoding은 `kStackPointerRegister`로 거절합니다.
* high-byte를 사용하지 않는 기존 `ESP` 명령은 기존
  `kStackPointerToR15` 경로를 그대로 사용합니다.
* high-byte 명령을 x64 원본 bytes로 복사하거나 host `SPL`로 실행하는
  fallback은 허용하지 않습니다.

### 검증 전략

1. long-mode lowering probe에서 `mov [esp],ah`가 새 verdict와 정확한
   emitted bytes를 얻는지 확인합니다.
2. 실행 probe에서 `R14D` scratch를 사용한 변환이 실제로 guest stack에
   `AH` 값을 저장하고, flags가 보존되는지 확인합니다.
3. `pumpit2a`를 재빌드하고 `0x010F1E13`, `0x010F1E17`,
   `0x010F1E1C`, `0x010F4FE8` probe를 다시 실행하여 branch 및 helper
   도달 여부를 관찰합니다.
4. 기본 실행의 기존 DOS 종료 경계와 새 frontier를 비교합니다.

## English

### Background

Task 613's dynamic-generation probe exposed the actual AOT bytes immediately
before the allocator boundary:

```text
guest:   mov byte ptr [esp], ah
lowered: 41 88 24 27
```

In 32-bit guest code, ModRM `reg=100` names `AH`. With any REX prefix in long
mode, the same field names `SPL`. Since the `ESP` lowering must insert a REX to
name `R15`, the current lowering stores the low byte of host `RSP` instead of
guest `AH`. The following compare and conditional branch therefore take the
wrong path and skip the allocator helper call.

### Goal

* Preserve instructions that combine an `ESP` memory operand with a ModRM
  high-byte source.
* Extract `AH`, `CH`, `DH`, and `BH` from their original 32-bit GPRs.
* Preserve guest flags and guest GPRs while using only the emitter scratch
  `R14D`.
* Keep unproven high-byte destinations and read/write exchanges fail-closed.

### Rewrite

Only a high-byte source in ModRM `reg` with a guest-`ESP` memory base is
admitted:

```text
 xchg low8, high8             ; legacy encoding, flags unchanged
 mov r14b, low8               ; original high byte is now in low8
 xchg low8, high8             ; restore source GPR, flags unchanged
<original byte operation>    ; ModRM reg -> R14B, ESP -> R15
```

For example, `mov [esp], ah` becomes:

```text
86 C4                         xchg al,ah
41 88 C6                      mov r14b,al
86 C4                         xchg al,ah
45 88 34 27                  mov byte ptr [r15],r14b
```

`XCHG` and `MOV` do not modify EFLAGS, so the original flags behavior is
preserved for source-only byte operations, including `MOV`. `R14D` is the emitter scratch
already established by the stack lowering design; 32-bit guest bytes cannot
name that callee-saved host register directly.

### Boundaries

* Admit only read-only `AH/CH/DH/BH` operands encoded in ModRM `reg`.
* Refuse high-byte destinations, read/write operands such as `XCHG`, implicit
  forms, and other encodings with `kStackPointerRegister`.
* Leave existing `ESP` instructions without high-byte operands on the existing
  `kStackPointerToR15` path.
* Never copy a high-byte instruction unchanged or allow it to execute using
  host `SPL`.

### Verification

1. Assert the new verdict and exact emitted bytes for `mov [esp],ah` in the
   long-mode lowering probe.
2. Execute the rewrite in a synthetic probe and verify that guest stack memory
   receives `AH` while flags remain unchanged.
3. Rebuild `pumpit2a` and repeat probes at `0x010F1E13`, `0x010F1E17`,
   `0x010F1E1C`, and `0x010F4FE8` to observe branch and helper reachability.
4. Compare the default DOS termination boundary and the new frontier.
