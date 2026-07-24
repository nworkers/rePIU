# AOT-DBT CALL continuation 제한 trap 관측 설계 / AOT-DBT bounded CALL-continuation trap probe design

## 한국어

### 1. 배경과 목표

Task 284는 calls-only 크래시 전 30개 dispatcher-visible CALL tuple과 control의 첫
30개가 모두 같고, 공통으로 상관된 26개 RET tuple도 모두 같음을 확인했습니다.
dispatcher-visible 상관 RET가 없는 CALL trace sequence는 27, 30, 33, 56입니다.
따라서 공용 C++ resolver 안의 상태 비교는 소진됐고, 남은 경계는 다음입니다.

1. host thunk가 복귀한 직후 실행되는 synthetic success continuation `C3`
2. C++ return resolver를 건너뛰는 inline-cache-hit RET
3. VEH를 우회할 때만 빠지는 부수효과

Task 223의 전체 trap-backend 장시간 단일스텝은 정확하지만 느리고, code-cache
sentinel은 첫 miss 뒤 inline cache가 직접 edge를 학습하면 재발화하지 않는 구조적
한계가 확인됐습니다. 이번 작업은 선택한 host CALL에만 두 instruction-step과 반환
주소 실행 breakpoint를 사용해 이 경계를 직접 관측합니다.

### 2. 비파괴 관측 방식

원본 guest byte와 code-cache byte는 패치하지 않습니다. CALL miss resolver가 성공하고
Task 284 event sequence가 설정 대상일 때, thunk가 복원할 saved EFLAGS에 TF만 추가합니다.

```mermaid
sequenceDiagram
    participant R as host CALL resolver
    participant T as host thunk
    participant C as synthetic C3
    participant G as callee cache code
    participant H as inline/dispatch RET
    participant P as probe handler

    R->>T: success + saved EFLAGS.TF=1
    T->>C: popfd; ret
    C-->>P: #DB before C3 (pre)
    P->>C: keep TF for one instruction
    C->>G: pop cache target
    G-->>P: #DB after C3 (post)
    P->>P: clear TF, arm DR0/DR1 at cache/guest return
    G->>H: callee executes
    H->>P: #DB at caller continuation (return)
    P->>P: snapshot and restore debug state
```

TF를 복원한 `popfd` 다음 thunk `ret`가 실행된 뒤 첫 `#DB`가 synthetic `C3` 주소에서
발생합니다. 이를 `pre-C3`로 기록하고 TF를 한 instruction 더 유지합니다. `C3` 실행 뒤
두 번째 `#DB`는 resolved callee cache target에서 발생하며 `post-C3`로 기록합니다.
여기서 TF를 지우고:

- DR0: guest return address의 현재 active cache address
- DR1: guest return address 자체

에 local execution breakpoint를 겁니다. return inline-cache hit가 cache continuation으로
직접 이동하면 DR0, legacy/guest 원본 경로로 이동하면 DR1이 잡습니다. 어느 쪽이든
breakpoint는 caller continuation instruction 실행 **전**, 물리적 RET stack pop은 완료된
상태를 제공합니다. hit 뒤 원래 DR0~DR3/DR6/DR7을 복원합니다.

### 3. 상태 모델

Win32 전용 `aot_dbt_call_step_probe.{h,cpp}`가 다음 phase를 소유합니다.

- `idle`
- `await-pre-c3`
- `await-post-c3`
- `await-return-target`
- `complete`
- `conflict`

환경 변수 `REPIU_AOT_DBT_CALL_STEP`은 쉼표로 구분한 Task 284 trace sequence를 최대
8개 받습니다. 예: `27,30,33,56`. `REPIU_AOT_DBT_CALL_TRACE=1`도 켜져 있어야 하며,
host origin의 CALL에만 적용합니다.

각 event는 고정 32칸 ring에 다음을 저장합니다.

```text
event sequence / phase / call sequence
guest source / target / return address
actual EIP / ESP / EFLAGS
EAX EBX ECX EDX ESI EDI EBP
[ESP+0..12] readable dwords + valid mask
expected EIP / ESP
EIP match / ESP match
DR6
```

`pre-C3` 기대 상태:

- EIP = `cache_base + success_cache_offset`
- ESP = `call_entry_esp - 8`
- `[ESP]` = resolved callee cache target
- `[ESP+4]` = guest return address

`post-C3` 기대 상태:

- EIP = resolved callee cache target
- ESP = `call_entry_esp - 4`
- `[ESP]` = guest return address
- GPR과 `C3`가 바꾸지 않는 EFLAGS bit는 pre와 동일

`return-target` 기대 상태:

- EIP = guest return의 active cache address 또는 guest return address
- ESP = `call_entry_esp`

모든 기대값은 진단 판정일 뿐 실행 상태를 수정하는 근거로 사용하지 않습니다.

### 4. debug-register 공존과 fail-closed

반환 watch를 설치하기 전에 기존 DR7 enable bit, native fast path/region/linear-span
활성 여부를 확인합니다. 이미 debug slot이 사용 중이면 `conflict` event를 남기고
DR을 바꾸지 않습니다. 반환 watch가 활성인 동안 새 native fast path 진입만 거부해
DR0/DR1 소유권을 보존합니다. 이는 probe 활성 구간의 진단 정책이며 기본 실행에는
영향이 없습니다.

일반 HLE/INT3/타이머 예외가 callee 안에서 발생해도 watch는 유지합니다. 해당 핸들러가
debug state를 바꾸지 않는 한 caller continuation에서 잡힙니다. 예상 밖 `#DB`,
return cache address 미발견, 중첩 설정 대상 CALL은 각각 counter/event로 남기고 기존
실행은 계속합니다. fatal/caught exception 시 active phase와 최근 event는 final attempt에
그대로 회수합니다.

### 5. 관측 교란 한계

이 probe는 선택한 CALL에 `#DB` 두 번을 추가하므로 완전히 무관찰적인 계측은 아닙니다.
특히 크래시 원인이 "VEH를 한 번도 거치지 않는 것"이라면 probe가 크래시를 늦추거나
없앨 수 있습니다. 그러므로 결과를 다음처럼 해석합니다.

- probe를 켜도 동일 크래시 + 상태 불일치: 해당 물리적 전이가 직접 근인 후보
- probe를 켜도 동일 크래시 + 상태 일치: 관측 CALL 밖의 누적 부수효과 후보
- 특정 sequence probe에서만 크래시 소멸: 그 위치의 VEH 부수효과가 인과 후보이며
  수정으로 간주하지 않음

한 실행에서 여러 sequence를 잡는 기능은 구조 검증용입니다. 인과 판정은 sequence
하나씩 독립 실행해 probe 교란 범위를 분리합니다.

### 6. 검증

1. synthetic probe:
   - 미설정/trace 비활성 시 미동작
   - 대상 sequence 선택과 host-only 게이트
   - pre/post 예상 EIP·ESP·stack 판정
   - DR0/DR1 설치, hit 판정과 원 debug state 복원
   - 기존 DR 사용 시 conflict fail-closed
   - ring wrap과 snapshot 순서
2. Win32 x86 Debug 전체 빌드와 기존 AOT probe 통과
3. calls-only, 격리 EEPROM으로 sequence 27/30/33/56을 각각 실행
4. probe-off Task 284 시그니처와 종료 여부·시점·EEPROM hash 비교
5. 각 sequence의 pre/post/return 또는 pending-at-crash를 기록하고 다음 구현 판단

### 7. 구현 결과와 근인 발견

Win32 x86 Debug 빌드와 모든 probe가 통과했습니다. calls-only 독립 실행 결과:

| sequence | event | 판정 | 종료 |
|---:|---|---|---|
| 27 | pre / post / return | EIP·ESP 모두 일치 | 기존 Glide AV 재현 |
| 30 | pre / post / return | EIP·ESP 모두 일치 | 기존 Glide AV 재현 |
| 33 | pre / post / return | EIP·ESP 모두 일치 | 기존 Glide AV 재현 |
| 56 | pre만, `await-post-c3` | pre EIP가 `0xEB53DDDD`, site source가 `0xDDDDDDDD`로 오염 | 기존 Glide AV 재현 |

sequence 56의 stack은 `[ESP]=0x0D84213A` resolved callee cache target,
`[ESP+4]=0x030D913E` guest return으로 정상이었습니다. 그러나 thunk가 먼저 pop할
success continuation과 probe가 같은 `site->success_cache_offset`에서 계산한 expected
pre EIP가 `0xEB53DDDD`였습니다. 이는 기존 크래시의 EAX/EDX garbage signature와
같은 계열입니다.

정적 대조로 근인을 확정했습니다. indirect adapter는
`dbt_indirect_dispatch_sites` 원소 포인터를 얻은 뒤 `HandleAotIndirectTransfer`를
호출합니다. sequence 56의 guest target `0x03086094` 번역은 동적 append를 일으키고,
append가 같은 placement 벡터에 `push_back`하여 원소 포인터를 무효화합니다. adapter는
그 뒤 무효 포인터의 `success_cache_offset`을 읽어 thunk continuation을 만들었습니다.
Task 285 probe도 같은 무효 포인터를 읽었기 때문에 오염을 독립적으로 드러냈습니다.
RET host adapter에도 같은 포인터 수명 패턴이 있습니다.

따라서 Task 285의 의사 결정은 확정됐습니다. 물리적 `C3`/inline RET 의미를 바꾸지
않고, Task 286에서 resolver 호출 전 dispatch site를 값으로 snapshot해 두 adapter의
use-after-reallocation을 제거합니다.

## English

### 1. Background and goal

Task 284 found all 30 dispatcher-visible CALL tuples before the calls-only crash identical
to control and all 26 common correlated RET tuples identical as well. CALL trace sequences
27, 30, 33, and 56 have no dispatcher-visible correlated RET. The remaining boundary is
the synthetic success `C3`, resolver-bypassing inline-cache-hit returns, or a side effect
lost when VEH is bypassed.

Task 223's full trap backend is accurate but slow, while its code-cache sentinel was proven
unable to re-fire after an inline cache learns a direct edge. Task 285 therefore adds only
two instruction steps and a return-address execution breakpoint to selected host CALLs.

### 2. Non-destructive observation

No guest or code-cache byte is patched. After a selected host CALL resolves, the adapter
sets TF in the EFLAGS that the thunk will restore. The first `#DB`, after the thunk's `ret`,
captures state before the synthetic `C3`. TF remains set for that one instruction; the
second `#DB` captures state after `C3` at the resolved callee cache target and clears TF.

The post handler installs local execution breakpoints at the active cache address of the
guest return (DR0) and the guest return itself (DR1). An inline-cache-hit return reaches
DR0; a legacy/original path reaches DR1. Either trap occurs before the caller continuation
instruction but after the physical return stack pop. Original DR0-DR3/DR6/DR7 values are
restored on hit.

### 3. State, safety, and interpretation

A Win32-only `aot_dbt_call_step_probe` owns idle, pre, post, return-watch, complete, and
conflict phases. `REPIU_AOT_DBT_CALL_STEP` accepts up to eight comma-separated Task 284
event sequences and requires `REPIU_AOT_DBT_CALL_TRACE=1`. Only host-origin CALLs qualify.
A fixed 32-entry ring stores phase, CALL identity, full integer state, four readable stack
dwords, expected EIP/ESP and match flags, and DR6.

Before installing DR0/DR1, the probe rejects existing debug-register ownership or an active
native fast path. New native-fast-path entry is suppressed only while the return watch owns
the registers. Intervening ordinary HLE, timer, or breakpoint exceptions do not cancel the
watch. Unexpected debug events, missing return mappings, nested selected calls, and
conflicts are accounted without changing normal execution.

The two added `#DB` events are an explicit observational perturbation. If a selected probe
alone removes the crash, that is evidence for a missing VEH side effect at that point, not
a fix. Causal runs therefore target one sequence at a time; a multi-sequence run is only a
structural check.

### 4. Verification

Synthetic probes cover disabled and host-only selection, pre/post state checks, DR0/DR1
installation and restoration, conflict handling, and ring wrap. The Win32 x86 Debug build
and all existing probes must pass. Isolated-EEPROM calls-only runs then target sequences
27, 30, 33, and 56 independently and compare termination, timing, signature, and EEPROM
hash against the Task 284 probe-off baseline.

### 5. Implementation outcome and root cause

The full Win32 Debug build and every probe passed. Independent calls-only runs for sequences
27, 30, and 33 captured pre-C3, post-C3, and return-target events with every EIP/ESP check
matching, then reproduced the original Glide AV. Sequence 56 captured only pre-C3 and
remained in `await-post-c3`; its pre EIP was `0xEB53DDDD` and its site source had become
`0xDDDDDDDD`, while the physical stack still held the valid callee cache target and guest
return.

Static comparison confirms a use-after-reallocation. The indirect adapter retains a pointer
to an element of `dbt_indirect_dispatch_sites`, then calls `HandleAotIndirectTransfer`.
Translating sequence 56's guest target dynamically appends to the same placement vector and
can reallocate it. The adapter subsequently reads `success_cache_offset` through the stale
pointer to construct the thunk continuation. The probe independently exposed the same stale
read. The RET host adapter has the same lifetime pattern. Task 286 must snapshot each
dispatch site by value before entering a resolver; no physical CALL/RET semantic change is
needed.
