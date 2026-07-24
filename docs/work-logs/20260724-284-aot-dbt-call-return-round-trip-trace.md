# 20260724-284 작업 로그: AOT-DBT CALL/RET 왕복 결정적 trace

## 한국어

### 목표와 결과

Task 283에서 CALL-only로 이분된 indirect host-dispatch 크래시를 추측 수정 없이
국소화하기 위해 dispatcher-visible CALL/RET 왕복을 계측했습니다. 결과는 **공용
resolver 관측 경계 내 발산 없음**입니다. calls-only 크래시 전에 관측된 30개 CALL
tuple과 양쪽 실행에서 공통으로 상관된 26개 RET tuple은 control과 전부 일치했습니다.
다음 관측점은 C++ resolver를 건너뛰는 inline-cache hit와 host CALL의 물리적 `C3`
continuation입니다.

### 구현

- Win32 전용 `aot_dbt_call_return_trace.{h,cpp}`를 추가했습니다.
  - 고정 256-event ring과 누적 카운터만 사용합니다.
  - resolver hot path에서 allocation, mutex, 파일 I/O와 형식화 로그를 사용하지
    않습니다.
  - `REPIU_AOT_DBT_CALL_TRACE=1`일 때만 기록합니다.
- 공용 indirect/return handler에 기본 `VEH`, DBT adapter에 명시적 `host` origin을
  전달했습니다.
- 기존 진단용 `AotCallFrame`에 CALL sequence, entry ESP와 origin을 추가했습니다.
- CALL event는 source/target/return/entry ESP를 기록합니다. dispatcher-visible RET
  전체는 누적하지만, ring에는 actual target이 기록된 CALL의 return address와 일치해
  sequence를 확정할 수 있는 RET만 보존합니다.
- target이 상관된 RET의 actual ESP와 `call_entry_esp - 4`를 비교하고, 첫 불일치를
  sticky slot에 보존합니다.
- guest thread 종료 뒤 live state를 `Win32MinimalExecutionAttempt`에 POD로 복사하고,
  loader 최종 로그에서 summary, sticky divergence와 시간순 event를 출력합니다.
- disabled/origin/match/ESP mismatch/unrelated RET filtering/ring wrap을 검증하는
  `dbt_call_return_trace_probe`를 추가했습니다.
- `scripts/task284_call_return_trace.ps1`를 추가했습니다. supervisor가
  `REPIU_EXECUTION_TIMEOUT_MS=0`으로 덮어 final attempt를 회수하지 못하므로, 이
  스크립트는 동일 loader를 직접 실행하고 loader 자체 timeout을 사용합니다.

### 실구동 중 보정

초기 240초 실행에서는 dispatcher-visible RET가 1만 회를 넘어 모든 CALL을 256칸
ring 밖으로 밀어냈습니다. 전체 RET 누적 카운터는 유지하면서 ring 보존 대상을 CALL과
target-correlated RET로 제한했습니다. target이 다른 RET은 앞선 inline-cache-hit
복귀와 같은 stack depth 재사용을 구분할 수 없어 강제 연계하지 않습니다. 이 보정 뒤
`stored = calls + matches + mismatches`가 성립하고 overwrite 없이 결정적 CALL 증거를
회수했습니다.

### 검증

- VS Win32 x86 Debug 전체 빌드 성공:
  `cmake --build build/win32_x86_debug --config Debug`
- `repiu_aot_probe MASTER\PIU_1ST\PIU.EXE` 전체 통과:
  - `dbt_return_fallback_all=true`
  - `dbt_indirect_dispatch_all=true`
  - `dbt_call_return_trace=true`
  - `coherence_all=true`
- 기존 소스와 Zydis에서 C4819/LNK4217 경고가 있었지만 새 오류는 없습니다.
- 30초 직접 smoke에서 trace final snapshot 회수를 확인했습니다.
- 최종 240초 결과:
  `build/task284-call-return-trace-20260724-145052/`

| 조건 | 종료 | trace `stored/call/RET-observed/match/mismatch/overwrite` | 예외 |
|---|---|---:|---|
| indirect off + trace | 240초 timeout | `69/36/14019/33/0/0` | 없음 |
| calls-only + trace | 약 49초 후 caught exception | `56/30/11684/26/0/0` | `0xC0000005`, EIP `0x1019E8D9`, EAX/EDX `0xEB52DDDD` |

두 EEPROM SHA-256은
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`으로
동일했습니다.

### 자동 비교

로그의 CALL/RET event를 파싱해 sequence별로 비교했습니다.

- control CALL 36개, calls-only CALL 30개
- 공통 첫 30개 CALL의 sequence/source/target/return/entry ESP 차이: **0**
- control 상관 RET 33개, calls-only 상관 RET 26개
- calls-only의 26개 상관 RET와 같은 sequence의 control RET 간
  source/actual target/expected tuple/actual ESP 차이: **0**
- calls-only 미상관 CALL sequence: **27, 30, 33, 56**
  - 27: `0x030F7FBF -> 0x0302DA10`, return `0x030F7FC1`
  - 30: `0x030F514F -> 0x03013840`, return `0x030F5153`
  - 33: `0x030F514F -> 0x0301E140`, return `0x030F5153`
  - 56: `0x030D913B -> 0x03086094`, return `0x030D913E`

27/30/33은 inline-cache hit로 복귀했을 수 있고 56은 크래시 시점에 아직 복귀하지 않은
호출일 수 있으므로 Task 284 경계만으로 구분하지 않습니다.

### 결론과 다음 작업

공용 `HandleAotIndirectTransfer`가 관측한 CALL 상태와 공용
`HandleAotReturnTransfer`가 관측한 target-correlated RET 상태에는 발산이 없습니다.
따라서 resolver-visible CALL tuple과 상관 RET ESP는 근인에서 배제합니다. Task 282의
CALL host-dispatch는 계속 opt-in/기본 비활성으로 유지합니다.

다음 Task 285는 Task 223 trap 기법을 제한적으로 재사용해 미상관 sequence
27/30/33/56 주변에서 emitted inline-cache hit와 물리적 `C3` 직후 상태를 관측합니다.
원본 guest 바이트나 게임 로직은 수정하지 않고, 먼저 관측 설계와 종료 조건을 고정합니다.

### 변경 파일

- `src/platform/win32/aot/aot_dbt_call_return_trace.{h,cpp}` (신규)
- `src/tools/aot_probe/dbt_call_return_trace_probe.{h,cpp}` (신규)
- `include/repiu/platform/win32/execution_trampoline.h`
- `src/platform/win32/execution/thread_context.h`
- `src/platform/win32/aot/aot_runtime_dispatch.{h,cpp}`
- `src/platform/win32/aot/aot_dbt_indirect_dispatch.cpp`
- `src/platform/win32/aot/aot_dbt_return_dispatch.cpp`
- `src/platform/win32/execution/execution_trampoline.cpp`
- `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`
- `src/host/win32/main.cpp`
- `src/tools/aot_probe/main.cpp`
- `CMakeLists.txt`
- `scripts/task284_call_return_trace.ps1` (신규)
- Task 284 설계·작업 지시, `ARCHITECTURE.md`,
  `docs/analysis/current-execution-frontier.md`, 본 작업 로그

## English

### Goal and result

Task 284 instrumented dispatcher-visible CALL/RET round trips to localize the
CALL-only indirect host-dispatch crash without a speculative fix. The result is **no
divergence inside the shared resolver observation boundary**. All 30 CALL tuples seen before
the calls-only crash and all 26 return tuples correlated in both runs matched control. The
next observation boundary is the C++-resolver-bypassing inline-cache hit and the physical
host-CALL `C3` continuation.

### Implementation

A Win32-only `aot_dbt_call_return_trace` owns a fixed 256-event ring and aggregate counters.
It performs no allocation, locking, file I/O, or formatted logging in resolver paths and is
enabled only by `REPIU_AOT_DBT_CALL_TRACE=1`. Shared indirect/return handlers receive a
default VEH or explicit host origin, while the diagnostic call frame retains CALL sequence,
entry ESP, and origin. CALL events retain source, target, return address, and entry ESP.
Every dispatcher-visible RET is counted, but only a RET whose actual target identifies a
recorded CALL is retained. Its ESP is compared with `call_entry_esp - 4`, with the first
correlated mismatch saved in a sticky slot.

After guest-thread exit, the live state is copied as POD into
`Win32MinimalExecutionAttempt` and printed in the final loader log. A new synthetic probe
covers disabled behavior, both origins, matching correlation, ESP mismatch, unrelated-RET
filtering, and ring wrap. The reproduction script runs the loader directly because the
supervisor forces the guest timeout to zero and therefore cannot recover the final attempt
on timeout.

### Verification and measured result

The complete Win32 x86 Debug build succeeded and all AOT probes passed, including
`dbt_return_fallback_all`, `dbt_indirect_dispatch_all`, `dbt_call_return_trace`, and
`coherence_all`. Existing C4819 and Zydis LNK4217 warnings remain unrelated.

The final isolated 240-second result is in
`build/task284-call-return-trace-20260724-145052/`. Indirect-off timed out cleanly with
`69/36/14019/33/0/0` stored/call/RET-observed/match/mismatch/overwrite. Calls-only
reproduced the known crash after about 49 seconds with
`56/30/11684/26/0/0`, `0xC0000005`, Glide EIP `0x1019E8D9`, and
EAX/EDX `0xEB52DDDD`. Both EEPROM copies have SHA-256
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`.

Automated comparison found zero differences across the common first 30 CALL tuples and the
26 correlated RET tuples. Calls-only sequences 27, 30, 33, and 56 had no
dispatcher-visible correlated RET. The first three may have returned through inline-cache
hits; sequence 56 may still be in flight at the crash. Task 284 cannot distinguish these
cases by design.

### Conclusion and next work

The CALL state visible to `HandleAotIndirectTransfer` and target-correlated RET state visible
to `HandleAotReturnTransfer` do not diverge, ruling out their tuple and return ESP inside
this boundary. CALL host dispatch remains opt-in and disabled by default. Task 285 will
adapt the bounded Task 223 trap technique around uncorrelated sequences 27/30/33/56 to
observe emitted inline-cache hits and state immediately after the physical `C3`, without
changing original guest bytes or gameplay logic.
