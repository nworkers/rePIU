# 20260901-564 x64 `ESP` operand 재인코딩 작업 로그

## 한국어

### 결과 — 그리고 이 단위는 자기 기준으로는 실패했습니다

| 항목 | 563 | 564 |
|---|---:|---:|
| 방출 가능 | 86.46% | **96.74%** |
| 완결 block | 64.13% | **83.01%** |
| `stack-pointer` 거부 | 6,401 | **234** |
| **도달 가능 block** | 1 | **1** |
| 정지 지점 | `0x10f4c31` `stack-pointer` | `0x10f4c46` `silently-different` |

설계에 **"방출 가능 비율이 아니라 도달 가능 block이 성패를 말한다"** 고 적었고, 그 수는
움직이지 않았습니다. 장애물이 같은 block 안에서 21바이트 앞으로 갔을 뿐입니다.

이것을 실패로 적는 이유는, 성공 기준을 미리 적어 두었기 때문입니다. 커버리지 두 수치가
크게 올랐으니 그것으로 성공을 말할 수는 있지만, 그건 Task 563이 방금 "다른 질문에
답하는 수"라고 확인한 바로 그 수치입니다.

> 하나의 장애물을 걷어내면 그 뒤의 것이 드러난다. §8이 네 번 만난 형태이고, 이번에는
> 걷어낸 즉시 드러났다.

### 재인코딩 자체는 맞습니다

`ESP`는 encoding 상 세 곳에 나타나고, 셋 다 "필드를 `111`로, 대응하는 `REX` 비트를"
이라는 한 가지 변환입니다.

| 자리 | 예 | 결과 |
|---|---|---|
| ModRM `rm` | `83 C4 10` `add esp,16` | `41 83 C7 10` `add r15d,16` |
| ModRM `reg` | `89 E2` `mov edx,esp` | `41 89 FA` |
| SIB `base` | `8B 44 24 08` `mov eax,[esp+8]` | `41 8B 44 27 08` |

실행으로 확인했습니다.

```text
esp_memory_base      observed=0xc0ffee01 expected=0xc0ffee01
esp_register_operand observed=0x20001810 expected=0x20001810
esp_as_source        observed=0x20001810 expected=0x20001810
```

**세 번째 줄의 값보다 중요한 것은 실행이 돌아왔다는 사실입니다.** 재인코딩이 없었다면
`add esp,16`이 host `RSP`를 16 옮겨 복귀 주소를 엉뚱한 곳에서 읽었을 것이고, 값을
비교할 기회조차 없었을 것입니다.

### 남긴 것 — 32비트 wraparound

메모리 base를 `R15`로 바꾼 뒤 `0x67`을 붙이지 않습니다. 상위 절반이 0이고 guest memory가
하위 4 GiB에 있으므로 64비트 계산 결과가 곧 guest 주소입니다. Task 559의 push 시퀀스와
같은 관례입니다.

**대신 `ESP + disp`가 32비트를 넘어 감싸는 경우가 보존되지 않습니다.** arena가
`0x085E7000` 아래라 현재 구성에서는 일어날 수 없지만 **우연한 안전이지 규칙이
아닙니다.**

### 세 번째로 probe가 옛 규칙을 주장했습니다

`long_mode_compatibility`와 `long_mode_emission` 둘이 "`ESP`는 거부"를 검사하다
빨개졌습니다. Task 562의 `kReturn`, 그 전 Task 561의 검증기에 이어 세 번째입니다.

> 규칙을 바꾸면 그 규칙을 검사하던 것도 함께 바뀌어야 한다. 빨개지는 것이 그것을
> 알려주는 방법이다.

두 probe의 주석에 그 사실을 적었습니다. `ProbeStackPointerRefusal`은 이름과 달리 이제
재인코딩을 검사하지만, 확인하는 위험은 그대로입니다 — **`kIdenticalBytes`가 되어서는
안 된다**는 것.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20/20, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

census `agrees=true`.

### 다음

정지 지점이 `silently-different`로 바뀌었고, image 전체 682개 중 **681개가 `mov`**
입니다 — moffs 형태(`A0`–`A3`)일 것이 거의 확실합니다. 변환은 Task 552가 이미 만든
`kAbsoluteToSib`와 같은 형태(`67 8B 04 25 disp32`)입니다.

**다만 그 다음에 또 무엇이 나올지는 알 수 없습니다.** 이번 단위가 보여준 것이 정확히
그것이므로, 다음 단위도 도달 가능 block으로 성패를 재야 합니다.

## English

### Result -- and by its own criterion this unit failed

| Item | 563 | 564 |
|---|---:|---:|
| Emittable | 86.46% | **96.74%** |
| Complete blocks | 64.13% | **83.01%** |
| `stack-pointer` refusals | 6,401 | **234** |
| **Reachable blocks** | 1 | **1** |
| Stopping point | `0x10f4c31` `stack-pointer` | `0x10f4c46` `silently-different` |

The design said **"reachable blocks decide this, not the emittable fraction"**, and that
number did not move. The obstruction advanced twenty-one bytes inside the same block.

This is recorded as a failure because the criterion was written down first. The two
coverage numbers rose a great deal and could be called success -- but they are exactly the
numbers Task 563 had just shown answer a different question.

> Remove one obstruction and the next appears. §8's shape, met a fifth time, and this time
> it appeared the moment the first was cleared.

### The re-encoding itself is right

`ESP` appears in three encoding places, and all three are one transform -- the field to
`111`, the matching `REX` bit:

| Place | Example | Result |
|---|---|---|
| ModRM `rm` | `83 C4 10` `add esp,16` | `41 83 C7 10` `add r15d,16` |
| ModRM `reg` | `89 E2` `mov edx,esp` | `41 89 FA` |
| SIB `base` | `8B 44 24 08` `mov eax,[esp+8]` | `41 8B 44 27 08` |

**What matters more than the third line's value is that the run came back at all.**
Without the re-encoding, `add esp,16` would have moved the host's `RSP` by sixteen and the
return address would have been read from the wrong place -- there would have been no
chance to compare a value.

### What this gives up: 32-bit wraparound

No `0x67` is added after the base becomes `R15`: the upper half is zero and guest memory
is below 4 GiB, so the 64-bit result is the guest address, which is Task 559's convention.
**An `ESP + disp` that wrapped past 32 bits would not wrap here.** The arena ends below
`0x085E7000` so it cannot happen in this configuration -- accidental safety, not a rule.

### A probe asserted the past, for the third time

`long_mode_compatibility` and `long_mode_emission` both checked "`ESP` is refused" and went
red. After Task 562's `kReturn` and Task 561's verifier, this is the third.

> Change a rule and the thing checking it has to change with it. Going red is how that
> gets said.

Both probes now say so in their comments. `ProbeStackPointerRefusal` checks a re-encoding
despite its name, but the danger it guards is unchanged: these must never become
`kIdenticalBytes`.

### Verification

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20 of 20, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

### Next

The stopping point is now `silently-different`, and **681 of the image's 682 are `mov`** --
almost certainly the moffs forms (`A0`–`A3`). The transform is the one Task 552 already
built, `kAbsoluteToSib` (`67 8B 04 25 disp32`).

**What comes after that is not knowable in advance**, which is precisely what this unit
demonstrated, so the next one must be judged by reachable blocks too.
