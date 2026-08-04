# AOT worker 기반 inline cache 분석

## 확인됨

정적 및 동적 AOT code cache는 반복되는 간접 전송을 매번 `INT3`/VEH로
보내지 않고, 실행 중 학습한 단일 target을 guard하는 inline cache를 사용할 수
있습니다. 현재 지원 범위는 prefix 없는 legacy-32 `FF /2` near indirect call,
`FF /4` near indirect jump, `C3`, `C2 iw`입니다.

```mermaid
sequenceDiagram
    participant G as Guest cache slot
    participant V as Existing VEH dispatcher
    participant W as Host patch worker
    participant C as RX code cache
    G->>V: first miss (popfd; INT3)
    V->>V: resolve guest target and cache target
    V->>W: miss address + guest/cache target
    W->>C: RX -> RW
    W->>C: target, rel32, guard 순서로 기록
    W->>C: RW -> RX + FlushInstructionCache
    W-->>G: patch complete
    G->>C: subsequent guarded direct jump
```

간접 call은 hit 시에도 guest fallthrough 주소를 stack에 넣습니다. return slot은
`pushfd` 이후 `[ESP+4]`가 학습한 guest return과 같은 경우에만 `LEA`로 원래
`RET`의 pop을 재현하고 cache target으로 이동합니다. 따라서 guest 주소와 cache
주소가 섞인 stack에서 raw native `RET`를 실행했던 이전 prototype의 충돌을
반복하지 않습니다. operand 또는 return 값이 바뀌면 기존 dispatcher로
fail-closed합니다.

`pushfd` 때문에 ESP 기반 ModRM/SIB operand의 effective address가 달라질 수
있으므로 emitter가 displacement를 4바이트 보정합니다. 안전하게 다시 쓸 수
없는 prefix, 16-bit operand/address 및 far transfer는 기존 sentinel로 남습니다.

## 확인됨 (Task 413) — patch 1회의 실제 가격은 보호 구간입니다

patch가 쓰는 바이트는 **14개**(선택된 entry의 target immediate 4, jump displacement 4,
guard 6)인데, 그동안 그 14바이트를 위해 **캐시 전체(16 MB = 4,096 페이지)의 보호
속성을 두 번** 바꿨습니다(`RW` → 쓰기 → `RX`).

| 항목 | 값 |
|---|---|
| 캐시 용량 | `kDynamicCacheCapacity` = 16 MB |
| `VirtualProtect` 쌍의 실측 비용 | **4,225 µs / 약 11.5 M cycle** (같은 호스트, 32비트, 2,000회 평균) |
| pumpit3 멈춤 실행의 patch 수 | **12,288회 이상** |
| 그 실행의 breakpoint gap | guest-run의 **62%**, 1건당 2.28 M cycle |

**왜 그렇게 자주 patch되는가 — 다형 return 지점입니다.** pumpit3에서 가장 자주 miss
하는 site `0x030D09D7`은 Watcom 스택 검사 helper(`0x030D09CA`)의 `ret 4`이고, 그
helper의 **정적 호출처가 259곳**입니다. inline cache는 **4-entry**이고 가득 차면
round-robin으로 교체하므로, 이 site는 사실상 **매번 miss**합니다.

**따라서 두 축이 분리됩니다.** (1) miss 1회의 가격 — 보호 구간을 쓰는 페이지로 좁히면
내려갑니다(Task 413). (2) miss 횟수 — 4-entry로 259 호출처를 담을 수 없으므로 그대로
남습니다. 후자는 별도 설계가 필요합니다(해시 기반 return dispatch, site별 entry 수).

**측정 결과(Task 413) — 좁히는 것만으로는 멈춤이 낫지 않습니다.** wide/narrow 3회씩
A/B에서 프레임은 양쪽 다 0~1이었고, 같은 시간의 breakpoint 예외만 약 15% 늘었습니다.
**"patch 12,288회 × 11.5 M cycle"은 실행 전체 예산을 넘는 값이었으므로 가설이
과했습니다.** Task 412의 host 표본 귀속은 그 자리를 여러 경로의 합으로 채웁니다 —
세그먼트 override 재해석 약 15%, `WriteGuestBytes` 약 14%, `FindAotCacheAddress` 약
13%, JAMMA 스냅샷 약 10%, inline-cache patch 요청 약 7%. 변경은 정확성·안전성 이유로
유지합니다.

**Measured (Task 413) — narrowing alone does not cure the stall.** Three wide against three
narrow runs left frames at zero or one in both conditions, with only about 15% more
breakpoint exceptions in the same wall time. The estimate "12,288 patches times 11.5 M
cycles" exceeded the entire guest-run budget, so the hypothesis was overreaching; Task 412's
host-sample attribution fills the slot with a sum of paths instead — segment-override
re-resolution about 15%, `WriteGuestBytes` about 14%, `FindAotCacheAddress` about 13%, the
JAMMA snapshot about 10%, and the inline-cache patch request about 7%. The change is kept
for accuracy and for the smaller writable window.

## Confirmed (Task 413) — the price of one patch is its protection window

A patch writes **fourteen bytes** — the chosen entry's four-byte target immediate, its
four-byte jump displacement, and its six-byte guard — and until Task 413 it flipped the
protection of the **whole 16 MB cache twice** to do so. That pair measures **4,225 µs
(about 11.5 M cycles)** on this host in a 32-bit process, and a stalled pumpit3 run performs
**over 12,288 patches**, against a breakpoint gap of 62% of guest-run at 2.28 M cycles each
(Task 411).

The reason patches are so frequent is a **polymorphic return site**: pumpit3's most-missed
site `0x030D09D7` is the `ret 4` of the Watcom stack-check helper at `0x030D09CA`, which has
**259 static call sites**, while the inline cache holds **four** entries and replaces
round-robin — so it misses essentially every time. That separates two axes: the **price** of
a miss, which the Task 413 page window lowers, and the **count** of misses, which it does
not touch and which needs its own design (hash-based return dispatch, or per-site entry
counts).

## 메모리 보호와 동시성

code cache는 평상시 `PAGE_EXECUTE_READ`입니다. serialized host worker만 guest
thread가 VEH에서 기다리는 동안 cache를 `PAGE_READWRITE`로 바꾸고, patch 후
`PAGE_EXECUTE_READ` 및 instruction-cache flush를 복원합니다. 영구적이거나
일시적인 RWX page를 만들지 않습니다.

현재 안전성은 한 loader process에서 guest 실행 thread가 하나라는 전제에
기반합니다. 향후 여러 guest thread가 같은 cache를 실행한다면 page-wide
protection 전환과 site publication에 별도 stop-the-world 또는 세밀한 동기화가
필요합니다.

## 실행 관찰

동일한 `pumpit1` AOT 실행에서 inline cache 적용 전 3초 관찰은 indirect
dispatcher 약 15,327회, return dispatcher 약 21,894회를 기록했습니다. guarded
indirect와 return을 함께 적용한 3초 관찰은 다음과 같습니다.

| 항목 | 적용 전 3초 | 적용 후 3초 |
|---|---:|---:|
| indirect dispatcher | 약 15,327 | 33 |
| return dispatcher | 약 21,894 | 1,089 |
| patch attempt/success | 0 | 1,122 / 1,122 |
| heartbeat | 약 4,238 | 약 89,502 |

30초 관찰에서도 예외나 legacy fallback 없이 heartbeat 약 1,248,228,
progress 약 145,963을 기록했습니다. indirect/return dispatcher와 patch 수는 각각
33, 1,089, 1,122에서 더 증가하지 않았으므로 반복 dispatch 병목은 초기 학습
비용으로 축소됐습니다.

## 새 실행 frontier

30초 AOT 관찰은 LINEXE service 5가 `_GRGLIDEINIT@0`을 약 72,980회 성공적으로
조회하고 `0x045D0300`을 반환하지만, Glide gate 진입은 0회임을 확인했습니다.
후속 frame/disassembly 비교로 LINEXE output과 wrapper 복원은 정상이며 PIU가
`0x030FED0E` stub을 `E9 rel32`로 수정한 뒤에도 AOT가 변경 전 resolver cache
entry를 선택한다는 사실을 확인했습니다. 현재 frontier는
[AOT self-modifying code 일관성](aot-self-modifying-code.md)입니다.

task 191에서 이 frontier는 page generation 방식으로 해결됐습니다. translated
instruction byte write는 기존 entry를 retire하고, 다음 page 진입에서 live byte로
새 세대를 발행합니다. 5바이트 이상인 stale entry는 새 cache entry로 가는
`E9 rel32` forwarding이 되므로 이미 학습된 inline-cache target도 새 세대로
수렴합니다. 짧은 entry는 `INT3` provenance trap으로 남습니다.

## 미확정

* 동일 slot에서 target이 반복적으로 바뀌는 polymorphic call site의 비용
* 여러 guest thread가 같은 cache를 실행할 때의 publication 정책
* retired inline-cache provenance와 generation cache의 장기 reclamation

# AOT Worker-backed Inline Cache Analysis

## Confirmed

Static and dynamic AOT caches use monomorphic guarded slots for prefix-free
legacy-32 `FF /2`, `FF /4`, `C3`, and `C2 iw`. The first miss resolves through
the existing VEH path. A serialized host worker temporarily changes the cache
from RX to RW, writes the learned guest target and cache-relative edge, activates
the guard last, restores RX, and flushes the instruction cache. RWX pages are not
used.

Calls continue to push guest fallthrough addresses. Returns jump directly only
when the guarded guest return value matches; all changed values fail closed to
the dispatcher. Three-second PIU observation reduced indirect dispatch from
about 15,327 to 33 and return dispatch from about 21,894 to 1,089, with all 1,122
patch attempts succeeding. A 30-second run remained exception-free without
legacy fallback.

The LINEXE output/return ABI was verified as correct. PIU patches its import stub,
but AOT continues to select the stale pre-patch cache entry. The current frontier
was a general self-modifying-code coherency policy.

Task 191 resolved that frontier with page generations. Writes overlapping
translated instruction bytes retire the old entry, and the next page entry
publishes a translation from live bytes. Stale entries of at least five bytes are
forwarded with `E9 rel32`, so already learned inline-cache targets converge on the
new generation; shorter entries remain `INT3` provenance traps. Polymorphic-site
cost, multi-thread publication, and long-term retired-cache reclamation remain
unresolved.
