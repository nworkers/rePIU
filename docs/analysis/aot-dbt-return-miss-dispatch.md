# AOT-DBT return miss host dispatch 분석 / AOT-DBT return-miss host dispatch

## 한국어

### 확인됨

Task 277은 `aot-dbt`의 translated `C3`/`C2 iw` return inline-cache miss를
정상 host-stack call로 처리합니다. 기존 `aot-dynamic`과 다른 backend의 miss tail은
계속 `popfd; INT3`입니다.

guest stack에서 C++를 실행하지 않기 위해 naked x86 thunk가 register/EFLAGS를 먼저
저장하고, entry trampoline이 기록한 host ESP와 TEB stack base/limit로 전환합니다.
기존 `HandleAotReturnTransfer`가 target resolution, dynamic append, return telemetry와
serialized inline-cache patch를 그대로 수행합니다. 성공 시 stack continuation의
`ret imm16`이 원본 return pop과 cache 이동을 재현합니다. 실패 시 `LEA ESP`가 DBT
metadata만 제거하고 provenance `INT3`가 기존 VEH 경로를 실행합니다.

최초 실제 실행은 시도/성공/fallback `39,296/0/39,296`을 기록했습니다. guest 상태는
손상되지 않았지만, 기존 return handler가 요구하는 `aot_reentry_pending` 진입 계약을
정상-call adapter가 설정하지 않아 모두 fail-closed한 것이 원인이었습니다. 이 상태를
handler 호출 전에 설정한 뒤 성공 경로가 활성화됐습니다.

최종 15초 `pumpit1` 실행은 다음을 기록했습니다.

| 지표 | `aot-dynamic` | `aot-dbt` |
|---|---:|---:|
| DBT return 시도/성공/fallback | 0/0/0 | 5,507/849/4,658 |
| AOT boundary/re-entry | 17,781/17,816 | 17,662/18,546 |
| progress | 12,745 | 13,251 |
| fatal / legacy fallback | 0 / 0 | 0 / 0 |

두 실행은 단일 표본이고 초기화 timing이 다르므로 progress 차이를 성능 향상으로
해석하지 않습니다. 확인 가능한 국소 효과는 성공 849회가 각각 return miss의
`INT3`/VEH 왕복 하나를 제거했다는 것입니다. 두 격리 EEPROM은 원본 SHA-256
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`을 유지했습니다.

### 미확정

- fallback 4,658회의 target 분류(quarantine, HLE target, dynamic miss 등)
- return host dispatch의 안정된 hot-phase wall-clock 효과
- 같은 host-stack ABI를 indirect call/jump miss에 일반화할 때 필요한 operand capture

## English

Task 277 routes translated `C3`/`C2 iw` inline-cache misses through a normal
host-stack call only under `aot-dbt`; all existing backends retain `popfd; INT3`.
A naked x86 thunk saves registers/EFLAGS, switches to the entry trampoline's
saved host ESP and TEB bounds, and reuses `HandleAotReturnTransfer` for target
resolution, dynamic append, telemetry, and serialized patching. Success uses a
`ret imm16` continuation to preserve the original stack effect; failure removes
only DBT metadata with `LEA ESP` and reaches the established provenance `INT3`.

The first live run safely failed closed for all 39,296 attempts because the
normal-call adapter had not established the return handler's existing
`aot_reentry_pending` entry contract. Setting that state before the shared
handler enabled the success path.

The final controlled 15-second runs recorded 5,507/849/4,658 DBT return
attempts/successes/fallbacks for `aot-dbt` and 0/0/0 for `aot-dynamic`, with zero
fatal state, zero legacy fallback, and unchanged isolated EEPROM hashes. Timing
differences make the progress totals unsuitable as a performance claim; the
local confirmed effect is one avoided `INT3`/VEH round trip per 849 successes.
Fallback classification, stable hot-phase wall-clock impact, and generalization
to indirect call/jump operand capture remain open.
