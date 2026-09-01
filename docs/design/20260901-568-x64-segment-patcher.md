# 설계 20260901-568 — x64 segment override를 실제 patcher에 연결

## 문제

Task 567은 slot을 만들고 검증했지만 `enable_long_mode_segment_override`를
false로 두었다. patch하는 것이 없기 때문이다. 그런데 patcher는 이미 있다 —
`ReResolveWin32AotSegmentOverrides`. 없는 것은 patcher가 아니라 **x64 slot에도
맞는** patcher다.

읽어보니 이렇게 되어 있다.

```cpp
// Restore the full guard prefix because HLE routing overwrites its
// first five bytes with JMP rel32.
bytes[site.cache_offset]      = 0x9CU;
bytes[site.cache_offset + 1U] = 0x66U;
bytes[site.cache_offset + 2U] = 0x81U;
bytes[site.cache_offset + 3U] = 0x3DU;
```

`9C 66 81 3D`는 i386 emitter가 slot 앞에 쓴 바이트다. patcher가 그것을 **복제해
들고 있다.** x64 slot은 lowered pushfd로 열리므로 앞이 다르다.

```
i386 : 9C 66 81 3D <abs32> <imm16>
x64  : 9C 41 5E 45 8D 7F FC 45 89 37   67 66 81 3C 25 <abs32> <imm16>
```

x64에 저 네 줄을 적용하면 `41 5E 45`가 `66 81 3D`로 덮인다. slot이 조용히
망가진다.

이번 세션에서 같은 모양의 버그를 이미 두 번 봤다. census가 emitter의 `#if`를
복제해 `agrees=false`가 났고(562), probe들이 지난 규칙을 복제해 네 번 틀렸다.
소비자가 생산자의 지식을 복제하면 규칙이 바뀌는 순간 어긋난다.

## 결정 1 — 복원 바이트를 site가 들고 다닌다

patcher가 상수를 갖는 대신 emitter가 `guard_prologue`에 자기가 쓴 앞 바이트를
기록하고, patcher는 그것을 되돌린다. 진실이 한 곳에만 있다.

길이는 5로 고정한다. HLE 라우팅이 덮는 것이 `JMP rel32` = 5바이트이므로,
되돌려야 할 것도 정확히 그 5바이트다. i386의 현재 코드가 4바이트만 되돌리고도
맞았던 것은 다섯째 바이트가 abs32의 첫 바이트라 뒤이어 덮어써지기 때문인데,
이건 우연에 기대는 형태라 5로 통일한다.

## 결정 2 — 기본값(fallback)을 두지 않는다

`guard_prologue_size == 0`일 때 예전 상수를 쓰는 호환 경로를 만들 수 있다.
만들지 않는다. 그 상수가 바로 없애려는 복제이고, 하나 남겨두면 i386은 계속
그 경로로 흘러 새 계약이 검증되지 않는다.

대신 기존 `selector_guard` probe가 이 계약을 강제한다. 그 probe는
`bytes[0] == 0x9C`를 단언하므로, 채우는 것을 빠뜨리면 **조용히 지나가지 않고
실패한다.** 손으로 site를 만드는 곳은 emitter 역할을 대신하는 것이니 emitter가
하는 일을 해야 한다.

## 결정 3 — 검증은 진짜 patcher로 한다

Task 567의 probe는 memcpy로 자기가 patch했다. 그것으로는 이 단위를 검증할 수
없다 — 확인해야 할 대상이 patcher 자신이기 때문이다.

x64 probe가 `AotCodeCachePlacement`를 만들어 실제
`ReResolveWin32AotSegmentOverrides`를 부르고, 그 뒤에 실행한다. selector 일치와
불일치 양쪽 모두. `selector_guard` probe가 이미 이 모양으로 placement를 손으로
세우므로 새로운 방식이 아니다.

여기서 Task 546 결정 4(코드 캐시를 하위 4 GiB에 둔다)가 값을 한다.
`placement.base_address`는 `std::uint32_t`다. 캐시가 4 GiB 위에 있었다면 이
구조체가 주소를 담지 못했을 것이다. probe는 이것을 가정하지 않고 단언한다.

## 결정 4 — 기본값을 켤지는 측정 뒤에 정한다

이 단위가 끝나면 slot은 진짜로 patch된다. 그때 `enable_long_mode_segment_override`
기본값을 true로 올릴 수 있다. 다만 올린 뒤의 census 수치를 보고 결정한다.
Task 567이 예측한 값은 8 → 9이고, **그 예측이 맞는지 확인하는 것이 이 단위의
부수적인 성과다.** 예측과 다르면 예측이 틀린 것이고, 그 사실을 기록한다.

## 하지 않는 것

- FS/GS. 계속 거부한다.
- `ReResolveWin32AotSegmentOverrides`의 이름에 남은 `Win32`. 잘못된 이름이지만
  (Task 545가 엔진 코드에서 Win32를 걷어냈는데 이것이 남았다) 호출부가 많아
  이 단위에 섞으면 실제 변경이 이름 변경에 묻힌다. 별도 단위로 남긴다.

---

# Design 20260901-568 — connecting the x64 segment override to the real patcher

## Problem

Task 567 built and verified the slot but left
`enable_long_mode_segment_override` false, because nothing patched it. Yet a
patcher already exists: `ReResolveWin32AotSegmentOverrides`. What is missing is
not a patcher but one that also **fits the x64 slot**.

Reading it:

```cpp
// Restore the full guard prefix because HLE routing overwrites its
// first five bytes with JMP rel32.
bytes[site.cache_offset]      = 0x9CU;
bytes[site.cache_offset + 1U] = 0x66U;
bytes[site.cache_offset + 2U] = 0x81U;
bytes[site.cache_offset + 3U] = 0x3DU;
```

`9C 66 81 3D` is what the i386 emitter wrote at the head of the slot. The
patcher holds a **copy of it**. The x64 slot opens with a lowered `pushfd`, so
its head is different:

```
i386 : 9C 66 81 3D <abs32> <imm16>
x64  : 9C 41 5E 45 8D 7F FC 45 89 37   67 66 81 3C 25 <abs32> <imm16>
```

Applying those four lines to the x64 slot overwrites `41 5E 45` with
`66 81 3D`. The slot breaks quietly.

The same shape of bug has appeared twice already this session: the census
copied the emitter's `#if` and reported `agrees=false` (562), and probes copied
rules that had moved on, four times. When a consumer duplicates a producer's
knowledge, the two diverge the moment the rule changes.

## Decision 1 — the site carries its own restore bytes

Instead of a constant in the patcher, the emitter records the head it wrote in
`guard_prologue`, and the patcher puts that back. One place holds the truth.

The length is fixed at five. HLE routing overwrites `JMP rel32` -- five bytes --
so five is exactly what must be restored. The current i386 code restores only
four and is still correct because the fifth byte is the first byte of the
abs32, overwritten immediately afterwards; that relies on a coincidence, so
five it is.

## Decision 2 — no fallback

A compatibility path could use the old constant when
`guard_prologue_size == 0`. It will not exist. That constant is the very
duplication being removed, and leaving one behind means i386 keeps flowing
through it and the new contract never gets exercised.

The existing `selector_guard` probe enforces the contract instead: it asserts
`bytes[0] == 0x9C`, so forgetting to fill the field **fails loudly rather than
passing quietly**. Anywhere a site is built by hand is standing in for an
emitter, and must do what an emitter does.

## Decision 3 — verify with the real patcher

Task 567's probe patched its own image with `memcpy`. That cannot verify this
unit, because the patcher is the thing under test.

The x64 probe builds an `AotCodeCachePlacement`, calls the real
`ReResolveWin32AotSegmentOverrides`, and only then executes -- both the
matching and the mismatching selector. The `selector_guard` probe already
stands a placement up by hand, so this is not a new technique.

Task 546's decision 4 (place the code cache below 4 GiB) earns its keep here:
`placement.base_address` is a `std::uint32_t`. Had the cache gone above 4 GiB
the struct could not hold the address. The probe asserts this rather than
assuming it.

## Decision 4 — whether to flip the default is decided after measuring

When this unit lands the slot really is patched, and
`enable_long_mode_segment_override` could default to true. That call is made
after looking at the census, not before. Task 567 predicted 8 -> 9, and
**checking that prediction is this unit's secondary result.** If the number
differs, the prediction was wrong, and that gets written down.

## Not in scope

- FS/GS, which stay refused.
- The `Win32` left in `ReResolveWin32AotSegmentOverrides`'s name. It is a
  misnomer (Task 545 took Win32 out of the engine code and missed this one) but
  it has many call sites, and folding a rename into this unit would bury the
  real change inside it. Its own unit.
