# 20260803-410 VEH 종료 지점 귀속 작업 로그 / VEH Exit Site Attribution Work Log

설계: [20260803-410](../design/20260803-410-veh-exit-site-attribution.md)

작업 지시: [20260803-410](../work-orders/20260803-410-veh-exit-site-attribution.md)

## 한국어

### 작업 요약

frontier 항목 2(`0x0301F7CE`의 single-step을 소비하는 곳)를 코드 읽기로 좁힌 결과
**답이 아니라 모순**이 나왔습니다. 그래서 이번 작업은 답을 주장하지 않고, 그 모순을
한 번에 가르는 계측을 넣었습니다: **VEH가 예외를 재개시키는 지점마다 이름을 붙이고,
다음 예외가 그 이름을 읽습니다.**

### 1. 코드 읽기가 만든 네 진술

Task 408이 확보한 디스어셈블리를 기준으로 합니다.

```
0x0301F7CB  fa                    cli
0x0301F7CC  31 d2                 xor  edx,edx
0x0301F7CE  83 ba 98 ec 34 00 00  cmp  dword [edx+0x34EC98],0   <- prev-eip
```

**(a) TF는 `CLI` 예외를 재개할 때 이미 켜져 있었어야 합니다.**
`HandlePrivilegedTrapInstruction`은 `0xFA`에서 IF만 지우고 `Eip`를 1 전진시킵니다
(`execution_trampoline.cpp:1483~1489`). TF는 건드리지 않습니다. `0x0301F7CE`에서
트랩이 났다는 것은 그 직전 명령 `xor edx,edx`(`0x0301F7CC`)가 TF 아래 실행됐다는
뜻이므로, TF는 `CLI`의 `0xC0000096`을 처리하고 돌아갈 때 켜져 있었습니다.

**(b) 그 시점에 TF를 켤 수 있는 곳은 전부 `enable_single_step_trace`를 켠 채 둡니다.**

| TF를 켜는 곳 | 함께 서는 상태 |
|---|---|
| `execution_trampoline.cpp:3251` | 진입 조건이 이미 trace = true |
| `aot_runtime_dispatch.cpp:1793` (경계) | reentry·trace = true |
| `aot_runtime_dispatch.cpp:1886/1907` (격리) | reentry·trace = true |
| `aot_dbt_hle_dispatch.cpp:227` (slot target-miss) | reentry·trace = true |
| `aot_dbt_glide_gate_dispatch.cpp:118` (gate target-miss) | reentry·trace = true |
| `aot_runtime_dispatch.cpp:723` (guest code write fault) | 상태 변화 없으나 **직전 명령이 store여야 함** |

`31 d2`는 store가 아니므로 마지막 줄은 배제됩니다.

**(c) 그런데 관측된 flags는 `0x00`이고 bit 4가 trace입니다**
(`port_io_emulator.cpp:499~504`). Task 408의 4회와 Task 409의 3회 모두 `0x00`입니다.

**(d) trace를 끄면서 arena에 남기는 경로가 없습니다.** `enable_single_step_trace =
false`를 쓰는 9곳이 **전부 같은 문단에서 `Eip`를 캐시 주소로 옮깁니다.**

**(e) 그리고 `0x0301F7CE`의 명령에는 처리기가 없습니다.** ModRM `0xBA` → reg 필드
`7` → `cmp r/m32, imm8`입니다. `0x83`을 받는 처리기는
`HandleTracedMemoryAddInstruction`(`/0`)과 `HandleTracedMemoryOrInstruction`(`/1`)
뿐입니다. trace가 꺼진 채라면 HLE 사슬을 전부 통과해
`execution_trampoline.cpp:3652`의 `RecoverToHost`(실행 종료)에 도달해야 하는데
실행은 계속됩니다.

**네 진술은 동시에 참일 수 없습니다.** 따라서 (1) 코드 읽기가 놓친 종료 경로가 있거나,
(2) flags가 가정한 시점의 상태가 아니거나, (3) 기록된 직전 예외가 슬롯이 말하는
것과 다릅니다.

### 2. 그래서 무엇을 넣었는가

세 갈래는 "누가 그 예외를 처리했는가"를 기록하면 한 번에 갈립니다. 추측을 하나 더
쌓는 대신 계측으로 넘겼습니다.

* `VehExitSite` 열거형(38종)과 `VehExitSiteName` —
  `include/repiu/platform/win32/veh_exit_site.h`. `kUnknown = 0`이라 **태그되지 않은
  경로는 스스로를 신고합니다.**
* `VehExitRecorder` — VEH의 choke point에서 생성하는 RAII 객체. **`AotHleTranslationScope`
  보다 먼저 생성**되므로 소멸은 나중이고, 그래서 그 scope가 `Eip`를 다시 쓴 **뒤의
  최종 재개 주소**를 기록합니다.
* `last_veh_exit_*` / `prev_veh_exit_*` — 기존 `last_veh_*`/`prev_veh_*`와 같은 회전
  규약. 매 예외마다 `kUnknown`으로 초기화합니다.
* **arena EIP single-step 종료 지점 히스토그램** — Task 409의 교훈(첫 표본은 모집단이
  아니다)을 그대로 적용해 모집단 전체를 셉니다. 총수를 따로 두어 **`합 == 총수`**를
  검산할 수 있게 했습니다.
* port I/O 주소별 진입 표본에 `entry_previous_exit_site`/`entry_previous_exit_eip`
  추가.

### 3. 판정 기준 (사전 등록, 설계 §5)

| 관측 | 결론 |
|---|---|
| 지배 종료 지점 하나 | 사슬의 시작점 확정 → frontier 항목 3으로 |
| `kUnknown` 지배 | 코드 읽기가 놓친 경로. 거기부터 다시 |
| `exit-eip` == `0x0301F7CE` | 소비자가 EIP를 전진시키지 않음 → 폐기 계열 |
| `exit-eip` == `0x0301F7D5` | 무언가 `cmp`를 emulate함 → §1(e) 반증 |
| `exit-eip`가 캐시 주소 | 캐시로 복귀 → §1 전제 반증 |
| 합 ≠ 총수 | 이 계측으로는 판정 불가로 기록 |

### 4. 검증

* **Release 빌드 성공** (`repiu_loader_win32`, `repiu_aot_probe`, 오류 0).
  첫 시도는 `live_telemetry_snapshot.cpp`의 `static_assert`가 스냅샷 구조체 이름을
  `Win32ExecutionAttempt`로 잘못 적어 실패했고(실제는 `Win32MinimalExecutionAttempt`),
  고친 뒤 통과했습니다.
* **probe 회귀 없음 — 변경 전 바이너리와 종료 코드가 같습니다.**

  | 대상 | 변경 전(`build/Release`) | 변경 후 |
  |---|---:|---:|
  | pumpit1 `PIU.EXE` | 0 | **0** |
  | pumpit3 `PIU.EXE` | 1 | **1** |

  pumpit3의 `1`은 `direct control-flow target is outside the cache`로, Task 395가
  다룬 **기존 조건이며 이번 변경과 무관**합니다. 같은 값이 양쪽에서 나온 것이 근거입니다.
* 동작 불변: 태그는 대입 한 번, 소멸자는 store 3~4회입니다. 분기·EIP·EFlags를
  바꾸는 코드는 없습니다. `REPIU_PORT_IO_CENSUS_MAPPING`과 달리 lookup을 추가하지
  않으므로 **이 빌드의 wall·프레임은 인용 가능합니다**(같은 세션 안에서만).
* 실행 측 확인(pumpit3/pumpit1 45초, 검산, 회귀 없음)은 **아직 하지 않았습니다.**
  5절이 그 자리입니다.

### 5. 실행 결과 — pumpit3 45초 8회 (같은 빌드·같은 세션)

`REPIU_EXECUTION_BACKEND=aot-dbt`, `REPIU_EXECUTION_TIMEOUT_MS=45000`,
`REPIU_EXECUTION_TIME_PROFILE=1`, EEPROM 실행별 격리.
`REPIU_PORT_IO_CENSUS_MAPPING`은 **끔**(그래서 wall·프레임 인용 가능).

| run | 격리 | 프레임 | arena single-step 총수 |
|---|---:|---:|---:|
| 4 | **0** | 1,362 | 12,133 |
| 5 | **0** | 1,394 | 12,901 |
| 8 | **0** | 1,402 | 13,094 |
| 2 | 1 | 867 | 2,286,195 |
| 3 | 1 | 867 | 2,344,851 |
| 6 | 1 | — | 4,953,866 |
| 7 | 1 | — | 4,974,756 |

run 1은 arena base가 `0x07000000`으로 잡혀(`VirtualAlloc MEM_RESERVE failed with
error 487` 후 fallback) 부팅 단계에서 죽었습니다 — **frontier 항목 9의 재현**입니다.
게스트 주소가 `0x070D0A1A`/`0x0701F516`으로 정상 실행의 `0x030D0A1A`/`0x0301F5xx`와
정확히 **+0x04000000**이라, 두 base의 census는 오프셋만 다르고 같은 코드입니다.

**검산 `합 == 총수`가 8회 전부 성립했습니다.** 계측을 신뢰할 근거입니다.

#### 5.1 답 — 소비 지점은 `HandleAotReentry`의 resolve 성공 분기입니다

격리 없는 3회가 **완전히 일치**합니다.

| run | arena single-step | `aot-reentry-resolved` | 비율 |
|---|---:|---:|---:|
| 4 | 12,133 | 12,133 | **100%** |
| 5 | 12,901 | 12,901 | **100%** |
| 8 | 13,094 | 13,094 | **100%** |

**모집단 전체가 한 지점입니다.** 따라서 이 모드에서는 첫 표본이 곧 모집단이며,
Task 409가 요구한 검증을 통과합니다.

`0x0301DB22`의 진입 표본도 3회 동일합니다.

```
#1 guest/count/cache/arena: 0x0301DB22 / 1,729,088 / 0 / 1,729,088
   entry count/prev-code/prev-eip/flags: 3,600 / 0x80000004 / 0x0301F7CE / 0x00000000
   entry prev step/bp/av/other:          3,600 / 0 / 0 / 0
   entry prev exit-site/exit-eip:        aot-reentry-resolved / 0x0C403877
```

(run 5는 3,677, run 8은 3,801이고 나머지는 같습니다. run 8만 캐시 배치가 달라
`0x0C903877`입니다.) 격리 실행 4회도 `0x0301DB22`에 대해서는 **같은 값**입니다
(진입 563,064~1,219,930, 전부 single-step 직전, 같은 exit-site와 `0x0C403877`).

#### 5.2 전제가 반증됐습니다 — 소비자는 arena에 남기지 않습니다

`exit-eip`가 **`0x0C403877`, 즉 AOT 캐시 주소**입니다(캐시 범위 `0x0A000000`~
`0x0E000000`). 설계 §5의 사전 등록 판정 그대로입니다:

> `exit-eip`가 캐시 주소 → 캐시로 복귀 → §2 전제 반증.

`aot_runtime_dispatch.cpp:1893~1902`가 `Eip = cache_address`, TF 해제,
reentry·legacy·trace 해제를 **한 문단에서** 합니다. 관측된 `flags = 0x00`은 바로 그
직후 상태이고, EIP는 arena가 아니라 캐시입니다.

**따라서 Task 408 §2의 서술("그 예외 처리가 TF를 끄고 arena에 그대로 재개하며")은
틀렸습니다.** 정정합니다. 동시에 설계 §3의 모순도 해소됩니다 — (d)가 옳았고,
"소비자가 arena에 남긴다"는 가정이 틀렸을 뿐입니다.

또한 `0x0C403877`은 **게스트 `0x0301F7CE`의 캐시 번역본**입니다(그 주소를 resolve한
결과이므로). 즉 실행은 타이머 핸들러의 그 지점부터 **캐시에서** 이어집니다.

#### 5.3 질문이 이동했습니다 — 이탈은 예외 없이 일어납니다

복귀 지점은 캐시인데 **바로 다음 예외**는 `0x0301DB22`를 **arena에서** 실행하다 난
port I/O fault입니다(`cache` = 0, 8회 전부). 그 사이에 **VEH 예외가 하나도 없습니다**
— 진입 판정 자체가 "직전 예외가 `0xC0000096`이 아님"이고, 직전 예외는 그 single-step
이었습니다.

**그러므로 캐시 → arena 이탈은 예외 없는 경로로 일어납니다.** 진입:count가 1:480
(3,600 : 1,729,088)이므로 한 번 나가면 오래 머뭅니다.

#### 5.4 두 모드의 차이가 수치로 갈렸습니다 (frontier 항목 1에 기여)

격리 없는 실행은 arena single-step이 12,133~13,094인데 격리 실행은 2,286,195~
4,974,756으로 **180~410배**입니다. 종료 지점 분포도 정반대입니다.

| 종료 지점 | 격리 없음(run 4) | 격리(run 6) |
|---|---:|---:|
| `aot-reentry-resolved` | **12,133 (100%)** | 9,953 (0.20%) |
| `step-trace-stepped` | 0 | 3,719,710 (75.1%) |
| `step-trace-hle-stepped` | 0 | 1,214,507 (24.5%) |
| `step-trace-timer-injected` | 0 | 9,696 (0.20%) |

격리 실행에서는 `HandleSingleStepTrace`의 스텝 재무장 경로 둘이 99.6%이며, 이들은
TF를 **켠 채** arena에 남깁니다 — Task 404가 본 격리 시 single-step 폭증의 정체입니다.
`aot-reentry-resolved`의 **절대수는 두 모드가 비슷합니다**(12,133 대 9,953). 즉 격리는
정상 경로를 없애는 것이 아니라 그 위에 스텝 실행을 얹습니다.

프레임도 같은 방향입니다: 격리 없음 1,362~1,402, 격리 867(또는 렌더 루프 미도달).

#### 5.5 주소마다 기전이 다르다는 것도 유지됩니다

| 주소 | 진입 | 직전 예외 | exit-site |
|---|---:|---|---|
| `0x0301DB22` | 3,600 | single-step `0x0301F7CE` | `aot-reentry-resolved` |
| `0x030D0A1A` | 10,404 (= count) | bp 9,139 / av 1,265 | `step-trace-stepped` |
| `0x0301F851` | 671 | av 655 / bp 9 / step 7 | `aot-reentry-resolved` |
| `0x0301EDC6` | 7 | bp 7 | `aot-reentry-retired-resolved` |

Task 408·409의 "주소마다 기전이 다르다"가 종료 지점 축에서도 성립합니다.

### 6. 후보 셋을 같은 로그로 배제했습니다 (추가 실행 없음)

"캐시에서 arena로 나가는 예외 없는 경로"의 후보 셋을 세웠는데, **같은 8회 로그가
셋 다 배제**했습니다.

| 후보 | 근거(격리 없는 3회) | 판정 |
|---|---|---|
| AOT-DBT HLE slot target-miss bridge (`aot_dbt_hle_dispatch.cpp:226~231`) | `AOT-DBT HLE host fallback reason .../target/...` = **0/0/0** (fallback 14·8·12는 전부 `unhandled`) | 배제 |
| Glide gate target-miss bridge (`aot_dbt_glide_gate_dispatch.cpp:118~121`) | `Glide direct dispatch ... target-miss` = **0/0/0** (entry 779,013~838,649 전부 success) | 배제 |
| 미해결 direct edge (사이트 10개) | `ResolveAotDbtDirectEdgeFrame`은 성공 시 `cache_target`, 실패 시 `fallback_cache_offset` — **두 경로 다 캐시 주소** | 배제 |

**세 번째는 특히 유력하다고 적었으나 코드가 반증했습니다.** 사이트가 10개 존재하는
것과 그 dispatch가 arena로 나가는 것은 별개였습니다.

### 6.1 배제가 지목한 새 가설 — 진입이 이탈이 아닐 수 있습니다

`HandlePortIoInstruction`은 주석대로 **VEH 밖 AOT fast-path thunk에서도 호출**되며
(`port_io_emulator.cpp:440~442`), 그때 `win32_context`는 합성 게스트 컨텍스트라
`Eip`가 **게스트 주소**입니다. 그러면 `from_aot_cache`가 false가 되어 **실제로는 캐시
실행인데도 "arena"로 기록**됩니다.

수치가 호환됩니다: run 4의 예외 없는 HLE slot dispatch는 **16,599회**이고
`0x0301DB22`의 진입은 **3,600회**입니다(3,600 < 16,599). Task 405가 인용한
`outside-veh` 카운터는 **현재 빌드에서 출력되지 않습니다.**

**이것은 가설이며 측정하지 않았습니다.** 다만 port I/O 총 1,772,285회 중 thunk 경유는
많아야 16,599회이고 예외 census의 `other`가 1,772,980으로 총수와 맞으므로,
**대부분의 port I/O가 진짜 VEH fault인 것은 확실합니다.** 가설이 겨냥하는 것은 총수가
아니라 **진입 3,600건의 분류**입니다.

### 6.2 다음 대상

1. **port I/O census에 호출 측(VEH / thunk) 태그.** 판정: 진입이 thunk 측이면 Task 405
   확인 1a("arena에서 실행된다")가 계측 아티팩트이고, VEH 측이면 진짜 이탈이므로 캐시
   블록이 방출한 것을 직접 읽습니다. 비용은 bool 하나입니다.
2. frontier 항목 3(arena→캐시 복귀)은 1번 뒤입니다. 복귀 자체는 이미 있고 정상 동작
   중이므로(100% resolve 성공), 필요한 것은 복귀가 아니라 **이탈을 막는 것**입니다.
3. 항목 9(부팅 크래시)는 8회 중 1회 재현했고 base가 `0x07000000`일 때였습니다.

### 7. 미확정

* 캐시에서 arena로 나가는 예외 없는 경로의 정체(위 6-1).
* 격리 발생 조건 — 8회 중 4회 발생으로 여전히 비결정적입니다.
* 재번역이 요청 진입 주소를 address map에 남기지 못하는 조건(Task 404 이월).
* `0x0301F7CE`에서 왜 single-step이 나는지(TF를 켠 주체). 5.3의 예외 없는 이탈과 같은
  기전일 가능성이 있으나 확인하지 않았습니다.

---

## English

### Summary

Reading the code for frontier item 2 — which site consumes the single step at
`0x0301F7CE` — produced **a contradiction rather than an answer**. This task therefore
claims no answer and instead adds the instrument that separates the possibilities:
**every point at which the VEH resumes the guest is named, and the next exception reads
that name.**

### 1. The four statements reading produced

From Task 408's disassembly — `cli` at `0x0301F7CB`, `xor edx,edx` at `0x0301F7CC`, and
`cmp dword [edx+0x34EC98],0` at `0x0301F7CE`:

**(a) The trap flag must already have been set when the `CLI` exception resumed.**
`HandlePrivilegedTrapInstruction` clears IF and advances `Eip` by one
(`execution_trampoline.cpp:1483-1489`) and never touches TF, so a trap at `0x0301F7CE`
means `xor edx,edx` ran under the flag.

**(b) Every site that can set it there leaves `enable_single_step_trace` on** — line
3251 (which requires trace already), the boundary path, both quarantine branches, and
the two exception-free target-miss bridges. The one site that sets TF without touching
that state, the guest-code write fault at `aot_runtime_dispatch.cpp:723`, needs the
preceding instruction to be a store, and `31 d2` is not one.

**(c) The recorded flags are `0x00`**, whose bit 4 is trace
(`port_io_emulator.cpp:499-504`), in all four Task 408 runs and all three Task 409 runs.

**(d) No path clears trace while leaving execution in the arena** — all nine sites that
write `enable_single_step_trace = false` move `Eip` to a cache address in the same
paragraph.

**(e) And nothing handles the instruction at `0x0301F7CE`.** ModRM `0xBA` gives reg
field 7, `cmp r/m32, imm8`, while the only `0x83` handlers cover `/0` and `/1`. With
trace off, that step would fall through the whole chain into the `RecoverToHost` exit at
`execution_trampoline.cpp:3652`, and the runs continue.

**These cannot all hold**, so either reading missed an exit path, the flags do not
describe the assumed moment, or the recorded predecessor is not what the slot says.

### 2. What was added

All three separate the moment "who consumed this exception" is recorded, so the work
stops at measurement rather than adding a fourth hypothesis.

A 38-value `VehExitSite` enumeration and its name function live in
`include/repiu/platform/win32/veh_exit_site.h`, with `kUnknown = 0` so an untagged path
reports itself. A `VehExitRecorder` RAII object at the VEH choke point — constructed
*before* `AotHleTranslationScope` and therefore destroyed after it — records the final
resume EIP and state. The `last_veh_exit_*`/`prev_veh_exit_*` pair follows the existing
rotation convention and resets to `kUnknown` per exception. Task 409's lesson is applied
directly: a **histogram over every single step taken at an arena EIP** counts exit sites
with the population total kept beside it, so `sum == total` is checkable rather than
assumed. The per-address port I/O entry sample gains the predecessor's exit site and
exit EIP.

### 3. Pre-registered decision rules

A single dominant site names the head of the chain and unblocks frontier item 3; a
dominant `kUnknown` names the path reading missed; an exit EIP equal to `0x0301F7CE`
means the consumer did not advance EIP, `0x0301F7D5` would refute §1(e), and a cache
address would refute §1's premise. A failed sum check is reported as "this instrument
cannot decide", not as a result.

### 4. Verification

**The Release build passes** for `repiu_loader_win32` and `repiu_aot_probe` with zero
errors; the first attempt failed only because a `static_assert` named the snapshot
struct `Win32ExecutionAttempt` instead of `Win32MinimalExecutionAttempt`.
**The probe shows no regression** — exit codes match the pre-change binary exactly:
pumpit1 `PIU.EXE` returns 0 before and after, and pumpit3 `PIU.EXE` returns 1 before and
after, that 1 being the pre-existing `direct control-flow target is outside the cache`
condition from Task 395 rather than anything this change introduced.

Behaviour is unchanged by construction: one assignment per exit and three or four stores
in the destructor, with no branch, EIP, or EFLAGS write. Unlike
`REPIU_PORT_IO_CENSUS_MAPPING` it adds no lookup, so **wall time and frames from this
build stay quotable** within a session. The run-side checks have **not** been done;
section 5 is their place.

### 5. Run results — eight 45-second pumpit3 runs, one build, one session

Backend `aot-dbt`, 45-second timeout, time profile on, EEPROM isolated per run, and
`REPIU_PORT_IO_CENSUS_MAPPING` **off**, so wall time and frames stay quotable.

| Run | Quarantines | Frames | Arena single steps |
|---|---:|---:|---:|
| 4 | **0** | 1,362 | 12,133 |
| 5 | **0** | 1,394 | 12,901 |
| 8 | **0** | 1,402 | 13,094 |
| 2 | 1 | 867 | 2,286,195 |
| 3 | 1 | 867 | 2,344,851 |
| 6 | 1 | — | 4,953,866 |
| 7 | 1 | — | 4,974,756 |

Run 1 died at boot with the arena at `0x07000000` after `VirtualAlloc MEM_RESERVE
failed with error 487` — **frontier item 9 reproduced**. Its guest addresses sit exactly
`+0x04000000` from the usual ones (`0x070D0A1A` against `0x030D0A1A`), so the two bases
describe the same code at an offset.

**The `sum == total` check held in all eight runs**, which is what makes the rest
trustworthy.

#### 5.1 The answer: `HandleAotReentry`'s resolve-success branch

The three quarantine-free runs agree exactly: **100% of arena-EIP single steps** are
consumed by `aot-reentry-resolved` — 12,133 of 12,133, 12,901 of 12,901, and 13,094 of
13,094. The population is one site, so here the first sample *is* the population, which
is the check Task 409 demanded. The `0x0301DB22` entry sample is identical across runs:
3,600 entries (3,677 and 3,801 in the others), predecessor `0x80000004` at
`0x0301F7CE`, flags `0x00000000`, all 3,600 with a single-step predecessor, and
**exit site `aot-reentry-resolved` resuming at `0x0C403877`**. The four quarantined runs
report the same values for this address.

#### 5.2 The premise is refuted — the consumer does not leave execution in the arena

`0x0C403877` is an **AOT cache address** (the cache spans `0x0A000000`-`0x0E000000`),
which is exactly the design's pre-registered reading: *a cache address means execution
returned to the cache, refuting §2's premise*. `aot_runtime_dispatch.cpp:1893-1902` sets
`Eip` to the cache address and clears the trap flag, re-entry, legacy, and trace in one
paragraph, and the observed `flags = 0x00` is precisely that post-resolve state.

**Task 408 §2's sentence — "that handler clears the trap flag and resumes in the arena"
— is therefore wrong and is corrected here.** The design's contradiction dissolves with
it: statement (d) was right all along, and only the assumption that the consumer left
EIP in the arena was false. `0x0C403877` is the cache translation of guest
`0x0301F7CE`, so execution continues from that point in the timer handler *inside the
cache*.

#### 5.3 The question moves: the departure is exception-free

Execution resumes in the cache, yet the **very next** exception is a port I/O fault
executing `0x0301DB22` **in the arena** (`cache` is zero in all eight runs), with no VEH
exception in between — that is what "entry" means here. **So the cache-to-arena
departure happens through a path that raises no exception**, and the 1:480 ratio of
entries to reads (3,600 against 1,729,088) says that once out, execution stays out.

#### 5.4 The two modes separate numerically (bearing on item 1)

Quarantine-free runs record 12,133-13,094 arena single steps against 2,286,195-4,974,756
when quarantine fires — **180 to 410 times more** — and the distributions are opposites.
In run 4 the whole population is `aot-reentry-resolved`; in run 6, `step-trace-stepped`
holds 75.1% and `step-trace-hle-stepped` 24.5%, both of which re-arm the trap flag and
**do** leave execution stepping in the arena, which is what Task 404 saw as the
quarantine single-step explosion. The absolute count of `aot-reentry-resolved` is
similar in both modes (12,133 against 9,953), so quarantine does not remove the healthy
path — it layers stepping on top of it. Frames follow: 1,362-1,402 against 867 or no
render loop at all.

#### 5.5 Mechanisms still differ per address

`0x0301DB22` enters after a single step and `aot-reentry-resolved`; `0x030D0A1A` has as
many entries as executions after breakpoints and access violations, exiting through
`step-trace-stepped`; `0x0301F851` is access-violation dominated; `0x0301EDC6` uses
`aot-reentry-retired-resolved`. Tasks 408 and 409's per-address observation holds on the
exit-site axis too.

### 6. All three candidates are excluded by the same logs (no extra runs)

The AOT-DBT HLE slot's target-miss bridge records **zero** misses in all three
quarantine-free runs (its 14, 8, and 12 fallbacks are all `unhandled`); the Glide gate's
equivalent likewise records **zero** against 779,013-838,649 entries, all successful;
and the unresolved direct edge — the candidate called most likely because pumpit3's
probe reports `direct control-flow target is outside the cache` (Task 395) — is refuted
by its own code, since `ResolveAotDbtDirectEdgeFrame` resumes at `cache_target` on
success and `fallback_cache_offset` on failure, **both cache addresses.** Ten dispatch
sites existing and those dispatches leaving for the arena were two different claims.

### 6.1 What the exclusions point to: the entry may not be a departure

`HandlePortIoInstruction` is reachable from the AOT fast-path thunk outside the VEH
(`port_io_emulator.cpp:440-442`), where the synthetic guest context carries a **guest**
EIP — which makes `from_aot_cache` false and records an "arena" entry even when
execution is in the cache. The counts are compatible: run 4 has **16,599**
exception-free HLE slot dispatches against **3,600** entries at `0x0301DB22`, and the
`outside-veh` counter Task 405 quoted is not printed by the current build.

**This is a hypothesis and was not measured.** What is certain is that the bulk of port
I/O really is a VEH fault — 1,772,285 handled against an exception census `other` of
1,772,980, with at most 16,599 possible through the thunk. The hypothesis targets the
classification of the 3,600 entries, not the total.

### 6.2 Next

Tag the port I/O census with its **caller side** (VEH or thunk): if the entries come
from the thunk, Task 405's "it executes in the arena" is an instrument artifact; if from
the VEH, the departure is real and the emitted cache block must be read directly. The
cost is one bool. Frontier item 3 needs rewording either way — the return already exists
and works at 100% resolve success, so what is needed is preventing the departure, not a
return. Item 9 reproduced once in eight runs, at arena base `0x07000000`.

### 7. Unresolved

The identity of the exception-free cache-to-arena departure; what decides whether
quarantine fires (four of eight runs here, still nondeterministic); the condition under
which a re-translation omits its requested entry from the address map (Task 404); and
why a single step arises at `0x0301F7CE` at all — possibly the same mechanism as the
departure, but not checked.
