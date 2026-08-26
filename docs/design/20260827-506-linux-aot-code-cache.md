# Task 506 — Linux AOT 코드 캐시

작업 지시: [20260827-506](../work-orders/20260827-506-linux-aot-code-cache.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260822-503](20260822-503-linux-execution-engine.md) · [20260827-505](20260827-505-linux-render-backend.md)

## 배경 — 이제 화면이 여기에 달려 있습니다

Task 503이 게스트를 Linux에서 실행시켰고, 503d-23이 정지를 없앴고, 505가 Glide 창을
열었습니다. **그런데 화면은 나오지 않습니다.**

505의 조사가 이유를 확정했습니다. 게스트는 시작 시 자산을 디코드하는데(`0x010EE170`–
`0x010EE1DA`, Task 219가 이미 확정한 Huffman류 비트스트림 디코더), Linux는 AOT 코드 캐시가
미이식이라 `legacy` 백엔드로 강제되고, legacy는 **명령마다 단일 스텝**합니다.

| 측정 | 값 |
|---|---|
| Linux legacy 처리량 | **초당 약 127,000 디스패치** |
| 네이티브 대비 | **약 만 배 느림** |
| 1,200초 실행의 버퍼 스왑 | **0회** |

그래서 이 항목의 성격이 바뀌었습니다. **"기본 백엔드가 Linux에서 돌려면 필요"에서 "화면이
기다리는 항목"이 되었습니다.**

## 조사 결과 — 3d-19의 예측이 맞았습니다

3d-19가 배치 함수를 옮기다 **Win32 메모리 호출 23곳**을 보고 되돌리며 "전부 3b가 덮는
호출이라 기계적이지만 양이 있다"고 적었습니다. 두 파일 전체를 세어 확인했습니다.

| 파일 | 줄 수 | Win32 호출 |
|---|---|---|
| `aot_code_cache_win32.cpp` | 2,165 | 47 |
| `aot_page_coherence_win32.cpp` | 1,010 | 15 |

내역과 대응물:

| Win32 | 횟수 | Linux 대응 | 상태 |
|---|---|---|---|
| `VirtualProtect` | 25 | `ProtectMemory` (previous 포함) | **3b가 이미 덮음** |
| `VirtualFree` | 10 | `ReleaseMemory` | **3b가 이미 덮음** |
| `VirtualAlloc` | 2 | `ReserveMemory` / `CommitMemory` | **3b가 이미 덮음** |
| `VirtualQuery` | 2 | `QueryMemory` / `IsRangeReadable` | **3b가 이미 덮음** |
| `GetCurrentProcess` | 8 | — | 인자 전용, 소멸 |
| `GetLastError` | 6 | `errno` | 진단용 |
| `FlushInstructionCache` | 8 | **없음** | 결정 1 |
| `GetSystemInfo` | 1 | `sysconf(_SC_PAGESIZE)` | 사소 |

**새로 만들어야 하는 것은 하나뿐입니다.** 나머지는 3b가 이미 답을 갖고 있습니다.

`VirtualQuery` 두 곳이 묻는 것도 같습니다 — "이 주소가 commit되어 있고 읽을 수 있는가".
3b의 `QueryMemory`/`IsRangeReadable`이 그대로 답합니다. 헤더가 "VirtualQuery에는 값싼 Linux
대응물이 없다"고 적어 둔 것은 **일반적인 VirtualQuery**에 대한 말이고, 이 두 호출부가 묻는
좁은 질문은 이미 덮여 있습니다.

## 결정 1: 명령 캐시 플러시는 계층에 올리되, x86에서 무엇인지 적습니다

`FlushInstructionCache`가 8곳입니다. **x86의 명령 캐시는 데이터 캐시와 일관되므로 이 호출은
아키텍처상 불필요합니다** — Windows에서도 사실상 형식입니다.

그래도 호출부를 지우지 않고 `platform/instruction_cache.h`(가칭)로 올립니다. 이유는 둘입니다.

1. **호출부가 옳습니다.** 코드를 쓰고 실행하기 전에 플러시하는 것은 이식 가능한 코드의
   규약이고, 이 엔진이 언젠가 x86 외로 간다면 그때 필요한 것이 정확히 이 지점들입니다.
2. **지우면 왜 없는지가 사라집니다.** 이 저장소가 505에서 배운 것이 그것입니다 — 호출을
   없애는 것과 그 자리에 "여기서는 필요 없다"를 적어 두는 것은 다릅니다.

Linux 구현은 `__builtin___clear_cache`이고, x86에서 컴파일러가 아무것도 내지 않습니다.
**"아무것도 하지 않는 함수"를 만드는 것이므로, 그 사실을 헤더에 적습니다** — frontier 8절이
모으는 함정이 바로 그것이고, 여기서는 **의도된** 무동작임을 구분해 두어야 합니다.

## 결정 2: 505의 방법을 그대로 씁니다

505가 효과적이었던 이유는 순서였습니다.

1. **먼저 세었습니다** — "44개 울타리"가 아니라 "진짜 Win32 API 0개"가 실제 규모였습니다.
2. **저장소를 고치지 않고 사본으로 측정했습니다** — 오류 2건.
3. **Windows 무영향을 증명했습니다** — 주장하지 않고 대조했습니다.

506도 같은 순서를 따릅니다. 다만 505와 다른 점이 하나 있습니다 — **여기는 진짜로 이식이
필요합니다.** 505는 이미 이식된 코드의 울타리를 걷는 일이었고, 506은 Win32 호출 62곳을 계층
호출로 바꾸는 일입니다. 규모를 낮춰 부르지 마십시오.

## 결정 3: 완료 조건은 컴파일도 실행도 아니고 **화면**입니다

`dynamic` 백엔드가 Linux에서 링크되고 돌기 시작하는 것으로는 부족합니다. 505가 남긴 교훈이
그것입니다 — **성공 신호 하나로 성공을 판정하지 말 것.**

이 작업이 성공했는지는 **버퍼 스왑이 0을 벗어나는가**로 판정합니다. 그것이 505가 열어 둔 창에
처음으로 무언가가 그려지는 순간이고, 이 이식이 존재하는 이유입니다.

## 범위 밖

무엇이 그려지는지의 정확성, 성능 튜닝, `native_linear_span`을 Linux에서 되살리는 것(503d-23이
막아 두었고 하드웨어 디버그 레지스터가 없는 한 유효합니다).

---

# Task 506 — The Linux AOT code cache

Work order: [20260827-506](../work-orders/20260827-506-linux-aot-code-cache.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessors: [20260822-503](20260822-503-linux-execution-engine.md) · [20260827-505](20260827-505-linux-render-backend.md)

## Background — the screen now depends on this

Task 503 got the guest executing on Linux, 503d-23 removed the stall, and 505 opened a Glide window.
**And still nothing is drawn.**

505's investigation settled why. The guest decodes assets at start-up (`0x010EE170`–`0x010EE1DA`, the
Huffman-style bitstream decoder Task 219 already identified), and Linux is confined to the `legacy`
backend because the AOT code cache is unported — and legacy **single-steps every instruction**.

| Measurement | Value |
|---|---|
| Linux legacy throughput | **about 127,000 dispatches a second** |
| Against native | **some ten thousand times slower** |
| Buffer swaps in a 1,200-second run | **zero** |

That changes what this item is. **From "the default backend needs it on Linux" to "the item the
screen is waiting on".**

## What the investigation found — 3d-19's estimate was right

3d-19 started moving the placement function, saw **23 Win32 memory calls**, reverted, and wrote that
"all of them are calls 3b covers, so the work is mechanical, but there is a lot of it". Counting both
files whole confirms it.

| File | Lines | Win32 calls |
|---|---|---|
| `aot_code_cache_win32.cpp` | 2,165 | 47 |
| `aot_page_coherence_win32.cpp` | 1,010 | 15 |

The breakdown, and what each maps to:

| Win32 | Count | Linux counterpart | State |
|---|---|---|---|
| `VirtualProtect` | 25 | `ProtectMemory` (with `previous`) | **already covered by 3b** |
| `VirtualFree` | 10 | `ReleaseMemory` | **already covered by 3b** |
| `VirtualAlloc` | 2 | `ReserveMemory` / `CommitMemory` | **already covered by 3b** |
| `VirtualQuery` | 2 | `QueryMemory` / `IsRangeReadable` | **already covered by 3b** |
| `GetCurrentProcess` | 8 | — | an argument only; disappears |
| `GetLastError` | 6 | `errno` | diagnostic |
| `FlushInstructionCache` | 8 | **none** | decision 1 |
| `GetSystemInfo` | 1 | `sysconf(_SC_PAGESIZE)` | trivial |

**Exactly one thing has to be built.** 3b already answers the rest.

The two `VirtualQuery` sites ask the same narrow question — "is this address committed and readable"
— which `QueryMemory` / `IsRangeReadable` answer directly. The header's note that VirtualQuery "has
no cheap Linux counterpart" is about **VirtualQuery in general**; the narrow question these two ask is
already covered.

## Decision 1: lift the instruction-cache flush into the layer, and record what it is on x86

There are eight `FlushInstructionCache` calls. **x86's instruction cache is coherent with its data
cache, so the call is architecturally unnecessary** — it is close to ceremonial on Windows too.

The call sites stay, lifted onto `platform/instruction_cache.h` (working name), for two reasons.

1. **The call sites are right.** Flushing after writing code and before running it is the contract
   portable code keeps, and if this engine ever leaves x86 these are exactly the points that will
   need it.
2. **Deleting them deletes the reason.** That is what this repository learned in 505 — removing a
   call and leaving "not needed here" in its place are not the same thing.

The Linux implementation is `__builtin___clear_cache`, which emits nothing on x86. **That makes it a
function that does nothing, so the header has to say so** — section 8 of the frontier collects
exactly that trap, and this one has to be marked as **deliberate** inaction.

## Decision 2: use 505's method unchanged

What made 505 work was its order.

1. **Count first** — the real size was not "44 fences" but "zero real Win32 API calls".
2. **Measure on a copy, without editing the repository** — two errors.
3. **Prove Windows is unaffected** — compared, not asserted.

506 follows the same order, with one difference from 505: **this one really is a port.** 505 took
fences off already-portable code; 506 moves 62 Win32 calls onto layer calls. Do not talk the size
down.

## Decision 3: the completion criterion is not the build or the run — it is **the screen**

The `dynamic` backend linking and starting to run on Linux is not enough. That is 505's lesson:
**do not judge success from a single success signal.**

Whether this work succeeded is decided by **buffer swaps leaving zero**. That is the first moment
anything is drawn into the window 505 opened, and it is why this port exists.

## Out of scope

Whether what is drawn is correct, performance tuning, and reviving `native_linear_span` on Linux
(503d-23 blocked it, and that holds as long as there are no hardware debug registers).
