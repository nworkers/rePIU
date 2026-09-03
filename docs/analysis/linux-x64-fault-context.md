# Linux x86-64 fault context — 게스트 상태 중 무엇이 실제로 관측되는가

Task 584에서 정리. 이 문서는 **Linux x86-64 호스트에서 폴트가 났을 때
`GuestCpuContext`의 어느 필드가 진짜 관측이고 어느 필드가 합성된 값인지**를
누적합니다.

세그먼트 축 작업이 반복해서 만나는 제약이므로 주제 문서로 둡니다.

## 확인됨 — `DS`·`ES`·`SS`는 관측이 아니다

`src/platform/linux/guest_cpu_context.cpp`의 x86-64 로드 경로:

```c
const std::uint64_t selectors = static_cast<std::uint64_t>(
    machine.gregs[REG_CSGSFS]);
registers->SegCs = static_cast<std::uint32_t>(selectors & 0xFFFFU);
registers->SegGs = static_cast<std::uint32_t>((selectors >> 16U) & 0xFFFFU);
registers->SegFs = static_cast<std::uint32_t>((selectors >> 32U) & 0xFFFFU);
registers->SegDs = 0U;
registers->SegEs = 0U;
registers->SegSs = 0U;
```

Linux의 x86-64 `mcontext_t`는 `REG_CSGSFS` 한 칸에 CS·GS·FS만 담고 **DS·ES·SS는
저장하지 않습니다.** 그러므로 세 필드의 0은 "게스트의 DS가 0이었다"가 아니라
**"이 호스트는 그 값을 주지 않는다"**입니다.

i386 경로는 다릅니다 — `REG_DS`·`REG_ES`·`REG_SS`가 실제로 있고 그대로 읽습니다.

### 왜 이것이 중요한가

x64에서 지금 진행 중인 벽이 세그먼트 모양입니다(3.30절). 그 상황에서
`ds=0x0000`을 찍어 두면 **측정된 적 없는 값에서 결론이 나옵니다.** Task 584의
`[repiu-regs]`가 그 세 값을 아예 찍지 않는 이유가 이것입니다.

### 하드웨어 쪽 사정도 같은 방향이다

64비트 모드에서 DS·ES·SS는 주소 계산에 쓰이지 않고 base가 0으로 강제됩니다.
그러므로 **게스트가 의도한 DS는 x64에서 CPU 상태가 아니라 엔진 상태입니다.**
그것을 알아야 하는 코드는 selector table 쪽을 봐야 하고, fault context를 보면
안 됩니다.

## 확인됨 — `Eip`는 RIP, `Esp`는 R15D

Task 577·578에서 정해진 것으로, 여기에도 함께 적어 둡니다.

| 필드 | x64에서의 출처 | 근거 |
|---|---|---|
| `Eip` | `REG_RIP` | 엔진이 이것을 cache 주소로 다루고, cache는 4 GiB 아래에 놓이므로 절단이 무손실 |
| `Esp` | `REG_R15` | 게스트 ESP는 R15D에 있고 host RSP는 SysV 스택 그대로 |
| GPR 8개 | 동번호 host 레지스터 | 진입이 그렇게 심음 |
| `SegCs`·`SegFs`·`SegGs` | `REG_CSGSFS` | 실제 관측 |
| `SegDs`·`SegEs`·`SegSs` | **없음(0)** | 위 절 |

## 미확정

* 게스트가 의도한 DS를 폴트 시점에 알아내는 방법. selector table과 guarded
  segment load 계측(Tasks 566~571)이 후보이지만, **폴트 시점의 값을 되짚는
  경로가 있는지는 확인하지 않았습니다.**

## 참고

* [Linux port frontier 3.30·3.31](linux-port-frontier.md)
* 작업 기록 [20260903-584](../work-logs/20260903-584-declined-fault-registers.md)

---

# Linux x86-64 fault context — which guest state is actually observed

Collected in Task 584. This document accumulates **which `GuestCpuContext`
fields are real observations on a Linux x86-64 host at fault time, and which are
synthesized.**

It is kept as a topic document because the segment axis meets this constraint
repeatedly.

## Confirmed — `DS`, `ES` and `SS` are not observations

The x86-64 load path in `src/platform/linux/guest_cpu_context.cpp`:

```c
const std::uint64_t selectors = static_cast<std::uint64_t>(
    machine.gregs[REG_CSGSFS]);
registers->SegCs = static_cast<std::uint32_t>(selectors & 0xFFFFU);
registers->SegGs = static_cast<std::uint32_t>((selectors >> 16U) & 0xFFFFU);
registers->SegFs = static_cast<std::uint32_t>((selectors >> 32U) & 0xFFFFU);
registers->SegDs = 0U;
registers->SegEs = 0U;
registers->SegSs = 0U;
```

Linux's x86-64 `mcontext_t` packs only CS, GS and FS into the single
`REG_CSGSFS` slot and **saves no DS, ES or SS.** So a zero in those three does
not say "the guest's DS was zero"; it says **"this host does not provide that
value."**

The i386 path differs — `REG_DS`, `REG_ES` and `REG_SS` genuinely exist there
and are read straight through.

### Why this matters

The wall x64 is currently at is segment-shaped (section 3.30). Printing
`ds=0x0000` in that situation is how **a conclusion gets drawn from a value that
was never measured.** That is why Task 584's `[repiu-regs]` does not print those
three at all.

### The hardware side points the same way

In 64-bit mode DS, ES and SS take no part in address computation and their bases
are forced to zero. So **the guest's intended DS is engine state on x64, not CPU
state.** Code that needs it must consult the selector table, not the fault
context.

## Confirmed — `Eip` is RIP and `Esp` is R15D

Settled in Tasks 577 and 578; recorded here alongside.

| Field | Source on x64 | Reason |
|---|---|---|
| `Eip` | `REG_RIP` | the engine treats it as a cache address, and the cache is placed below 4 GiB, so the truncation is lossless |
| `Esp` | `REG_R15` | guest ESP lives in R15D while host RSP stays the SysV stack |
| The eight GPRs | the same-numbered host registers | that is how entry seeds them |
| `SegCs`, `SegFs`, `SegGs` | `REG_CSGSFS` | genuine observations |
| `SegDs`, `SegEs`, `SegSs` | **absent (zero)** | the section above |

## Unresolved

* How to recover the guest's intended DS at fault time. The selector table and
  the guarded segment-load instrumentation (Tasks 566–571) are candidates, but
  **whether a path exists back to the value at that moment has not been
  checked.**

## References

* [Linux port frontier, sections 3.30 and 3.31](linux-port-frontier.md)
* Work log [20260903-584](../work-logs/20260903-584-declined-fault-registers.md)
