# 20260724-285 작업 로그: AOT-DBT CALL continuation 제한 trap 관측

## 한국어

### 구현

- `aot_dbt_call_step_probe.{h,cpp}`를 추가했습니다.
- `REPIU_AOT_DBT_CALL_STEP`에 Task 284 event sequence를 최대 8개 지정합니다.
- 선택된 host CALL만 saved EFLAGS.TF를 켜 synthetic `C3` 직전과 직후 두 `#DB`를
  기록합니다.
- post-C3에서 guest return의 active cache address와 guest address에 DR0/DR1 실행
  breakpoint를 설치하고, caller continuation 도달 시 원 debug state를 복원합니다.
- 기존 DR 사용과 native fast path 활성은 conflict로 fail-closed하며, return watch
  동안 새 native fast path 진입만 억제합니다.
- 고정 32-event trace와 active phase를 final attempt에 복사해 loader 종료 로그에서
  출력합니다.
- disabled/host-only/pre/post/return/DR 복원/conflict를 검증하는
  `dbt_call_step_probe`를 추가했습니다.
- `scripts/task285_call_step_probe.ps1`로 sequence 하나씩 격리 EEPROM 실행을
  재현합니다.

### 검증

- VS Win32 x86 Debug 전체 빌드 성공.
- 전체 AOT probe 통과:
  `dbt_indirect_dispatch_all=true`,
  `dbt_call_return_trace=true`,
  `dbt_call_step_probe=true`,
  `coherence_all=true`.
- 기존 C4819와 Zydis LNK4217 경고 외 새 오류 없음.
- 모든 실행의 EEPROM SHA-256:
  `A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`.

### sequence별 실측

| sequence | CALL | trace 결과 | 크래시 |
|---:|---|---|---|
| 27 | `0x030F7FBF -> 0x0302DA10`, ret `0x030F7FC1` | pre/post/return 3개, EIP·ESP 모두 match | 기존 `0xC0000005`, Glide `0x101A1F29` |
| 30 | `0x030F514F -> 0x03013840`, ret `0x030F5153` | pre/post/return 3개, EIP·ESP 모두 match | 동일 |
| 33 | `0x030F514F -> 0x0301E140`, ret `0x030F5153` | pre/post/return 3개, EIP·ESP 모두 match | 동일 |
| 56 | `0x030D913B -> 0x03086094`, ret `0x030D913E` | **pre만**, phase `await-post-c3`; pre EIP `0xEB53DDDD`, site source `0xDDDDDDDD` | 동일 |

로그:

- `build/task285-seq27-smoke/`
- `build/task285-call-step-seq30-20260724-171928/`
- `build/task285-call-step-seq33-20260724-172104/`
- `build/task285-call-step-seq56-20260724-172216/`

27/30/33은 물리적 `C3`가 cache target을 pop하고 ESP를 4 증가시키며, inline-cache
RET가 guest return을 pop해 CALL entry ESP로 복원하는 과정이 모두 정확했습니다.
probe가 추가한 VEH 두 번도 기존 크래시를 없애지 않았으므로 이 세 위치의 VEH 우회
부수효과는 근인이 아닙니다.

### 근인 확정

sequence 56의 pre-C3 stack 자체는 정상입니다.

- `[ESP] = 0x0D84213A`: resolved callee cache target
- `[ESP+4] = 0x030D913E`: guest return

반면 thunk가 먼저 복귀할 synthetic success continuation은 `0xEB53DDDD`로
오염됐습니다. 원인은 adapter의 dispatch-site 포인터 수명입니다.

1. `FindDispatchSite`가 placement의 `dbt_indirect_dispatch_sites` 원소 포인터를
   반환합니다.
2. adapter가 이 포인터를 유지한 채 `HandleAotIndirectTransfer`를 호출합니다.
3. sequence 56 target `0x03086094`가 아직 번역되지 않아
   `AppendWin32DynamicAotTranslation`이 실행됩니다.
4. append가 `placement->dbt_indirect_dispatch_sites.push_back(site)`를 수행해 기존
   원소 포인터를 무효화할 수 있습니다.
5. adapter가 무효 포인터의 `success_cache_offset`을 읽어
   `frame[kGuestSourceIndex]`를 계산합니다.

`0xEB53DDDD`/`0xDDDDDDDD`는 기존 크래시의 EAX/EDX garbage signature와 같은
allocator poison 계열이며, 동적 append가 실제로 포인터를 무효화했다는 직접
증거입니다. RET host adapter도 같은 방식으로 `dbt_return_dispatch_sites` 원소
포인터를 resolver 호출 뒤 재사용합니다.

### 결론과 다음 작업

Task 282 크래시의 근인은 CALL/RET ABI가 아니라 **동적 append를 가로지르는
dispatch-site use-after-reallocation**입니다. Task 286에서 resolver 호출 전에 site를
값으로 snapshot하고, indirect와 RET adapter 모두 같은 수명 규칙을 적용합니다.
그 뒤 sequence 56 step probe, calls-only probe-off 240초와 기존 전체 probe를 다시
검증합니다.

## English

Task 285 added an opt-in bounded probe that sets TF only for selected host CALLs, captures
state before and after the synthetic `C3`, and then installs DR0/DR1 execution breakpoints
at the active-cache/guest return addresses. It restores original debug state at the caller
continuation and fails closed on existing debug-register ownership. Fixed final-attempt
state and a synthetic probe were added, along with a one-sequence isolated-EEPROM script.

The complete Win32 x86 Debug build and all AOT probes passed, including
`dbt_call_step_probe=true`. Sequences 27, 30, and 33 each produced matching pre-C3,
post-C3, and return-target EIP/ESP events and then reproduced the original Glide AV. Their
physical CALL/RET transitions are correct, and adding two VEH events there does not remove
the crash.

Sequence 56 is decisive. It captured only pre-C3 and remained in `await-post-c3`. The stack
still held a valid resolved callee cache target followed by the correct guest return, but
the synthetic continuation EIP was `0xEB53DDDD` and the site source read as `0xDDDDDDDD`.
These match the allocator-poison family seen in the original crash.

The root cause is a dispatch-site use-after-reallocation. The adapter keeps a pointer into
`placement->dbt_indirect_dispatch_sites` across `HandleAotIndirectTransfer`; translating
sequence 56 dynamically appends to that same vector, potentially reallocating it. The
adapter then reads `success_cache_offset` through the stale pointer. The RET host adapter
has the same pattern for its site vector. Task 286 will snapshot site metadata by value
before entering either resolver, then re-run sequence 56, a probe-off 240-second calls-only
test, and all existing probes.
