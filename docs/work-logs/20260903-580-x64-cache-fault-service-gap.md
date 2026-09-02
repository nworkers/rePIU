# 작업 기록 20260903-580 — cache 안 폴트를 서비스하는 경로가 없다

설계·작업 지시: 이 단위는 코드 변경이 한 줄짜리 도구 보완뿐인 **진단 단위**이고,
설계는 [20260903-579](../design/20260903-579-emitted-cache-dump.md)의 결정 3이
예고한 후속입니다.

## 먼저 — Task 579의 구현이 설계를 따르지 않았습니다

Task 579의 설계는 덤프에 찍을 열을 표로 정해 두었고 그중 하나가 **kind**였습니다.
구현이 그것을 빠뜨렸습니다.

**그 누락이 조사 하나를 통째로 낭비하게 했습니다.** `fb`만 보고 `kCopy`를 보지
못하면 "거절되어 INT3가 된 것인가, 일부러 복사된 것인가"가 열린 채로 남는데,
그것이 바로 물어야 할 질문이었습니다. 이번에 열을 더했습니다.

```text
     cache=0x0 len=5  guest=0x10f16b0  kind=kDirectJump
        emitted: e9 00 00 00 00
  >> cache=0x5 len=1  guest=0x10f1728  kind=kCopy
        emitted: fb
     cache=0x6 len=4  guest=0x10f1729  kind=kCopy
        emitted: 41 83 e7 fc
```

`kind=kCopy`입니다. **planner가 `sti`를 `kCopy`로 표시합니다** — i386 emitter도
같은 `fb`를 복사합니다. 즉 방출은 두 호스트에서 같습니다.

## 진단 — 끊긴 곳

### 확인된 것

1. guest `0x10f1728`의 `sti`는 `kCopy`이고 long-mode cache에 `fb`로 방출됩니다.
2. `STI`는 CPL 3에서 #GP를 일으키고, Linux는 `SIGSEGV`(0xb)·`si_addr=0`으로
   전달합니다 — Task 578의 관측과 정확히 일치합니다.
3. `HandlePrivilegedTrapInstruction`은 **`Eip`에서 직접 바이트를 읽고**
   `IsGuestRangeReadable`을 요구합니다. 그 함수는
   `runtime_base ≤ 주소 < runtime_base + runtime_size`, 즉 **게스트 arena만**
   허용합니다. cache는 `0x20000000`으로 그 밖입니다.
4. cache 주소를 guest 주소로 되돌리는 곳은 `HandleAotReentry`이고, 그것은
   **`fault.kind == kBreakpoint`일 때만** 합니다
   (`src/engine/aot/aot_runtime_dispatch.cpp`).
5. `sti`의 #GP는 breakpoint가 아니라 **access violation**입니다.

**따라서 cache 안에서 일어난 access-violation 폴트를 guest 주소로 되돌리는 경로가
없습니다.** `Eip`가 cache 주소로 남으므로, `Eip`에서 바이트를 읽는 모든 HLE
handler가 거절합니다.

### 왜 i386에서는 드러나지 않았는가 — 추정

같은 ROM(`pumpit2a`)으로 i386 `repiu`를 돌리면 **42초를 계속 실행합니다**
(exit 124는 제 timeout이지 게임의 정지가 아닙니다).

```text
[repiu-live] elapsed_ms=42699 ... last_eip=0x010F2786
             single_step=14304 aot=14481/197211
```

`repiu-fault` 줄은 **한 번도 나오지 않았습니다.**

i386이 이 지점을 넘기는 이유는 **추정**입니다 — single-step 비중이 크고 cache
진입이 dispatch를 거치므로, entry 영역의 `sti`를 cache에서 실행하지 않을
가능성이 높습니다. **그 블록이 실제로 어떻게 실행되는지는 재지 않았습니다.**

x64는 다릅니다. Task 578의 진입은 프로그램의 **첫 block부터 곧장 cache**로
들어가므로, 두 번째 block에서 곧바로 그 공백에 닿습니다.

## 이것이 Task 578의 구조적 선택을 되묻게 합니다

x64 진입이 cache로 곧장 들어가는 것은 Task 578 설계 결정 1이었고, 그때는 그것이
유일한 선택으로 보였습니다 — 게스트 바이트는 long mode에서 실행할 수 없으니까요.

그 판단 자체는 여전히 옳습니다. 다만 **그 결과로 i386이 통과해 온 경로를
건너뛰었다**는 것이 이번에 드러났습니다. 다음 단위가 고를 것은 둘 중 하나입니다.

- cache 안의 access-violation 폴트도 guest 주소로 되돌리게 한다(공백을 메운다).
- x64도 dispatch를 거쳐 cache에 들어가게 한다(i386 경로를 따라간다).

어느 쪽인지는 이 단위에서 정하지 않습니다. 첫 번째가 작아 보이지만, 서비스 후
`++Eip`한 guest 주소를 다시 cache 주소로 되돌리는 반대 방향도 필요하므로
**작아 보이는 것이 함정일 수 있습니다.**

## 검증

| 항목 | 결과 |
|---|---|
| `--cache`가 kind를 찍음 | `kind=kCopy` 등 |
| 옵션 없는 census 불변 | 73,748 · 585 · 15,646 · 7,723 · `agrees=true` |
| 변경 범위 | `src/tools/instruction_census/main.cpp` 한 파일 |

probe 회귀는 돌리지 않았습니다 — 바뀐 파일이 census 하나이고 어떤 probe도 그것을
링크하지 않습니다.

## 아직 확인하지 않음

- **i386이 이 `sti`를 어떻게 실행하는지 재지 않았습니다.** 위의 "추정"은
  추정입니다. 다음 단위가 어느 방향을 고르든 이것을 먼저 재는 편이 낫습니다 —
  이 세션에서 추정이 네 번 반증됐습니다.
- `pumpit2a`의 미해결 분기 1건은 여전히 쫓지 않았습니다.

---

# Work log 20260903-580 — Nothing services a fault raised inside the cache

Design and work order: this is a **diagnosis unit** whose only code change is a
one-column tool correction, and it is the follow-up decision 3 of
[20260903-579](../design/20260903-579-emitted-cache-dump.md) announced.

## First — Task 579's implementation did not follow its design

Task 579's design fixed the dump's columns in a table, and one of them was
**kind**. The implementation left it out.

**That omission wasted an entire investigation.** Seeing `fb` without `kCopy`
beside it leaves "was this refused into an INT3, or copied on purpose" open, and
that was precisely the question. The column is added here.

```text
     cache=0x0 len=5  guest=0x10f16b0  kind=kDirectJump
        emitted: e9 00 00 00 00
  >> cache=0x5 len=1  guest=0x10f1728  kind=kCopy
        emitted: fb
     cache=0x6 len=4  guest=0x10f1729  kind=kCopy
        emitted: 41 83 e7 fc
```

`kind=kCopy`. **The planner marks `sti` as `kCopy`**, so the i386 emitter copies
the same `fb`. The emission is identical on both hosts.

## The diagnosis — where the path breaks

### Confirmed

1. `sti` at guest `0x10f1728` is `kCopy` and is emitted as `fb` in the long-mode
   cache.
2. `STI` raises #GP at CPL 3, delivered by Linux as `SIGSEGV` (0xb) with
   `si_addr=0` — exactly Task 578's observation.
3. `HandlePrivilegedTrapInstruction` **reads bytes directly at `Eip`** and
   requires `IsGuestRangeReadable`, which admits only
   `runtime_base ≤ address < runtime_base + runtime_size` — the **guest arena**.
   The cache is at `0x20000000`, outside it.
4. The place that maps a cache address back to a guest address is
   `HandleAotReentry`, and it does so **only when
   `fault.kind == kBreakpoint`** (`src/engine/aot/aot_runtime_dispatch.cpp`).
5. `sti`'s #GP is not a breakpoint but an **access violation**.

**So no path translates an access-violation fault raised inside the cache back to
a guest address.** `Eip` stays a cache address, and every HLE handler that reads
bytes at `Eip` refuses.

### Why i386 never showed this — inferred

Running the i386 `repiu` on the same ROM (`pumpit2a`) keeps going for **42
seconds** (exit 124 is my timeout, not the game stopping).

```text
[repiu-live] elapsed_ms=42699 ... last_eip=0x010F2786
             single_step=14304 aot=14481/197211
```

Not one `repiu-fault` line appeared.

Why i386 gets past this point is **inferred**: single-stepping is a large share
of its execution and cache entry goes through dispatch, so it very likely does
not execute the entry region's `sti` from the cache. **How that block actually
executes there was not measured.**

x64 differs. Task 578's entry goes **straight into the cache from the program's
first block**, so it meets the gap at the second one.

## This puts a Task 578 structural choice back in question

Entering the cache directly was Task 578's decision 1, and at the time it looked
like the only choice — guest bytes cannot be executed in long mode.

That judgement still holds. What has emerged is that **it also skipped the path
i386 has been coming through**. The next unit picks one of two:

- make an access-violation fault inside the cache translate back to a guest
  address too (fill the gap); or
- have x64 reach the cache through dispatch as i386 does (follow the path).

Which one is not decided here. The first looks smaller, but servicing then
`++Eip`s a *guest* address that must be mapped back to a cache address to
resume — so **looking smaller may be the trap.**

## Verification

| Item | Result |
|---|---|
| `--cache` prints the kind | `kind=kCopy`, etc. |
| Census unchanged without the option | 73,748 · 585 · 15,646 · 7,723 · `agrees=true` |
| Change scope | one file, `src/tools/instruction_census/main.cpp` |

Probe regressions were not run: the only changed file is the census and no probe
links it.

## Not yet verified

- **How i386 actually executes this `sti` was not measured.** The inference
  above is an inference. Whichever direction the next unit takes, measuring this
  first is the better move — four inferences have been refuted this session.
- `pumpit2a`'s one unresolved branch is still unchased.
