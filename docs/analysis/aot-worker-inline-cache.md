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
