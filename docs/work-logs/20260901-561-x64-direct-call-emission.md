# 20260901-561 x64 direct call emission 작업 로그

## 한국어

### 결과

`kDirectCall`을 long-mode emission에서 방출합니다.

| 항목 | 559 | 560 | 561 |
|---|---:|---:|---:|
| branches | — | 6,847 | **11,051** |
| 방출 가능 | 66.17% | 77.60% | **84.62%** |
| **완결 block** | 2.66% | 38.32% | **59.90%** |
| non-copy 거부 | 12,856 | 6,009 | **1,805** |

늘어난 4,204는 정확히 `kDirectCall` 레코드 수입니다.

### 새 시퀀스를 쓰지 않았습니다

i386 call slot은 "복귀 주소 push + 대상으로 jump"이고, x64에는 두 조각이 이미
있었습니다 — Task 559의 stack lowering과 Task 560의 direct edge. 그래서 push 바이트를
직접 쓰지 않고 `{0x68, fallthrough}`를 합성해 기존 lowering에 통과시킵니다.

```text
45 8D 7F FC          lea r15d, [r15-4]        ESP -= 4, flag 불변
41 C7 07 <imm32>     mov dword ptr [r15], 복귀 주소
```

Task 559가 `LEA`를 쓰는 이유(guest `PUSH`는 flag를 바꾸지 않음)는 틀려 보고 얻은
것입니다. 같은 시퀀스를 이 파일에 다시 쓰면 그것이 다시 틀릴 자리가 하나 더 생깁니다.

### 실행으로 확인했습니다

```text
call_reached_callee   observed=0x3333 expected=0x3333
call_return_address   observed=0x14000a expected=0x14000a
call_esp              observed=0x200017fc expected=0x200017fc
```

점프가 갔는지만 봤다면 **call이 아니라 jump를 확인한 것**입니다. call을 call로 만드는
것은 guest stack에 남는 복귀 주소이므로, register가 아니라 guest 메모리를 직접 읽어
확인했습니다.

### 오류 경로를 시험해서 두 번 틀린 것을 잡았습니다

미해결 call 경로에 전용 검사를 붙였고, 그것이 연달아 두 결함을 드러냈습니다.

**첫째: push가 trap보다 먼저 실행됩니다.** 처음에는 `E9`만 `0xCC`로 덮었습니다. call은
push가 jump 앞에 있으므로 guest ESP가 내려가고 복귀 주소가 쓰인 뒤에 trap하고,
boundary 핸들러는 guest의 `call`에서 재개하므로 **두 번 push**하게 됩니다. 주석에는
그 반대를 적어 두기까지 했습니다.

**둘째: 검증기가 이미지를 거부했습니다.** slot 시작을 덮도록 고치자 이렇게 나왔습니다.

```text
guest_unresolved_call=false message="emitted code cache failed decode verification"
```

entry는 "3 명령"이라고 말하는데 바이트는 `INT3` + 잔여물이 됐기 때문입니다. Task 559가
정확하게 만들어 둔 검증기가 정확히 그 불일치를 잡았습니다. slot 전체를 `INT3`로 채우고
의도 명령 수를 길이에 맞추자 통과했습니다 — trap 하나가 명령 하나이므로 스스로
일관됩니다.

**그리고 이 두 번째 결함은 Task 560의 경로에도 이미 있었습니다.** 실제 이미지에서
`unresolved=0`이라 한 번도 실행되지 않아 드러나지 않았을 뿐입니다. Task 560 로그에
"검증되지 않은 안전망"이라고 적어 둔 그것이 실제로 고장나 있었습니다.

> 실행되지 않는 오류 경로는 작동한다는 증거가 없는 코드다.

### 이것만으로 실행이 이어지지는 않습니다

call을 방출해도 피호출자의 `ret`은 여전히 boundary입니다. guest stack에는 올바른 복귀
주소가 들어가지만 그 guest 주소를 cache 주소로 바꿀 dispatch resolver가 없습니다.
늘어난 것은 **방출 범위와 완결 block**이고, 체인이 실제로 길어지는 것은 `kReturn`의
resolver가 생긴 뒤입니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20/20, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

census `agrees=true`, `branch edges emitted=11051 unresolved=0`.

### 다음

남은 non-copy 1,805 중 `kReturn` 1,105(61%)가 최대이고, dispatch resolver를 요구합니다.

## English

### Result

`kDirectCall` is emitted under long-mode emission.

| Item | 559 | 560 | 561 |
|---|---:|---:|---:|
| branches | — | 6,847 | **11,051** |
| Emittable | 66.17% | 77.60% | **84.62%** |
| **Complete blocks** | 2.66% | 38.32% | **59.90%** |
| non-copy refusals | 12,856 | 6,009 | **1,805** |

The 4,204 added is exactly the number of `kDirectCall` records.

### No new sequence was written

The i386 slot is a push of the return address followed by a jump, and x64 already had both
halves: Task 559's stack lowering and Task 560's direct edge. So the push is synthesised
as `{0x68, fallthrough}` and passed through the existing lowering rather than written here.

Task 559's reason for using `LEA` -- a guest `PUSH` changes no flags -- was learned by
getting it wrong. A second copy of that sequence in this file would be a second place for
it to be got wrong again.

### Confirmed by execution

Checking only that the jump arrived would have confirmed **a jump, not a call**. What
makes it a call is the return address left on the guest stack, so that was read out of
guest memory rather than from a register.

### Testing the error path caught two wrong things

A dedicated check for the unresolved-call path exposed two defects in a row.

**First: the push runs before the trap.** The first version overwrote only the `E9`. A
call's push precedes its jump, so guest ESP would move and the return address be stored
before trapping -- and the boundary's handler resumes at the guest's own `call`, pushing a
second time. The comment even claimed the opposite.

**Second: the verifier refused the image.** With the slot start overwritten instead:

```text
guest_unresolved_call=false message="emitted code cache failed decode verification"
```

The entry claimed three instructions while its bytes had become a trap plus leftovers.
The check Task 559 made exact found exactly that disagreement. Filling the whole slot with
`INT3` and setting the intended count to its length passed -- one trap is one instruction,
so the slot is self-consistent.

**And that second defect was already in Task 560's path**, invisible only because no edge
ever went unresolved on the real image. What Task 560's log called an unexercised safety
net was in fact broken.

> An error path that never runs is code with no evidence it works.

### This alone does not make execution continue

The callee's `ret` is still a boundary. The correct return address reaches the guest
stack, but nothing turns that guest address into a cache address. What rose is emission
coverage and complete blocks; chains lengthen once `kReturn` has its resolver.

### Verification

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20 of 20, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

The census reports `agrees=true` and `branch edges emitted=11051 unresolved=0`.

### Next

Of the 1,805 non-copy records left, `kReturn` is 1,105 (61%) and needs the dispatch
resolver.
