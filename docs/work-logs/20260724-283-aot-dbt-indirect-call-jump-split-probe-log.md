# 20260724-283 작업 로그: AOT-DBT indirect host dispatch call/jump 분리 통제 실험

## 한국어

### 구현

- `AotCodeCacheBuildOptions`에 `enable_dbt_indirect_dispatch_calls`,
  `enable_dbt_indirect_dispatch_jumps`(기본 `true`)를 추가했습니다. master
  `enable_dbt_indirect_miss_dispatch`와 AND로 결합합니다.
- `EmitIndirectInlineCacheSlot`을 `enable_call_dispatch`/`enable_jump_dispatch` 두
  인자로 바꾸고, site 종류(`site.is_call`)에 따라 유효 게이트를 선택하게 했습니다.
  두 게이트가 모두 켜지면(기본·probe 경로) 방출 바이트는 Task 282와 동일합니다.
- `main.cpp`의 `REPIU_AOT_DBT_INDIRECT` 파싱을 확장했습니다:
  `1`/`both`=양쪽, `call`/`calls`=call만, `jump`/`jumps`=jump만, 그 외/미설정=off.

### 검증

- VS2026 Win32 x86 Debug 전체 빌드 성공.
- `repiu_aot_probe`(PIU.EXE) 전체 통과 — `dbt_indirect_dispatch_call_layout`,
  `dbt_indirect_dispatch_jump_layout`, `dbt_indirect_dispatch_all=true` 포함. master
  플래그만으로 call/jump 양쪽 layout이 여전히 방출됨을 확인(byte-neutral).
- 격리 EEPROM `aot-dbt` 헤드리스 4회(각 30초): `=0`, `=call`, `=jump`, `=1`.
  `scripts/task283_indirect_call_jump_split.ps1`로 실행. 결과 로그는
  `build/task283-indirect-split-20260724-095121/`와 `build/task283-both-check/`.

### 실측 1차: 30초 실행 (미결정)

네 조건(`=0`/`=call`/`=jump`/`=1`) 모두 `child_exit=124`로 완주하고 크래시 없음. 그러나
본 환경의 control(indirect off, pristine Task 282와 byte-identical)이 progress ~10,865에서
정체(wedge)하여 Task 282 프로파일(progress 29,782, indir boundary 8,891)에 미도달. guest는
`_GRSSTWINOPEN`/`_GRCLIPWINDOW` 직후 spin. → 크래시 단계 미도달로 **미결정**.

### 실측 2차: 240초 실행 (이분 완료·결정적)

240초 실행은 30초 wedge를 넘어 indirect-heavy 단계(indir boundary 3만+)에 도달했습니다.
로그: `build/task283-indirect-split-20260724-113813/`.

| 조건 | 결과 | glide milestone | last_eip / guest_eip | indir boundary |
|---|---|---|---|---:|
| `=0` (control) | ✅ 240초 완주 | 1/1/1/1/1 | — | 35,363 |
| `=call` (calls만) | 💥 크래시 @~36초 | 1/1/0/0/0 | 0x1019b7b9 / 0x30f1dd7 | 0 |
| `=jump` (jumps만) | ✅ 240초 완주 | 1/1/1/1/1 | — | 33,935 |

`=call`: `exception=0xc0000005`, Glide DLL 0x101xxxxx, guest_eip 0x30f1dd7 — Task 282
시그니처와 정확히 일치. `=jump`·`=0`는 texture/draw/swap까지 완주.

### 결론 (결정적)

- **근인은 CALL 경로 host-dispatch 전용.** JUMP host-dispatch는 240초·33,935회 무크래시로
  안전 입증.
- 확증 후보: **CALL 경로 guest-stack 반환주소 write**(`HandleAotIndirectTransfer`의
  `WriteGuestUInt32([Esp-4], return_addr)` + `Esp -= 4`). JUMP·RET 경로에는 없음.
- 30초 wedge는 진행 속도/타이밍 문제로 크래시와 독립. 재현에는 240초 실행 필요.

### 실측 3차: guest-stack write 억제 국소 실험 (negative)

CALL 경로 guest-stack 반환주소 write를 host-dispatch 성공 경로에서 억제
(`HandleAotIndirectTransfer`에 `suppress_call_stack_write` 파라미터 추가, adapter가 `true`
전달)하고 calls-only 240초 재실행. 결과는 **동일 크래시**(`0xc0000005`, EIP 0x1019b7b9,
guest_eip 0x30f1dd7, milestone 1/1/0/0/0). 로그: `build/task283-suppress-callwrite/`.
→ redundant write는 **근인 아님**. 실험 코드는 revert.

이로써 CALL 전용 상태 차이(guest-stack write, Esp 감소, shadow 스택, 반환주소 값)를 모두
배제. 남는 것은 물리적 왕복 의미(`C3`가 esp를 4 낮추고 반환주소를 남김)뿐이며 이는 VEH-call과
증명상 동일. 정적/저비용 실험 소진.

다음: 결정적 관측 — 모든 host-dispatch CALL의 `(source,target,return_addr,esp)`와 이를
소비하는 RET를 로깅해 크래시 직전 첫 발산 지점을 격리하거나, trap 백엔드 단일스텝으로
host-dispatch 직후 상태를 비교. JUMP-only host-dispatch는 안전한 부분 활성화 옵션으로 유지.

### 변경 파일

- `include/repiu/runtime/aot_code_cache.h`
- `src/runtime/aot_code_cache.cpp`
- `src/host/win32/main.cpp`
- `scripts/task283_indirect_call_jump_split.ps1` (신규)
- `docs/work-orders/20260724-283-...md`, 본 로그, `docs/analysis/current-execution-frontier.md`

## English

Added a byte-neutral calls-only / jumps-only gate to bisect the Task 282 indirect
host-dispatch crash by instruction kind: two per-kind options AND-combined with the master
flag, `EmitIndirectInlineCacheSlot` gated by `site.is_call`, and `REPIU_AOT_DBT_INDIRECT`
extended to accept `1`/`both`/`call`/`jump`. The Win32 Debug build and every probe pass,
including `dbt_indirect_dispatch_all`, and the master flag alone still emits both layouts
byte-for-byte.

A 30 s isolated-EEPROM headless A/B over `=0`/`=call`/`=jump`/`=1`
(`scripts/task283_indirect_call_jump_split.ps1`) finished gracefully in all four with no
crash and an unchanged EEPROM, but it was inconclusive: even the indirect-off control wedged
at progress ~10,865 immediately after `_GRSSTWINOPEN`/`_GRCLIPWINDOW` and did not reach the
indirect-heavy phase.

A second isolated A/B extended the observation window to 240 seconds and cleared that
timing-dependent wedge. The indirect-off control completed all Glide milestones with 35,363
VEH indirect boundaries. Calls-only crashed at about 36 seconds with the exact Task 282
signature (`0xc0000005`, Glide EIP `0x1019b7b9`, guest EIP `0x30f1dd7`, milestone
1/1/0/0/0). Jumps-only completed all 240 seconds and all milestones across 33,935 indirect
transfers. This decisively isolates the regression to CALL host dispatch and proves the JUMP
success path safe for this workload.

A final localizing experiment suppressed the CALL path's redundant guest-stack
return-address write while retaining the physical slot/`C3` continuation semantics. The
calls-only run crashed identically, ruling out the write itself. The adapter's discarded ESP
decrement, the telemetry-only shadow stack, and the return-address value are also excluded.
The experiment code was reverted.

The remaining difference is the physical CALL/RET round trip: `C3` leaves ESP four bytes
lower with a return address for a later RET, even though its architectural result is
provably identical to the VEH call path. Static analysis and low-cost perturbations are
exhausted. Next, record each host-dispatched CALL tuple
`(source,target,return_addr,esp)` and correlate it with the RET that consumes the frame to
isolate the first divergence before the crash; use trap-backend single-step comparison if
that bounded trace remains insufficient. JUMP-only dispatch remains a safe partial option.
