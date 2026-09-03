# 작업 기록 20260903-584 — `access`와 `ESI`가 같다

설계: [20260903-584](../design/20260903-584-declined-fault-registers.md) ·
작업 지시: [20260903-584](../work-orders/20260903-584-declined-fault-registers.md)

## 답 — base가 붙지 않았습니다

```text
[repiu-exit] site=no-host-frame-to-unwind eip=0x200002AA code=0x0000000B
             guest_stack=1 call_state=0 n=1
[repiu-regs] access=0x00200202 eax=0x00000000 ecx=0x00000000 edx=0x01380000
             ebx=0x00000024 esp=0x0158CC74 ebp=0x00000000 esi=0x00200202
             edi=0x0138CC96 eflags=0x00210246 cs=0x0033 fs=0x0000 gs=0x0000
```

**`access == esi == 0x00200202`.**

방출된 `67 8b 06`(`mov eax, dword ptr [esi]`)이 만든 선형 주소가 정확히 `ESI`
입니다. 변위도 index도 없고 long mode에서 DS base가 0이므로 예상되는 결과이며,
Task 583이 "어느 base도 더해지지 않은 값과 일관된다"고 추정으로 적은 것이
**확정됐습니다.**

## 확정된 것과 확정되지 않은 것

설계가 미리 갈라 둔 두 질문 그대로입니다.

| 질문 | 상태 |
|---|---|
| 방출된 명령이 base 없이 주소를 만들었는가 | **확인됨** |
| `ESI`가 애초에 `0x200202`여야 했는가 | **미확정 — 재지 않았습니다** |

두 번째를 여기서 결론짓지 않는 것이 이 단위의 규율입니다. `0x200202`가 이미
base가 적용된 완전한 선형 주소인데 그 영역이 매핑되지 않은 것일 수도 있고,
base를 기다리는 세그먼트 상대 offset일 수도 있습니다. **둘은 다른 수정으로
이어집니다.**

Task 580이 옳은 관찰에서 틀린 결론으로 넘어간 것이 이 세션에서 반증된 다섯 추정의
첫 번째였습니다. 같은 모양을 반복하지 않습니다.

## 부수 관측 — 레지스터 파일이 일관돼 보입니다

`edx=0x01380000`, `edi=0x0138CC96`, `esp=0x0158CC74`가 게스트 arena 대역의
그럴듯한 주소입니다. **x64 실행이 여기까지 헤맨 것이 아니라 실제로 진행했다**는
정황입니다 — Task 583이 잰 673바이트 전진과 맞습니다.

`ebx=0x00000024`가 눈에 띕니다. census가 찍은 selector 목록에 `selector=0x24`가
있습니다. **정황일 뿐이고 확인된 것이 아닙니다** — 같은 숫자가 우연히 다른 것을
뜻할 수 있습니다.

## 새로 발견한 제약 — x64는 `DS`·`ES`·`SS`를 주지 않습니다

설계 결정 3을 쓰기 위해 로드 경로를 확인하다 나왔습니다.

```c
registers->SegDs = 0U;
registers->SegEs = 0U;
registers->SegSs = 0U;
```

Linux의 x86-64 `mcontext_t`는 `REG_CSGSFS`에 CS·GS·FS만 담고 나머지 셋은
저장하지 않습니다. 그러므로 그 0은 **게스트 상태가 아니라 "이 호스트가 주지
않는다"**입니다.

지금 벽이 세그먼트 모양이라 이것은 오독하기 딱 좋은 값입니다. `ds=0x0000`을
보고 "DS가 0이라 그렇구나"라고 읽으면 **측정된 적 없는 값에서 결론이 나옵니다.**
그래서 `[repiu-regs]`는 그 셋을 **찍지 않습니다.** 없는 것을 0으로 찍는 것보다
비워 두는 편이 정직합니다.

주제 문서로 남겼습니다 —
[Linux x86-64 fault context](../analysis/linux-x64-fault-context.md). 세그먼트
축 작업이 반드시 다시 만날 제약입니다.

## 구현

`fault_exit_trace.cpp`에 둘째 줄 하나입니다. 같은 게이트, 같은 상한, 같은 호출.
`access=`는 `fault.access.valid`가 거짓이면 `none`으로 찍습니다 — 0을 찍어
"주소 0에 접근했다"로 읽히게 두지 않습니다.

`ds`·`es`·`ss`를 빼는 이유는 소스 주석에 남겼습니다. 그 줄을 다시 읽는 사람이
"왜 빠졌지"를 묻게 두지 않기 위해서입니다.

## 검증

| 항목 | 결과 |
|---|---|
| 추적 없는 i386 (`pumpit2a`, 45s) | `[repiu-exit]`·`[repiu-regs]` 0줄, `last_eip=0x010F2786`, `single_step=11508` |
| x64 레지스터 줄 | 나옴 — `access == esi` |
| Linux i386 `repiu_core_probe` | 19/19, failures 0 |
| Linux x64 `repiu_core_probe` | 20/20, failures 0 |
| Win32 build | exit 0, error 0 |
| Win32 `repiu_core_probe` | 19/19, failures 0 |
| Win32 `repiu_aot_probe` (pumpit1) | `_all=true` 41개, `_all=false` 0개 |

## 아직 확인하지 않음

- **i386에서 같은 guest EIP의 `ESI`를 읽지 않았습니다.** 그것이 남은 질문을
  가릅니다.
- **`0x200202`가 무엇인지 모릅니다.** 게스트 arena 어디에 해당하는지, 매핑된
  영역인지 확인하지 않았습니다.
- `ebx=0x24`가 selector 0x24와 관계있는지 확인하지 않았습니다.
- `pumpit2a`의 미해결 분기 1건은 여전히 쫓지 않았습니다.

---

# Work log 20260903-584 — `access` and `ESI` are the same

Design: [20260903-584](../design/20260903-584-declined-fault-registers.md) ·
Work order: [20260903-584](../work-orders/20260903-584-declined-fault-registers.md)

## The answer — no base was applied

```text
[repiu-exit] site=no-host-frame-to-unwind eip=0x200002AA code=0x0000000B
             guest_stack=1 call_state=0 n=1
[repiu-regs] access=0x00200202 eax=0x00000000 ecx=0x00000000 edx=0x01380000
             ebx=0x00000024 esp=0x0158CC74 ebp=0x00000000 esi=0x00200202
             edi=0x0138CC96 eflags=0x00210246 cs=0x0033 fs=0x0000 gs=0x0000
```

**`access == esi == 0x00200202`.**

The linear address formed by the emitted `67 8b 06`
(`mov eax, dword ptr [esi]`) is exactly `ESI`. With no displacement, no index,
and DS's base zero in long mode, that is the expected result — and it
**confirms** what Task 583 recorded as an inference: no base was added.

## What is settled and what is not

Exactly the two questions the design separated in advance.

| Question | Status |
|---|---|
| Did the emitted instruction form its address with no base? | **Confirmed** |
| Should `ESI` have held `0x200202` at all? | **Unresolved — not measured** |

Not concluding the second here is this unit's discipline. `0x200202` might be a
complete linear address into a region that is simply unmapped, or a
segment-relative offset still waiting for a base. **Those lead to different
repairs.**

Task 580 stepping from a correct observation to a wrong conclusion was the first
of five inferences refuted this session. That shape is not repeated.

## An incidental observation — the register file looks coherent

`edx=0x01380000`, `edi=0x0138CC96` and `esp=0x0158CC74` are plausible addresses
in the guest arena band. That is circumstantial evidence that **x64 execution
genuinely progressed here rather than wandering** — consistent with the 673
bytes Task 583 measured.

`ebx=0x00000024` stands out: the census's selector list contains
`selector=0x24`. **That is circumstance, not a confirmed fact** — the same
number can coincidentally mean something else.

## A newly discovered constraint — x64 provides no `DS`, `ES` or `SS`

Found while checking the load path in order to write design decision 3.

```c
registers->SegDs = 0U;
registers->SegEs = 0U;
registers->SegSs = 0U;
```

Linux's x86-64 `mcontext_t` packs only CS, GS and FS into `REG_CSGSFS` and saves
none of the other three. So those zeros are **not guest state; they are "this
host does not provide it."**

With the current wall being segment-shaped, that is exactly the value that
invites a misreading: seeing `ds=0x0000` and concluding "DS is zero, that
explains it" **draws a conclusion from a value never measured.** So
`[repiu-regs]` **does not print** those three. Leaving an absent value absent is
more honest than printing it as zero.

Recorded as a topic document —
[Linux x86-64 fault context](../analysis/linux-x64-fault-context.md). Any work
on the segment axis will meet this constraint again.

## Implementation

One second line in `fault_exit_trace.cpp`: same gate, same limit, same call.
`access=` prints `none` when `fault.access.valid` is false, rather than a zero
that would read as "it accessed address zero".

The reason `ds`, `es` and `ss` are omitted is recorded in a source comment, so
the next reader of that line is not left asking.

## Verification

| Item | Result |
|---|---|
| i386 with the trace off (`pumpit2a`, 45s) | 0 `[repiu-exit]` and `[repiu-regs]` lines, `last_eip=0x010F2786`, `single_step=11508` |
| x64 register line | present — `access == esi` |
| Linux i386 `repiu_core_probe` | 19/19, 0 failures |
| Linux x64 `repiu_core_probe` | 20/20, 0 failures |
| Win32 build | exit 0, 0 errors |
| Win32 `repiu_core_probe` | 19/19, 0 failures |
| Win32 `repiu_aot_probe` (pumpit1) | 41 `_all=true`, 0 `_all=false` |

## Not yet verified

- **`ESI` at the same guest EIP on i386 was not read.** That is what separates
  the remaining question.
- **What `0x200202` is remains unknown** — where it falls in the guest arena, or
  whether that region is mapped, was not checked.
- Whether `ebx=0x24` relates to selector `0x24` was not checked.
- `pumpit2a`'s one unresolved branch is still unchased.
